#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "cli_task.h"

/* Intentionally visible to a debugger and the future I2C worker. */
QueueHandle_t i2c_request_queue;
volatile const char *cli_assert_file;
volatile unsigned int cli_assert_line;

void cli_example_assert_failed(const char *file, unsigned int line)
{
    cli_assert_file = file;
    cli_assert_line = line;
    taskDISABLE_INTERRUPTS();
    for (;;) { /* Inspect file/line in the debugger. */ }
}

void vApplicationMallocFailedHook(void)
{
    cli_example_assert_failed(__FILE__, __LINE__);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    cli_example_assert_failed(__FILE__, __LINE__);
}

int main(void)
{
    configASSERT(am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, NULL) == AM_HAL_STATUS_SUCCESS);
    configASSERT(am_hal_cachectrl_config(&am_hal_cachectrl_defaults) == AM_HAL_STATUS_SUCCESS);
    configASSERT(am_hal_cachectrl_enable() == AM_HAL_STATUS_SUCCESS);
    am_bsp_low_power_init();

    i2c_request_queue = xQueueCreate(8, sizeof(cli_i2c_request_t));
    configASSERT(i2c_request_queue != NULL);
    configASSERT(cli_start(i2c_request_queue) == CLI_START_OK);

    /* Create your I2C worker here, passing i2c_request_queue to it.
     * No consumer is implemented: the ninth queued request reports full. */
    vTaskStartScheduler();
    cli_example_assert_failed(__FILE__, __LINE__);
    return 0;
}
