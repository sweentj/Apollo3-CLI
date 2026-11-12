#include <stdbool.h>
#include <string.h>

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "cli_task.h"
#include "cli_line.h"
#include "FreeRTOS_CLI.h"

/* Must fit each registered help string and framework diagnostic plus NUL. */
#if configCOMMAND_INT_MAX_OUTPUT_SIZE < 256
#error "The I2C CLI requires configCOMMAND_INT_MAX_OUTPUT_SIZE >= 256"
#endif

#define CLI_RX_QUEUE_LENGTH 256u
#define CLI_UART_ERRORS (AM_HAL_UART_INT_OVER_RUN | AM_HAL_UART_INT_BREAK_ERR | \
                         AM_HAL_UART_INT_PARITY_ERR | AM_HAL_UART_INT_FRAME_ERR)
#define CLI_UART_INTERRUPTS (AM_HAL_UART_INT_RX | AM_HAL_UART_INT_RX_TMOUT | CLI_UART_ERRORS)

static void *uart;
static QueueHandle_t rx_queue;
static volatile bool rx_lost;
volatile uint32_t cli_last_hal_error;

static const am_hal_uart_config_t uart_config = {
    .ui32BaudRate = 115200,
    .ui32DataBits = AM_HAL_UART_DATA_BITS_8,
    .ui32Parity = AM_HAL_UART_PARITY_NONE,
    .ui32StopBits = AM_HAL_UART_ONE_STOP_BIT,
    .ui32FlowControl = AM_HAL_UART_FLOW_CTRL_NONE,
    .ui32FifoLevels = AM_HAL_UART_TX_FIFO_1_2 | AM_HAL_UART_RX_FIFO_1_8,
    /* HAL software queues disabled: ISR owns RX FIFO; task owns TX FIFO. */
};

static void uart_close(void)
{
    NVIC_DisableIRQ(UART0_IRQn);
    if (uart != NULL) {
        (void)am_hal_uart_interrupt_disable(uart, CLI_UART_INTERRUPTS);
        (void)am_hal_uart_power_control(uart, AM_HAL_SYSCTRL_DEEPSLEEP, false);
        (void)am_hal_uart_deinitialize(uart);
        uart = NULL;
    }
    (void)am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_TX, g_AM_HAL_GPIO_DISABLE);
    (void)am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_RX, g_AM_HAL_GPIO_DISABLE);
}

static uint32_t uart_open(void)
{
    uint32_t result;
    am_hal_uart_clock_speed_e clock = eUART_CLK_SPEED_DEFAULT;
#define UART_CHECK(call) do { result = (call); if (result != AM_HAL_STATUS_SUCCESS) return result; } while (0)
    NVIC_DisableIRQ(UART0_IRQn);
    UART_CHECK(am_hal_uart_initialize(0, &uart));
    UART_CHECK(am_hal_uart_power_control(uart, AM_HAL_SYSCTRL_WAKE, false));
    UART_CHECK(am_hal_uart_control(uart, AM_HAL_UART_CONTROL_CLKSEL, &clock));
    UART_CHECK(am_hal_uart_configure(uart, &uart_config));
    UART_CHECK(am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_TX, g_AM_BSP_GPIO_COM_UART_TX));
    UART_CHECK(am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_RX, g_AM_BSP_GPIO_COM_UART_RX));
    UART_CHECK(am_hal_uart_interrupt_clear(uart, CLI_UART_INTERRUPTS));
    UART_CHECK(am_hal_uart_interrupt_enable(uart, CLI_UART_INTERRUPTS));
#undef UART_CHECK
    /* CMSIS expects the unshifted priority. Lowest priority is RTOS-safe. */
    NVIC_SetPriority(UART0_IRQn, configKERNEL_INTERRUPT_PRIORITY >> (8u - __NVIC_PRIO_BITS));
    NVIC_ClearPendingIRQ(UART0_IRQn);
    /* The task enables NVIC only once the scheduler is running. */
    return AM_HAL_STATUS_SUCCESS;
}

cli_start_result_t cli_start(QueueHandle_t requests)
{
    if (requests == NULL) return CLI_START_INVALID_QUEUE;
    if (rx_queue != NULL) return CLI_START_ALREADY_STARTED;
    if (cli_commands_register(requests) != pdPASS) return CLI_START_NO_MEMORY;
    rx_queue = xQueueCreate(CLI_RX_QUEUE_LENGTH, sizeof(uint8_t));
    if (rx_queue == NULL) return CLI_START_NO_MEMORY;
    rx_lost = false;
    cli_last_hal_error = uart_open();
    if (cli_last_hal_error != AM_HAL_STATUS_SUCCESS) {
        uart_close();
        vQueueDelete(rx_queue);
        rx_queue = NULL;
        return CLI_START_UART_ERROR;
    }
    if (xTaskCreate(cli_task, "cli_task", CLI_TASK_STACK_WORDS, NULL,
                    CLI_TASK_PRIORITY, NULL) != pdPASS) {
        uart_close();
        vQueueDelete(rx_queue);
        rx_queue = NULL;
        return CLI_START_NO_MEMORY;
    }
    return CLI_START_OK;
}

void am_uart_isr(void)
{
    uint32_t status = 0, received = 0;
    uint8_t bytes[AM_HAL_UART_FIFO_MAX];
    BaseType_t wake = pdFALSE;
    am_hal_uart_transfer_t transfer = {
        .ui32Direction = AM_HAL_UART_READ,
        .pui8Data = bytes,
        .ui32NumBytes = sizeof(bytes),
        .ui32TimeoutMs = 0,
        .pui32BytesTransferred = &received,
    };

    if (am_hal_uart_interrupt_status_get(uart, &status, true) != AM_HAL_STATUS_SUCCESS) {
        rx_lost = true;
    }
    (void)am_hal_uart_interrupt_clear(uart, status);
    if ((status & CLI_UART_ERRORS) != 0u) rx_lost = true;
    /* Bounded, nonblocking FIFO drain; never parse or print from the ISR. */
    if (am_hal_uart_transfer(uart, &transfer) != AM_HAL_STATUS_SUCCESS) rx_lost = true;
    for (uint32_t i = 0; i < received; ++i) {
        if (xQueueSendFromISR(rx_queue, &bytes[i], &wake) != pdPASS) rx_lost = true;
    }
    portYIELD_FROM_ISR(wake);
}

static void put_text(const char *text)
{
    size_t remaining = strlen(text);
    TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(1000) + 1u;
    while (remaining != 0u) {
        uint32_t sent = 0;
        am_hal_uart_transfer_t transfer = {
            .ui32Direction = AM_HAL_UART_WRITE,
            .pui8Data = (uint8_t *)text,
            .ui32NumBytes = (uint32_t)remaining,
            .ui32TimeoutMs = 0,
            .pui32BytesTransferred = &sent,
        };
        uint32_t result = am_hal_uart_transfer(uart, &transfer);
        if (result != AM_HAL_STATUS_SUCCESS ||
            (TickType_t)(xTaskGetTickCount() - start) >= timeout) {
            cli_last_hal_error = result != AM_HAL_STATUS_SUCCESS ? result : AM_HAL_STATUS_TIMEOUT;
            NVIC_DisableIRQ(UART0_IRQn);
            /* Do not execute invisible commands after losing the console. */
            for (;;) vTaskSuspend(NULL);
        }
        text += sent;
        remaining -= sent;
        if (remaining != 0u) vTaskDelay(1);
    }
}

static void dispatch(char *line)
{
    BaseType_t more;
    char *output = FreeRTOS_CLIGetOutputBuffer();
    cli_line_normalize(line);
    if (line[0] == '\0') return;
    /* FreeRTOS+CLI owns command lookup, parameter counting and built-in help.
     * Drain every output chunk before processing another line (non-reentrant). */
    do {
        more = FreeRTOS_CLIProcessCommand(line, output, configCOMMAND_INT_MAX_OUTPUT_SIZE);
        put_text(output);
    } while (more != pdFALSE);
}

void cli_task(void *argument)
{
    cli_line_t line;
    (void)argument;
    cli_line_init(&line);
    NVIC_EnableIRQ(UART0_IRQn);
    put_text("\r\nApollo3 I2C CLI; type help\r\n> ");
    for (;;) {
        uint8_t byte;
        bool lost;
        /* Periodic wake also detects a hardware error with no received bytes. */
        BaseType_t have_byte = xQueueReceive(rx_queue, &byte, pdMS_TO_TICKS(100) + 1u);
        taskENTER_CRITICAL();
        lost = rx_lost;
        if (lost) {
            /* A missing byte makes all buffered text suspect. Reset only in
             * task context, with this RTOS-safe UART ISR masked. */
            (void)xQueueReset(rx_queue);
            rx_lost = false;
        }
        taskEXIT_CRITICAL();
        if (lost) {
            cli_line_invalidate(&line);
            put_text("\r\nERR UART input lost; press Enter and retry\r\n");
            continue;
        }
        if (have_byte != pdPASS) continue;
        switch (cli_line_feed(&line, byte)) {
        case CLI_LINE_ECHO: {
            /* Display a tab as one space so backspace erases one position. */
            char echo[2] = {byte == '\t' ? ' ' : (char)byte, '\0'};
            put_text(echo);
            break;
        }
        case CLI_LINE_ERASE:
            put_text("\b \b");
            break;
        case CLI_LINE_READY:
            put_text("\r\n");
            dispatch(line.text);
            put_text("> ");
            break;
        case CLI_LINE_REJECTED:
            put_text("\r\nERR line discarded (too long, invalid byte, or UART loss)\r\n> ");
            break;
        case CLI_LINE_CANCELLED:
            put_text("^C\r\n> ");
            break;
        default:
            break;
        }
    }
}
