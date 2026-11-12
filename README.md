# Apollo3 FreeRTOS I²C command console

UART0 command input for the Ambiq **Apollo3 EVB** (`boards/apollo3_evb`),
developed against the local AmbiqSuite R3.2.0. A dedicated `cli_task` parses
commands and copies requests into an application-owned FreeRTOS queue. The I²C
worker is intentionally not implemented.

## Build and test

Install/use GNU Make and the Arm GNU `arm-none-eabi` toolchain on `PATH`:

```sh
make -j4
make test
```

The default SDK location is:

```text
/Users/tjswe/Solinst/Software/AmbiqSuite_R3.2.0
```

Override it with `make SDK_ROOT=/path/to/AmbiqSuite_R3.2.0`. You can also set
`TOOLCHAIN=/path/to/arm-none-eabi` and `HOST_CC=clang`. Use `make clean` when
changing SDK roots or toolchains. Paths supplied to this Makefile should not
contain spaces.

The build reads the SDK's FreeRTOS v10.5.1 sources, `GCC/AMapollo2` port (the
port used by the SDK's Apollo3 example), startup file, linker script and existing
HAL/BSP archives. It does not modify the SDK. If those archives are absent,
build the SDK's Apollo3 HAL and Apollo3 EVB BSP first.

Outputs:

- `build/amb3_cli.elf`: linked firmware with debug symbols.
- `build/amb3_cli.bin`: raw firmware image, linked at **0x0000C000**, matching
  the SDK example's application layout. Use the board's existing bootloader or
  debugger workflow with this address; it is not a bootloader image.
- `build/amb3_cli.map`: link map.

`make test` runs host tests for parsing and line handling with AddressSanitizer
and UndefinedBehaviorSanitizer. Tests cover all byte values, write counts 1–16,
a rejected seventeenth byte, invalid syntax, read-length limits, CR/LF/CRLF,
editing, cancellation, long lines and recovery after input corruption.
Firmware compilation/linking and host tests have passed; physical UART/board
operation has not been tested.

## Serial connection and commands

Use **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**, with
terminal local echo disabled. The SDK Apollo3 EVB BSP selects UART0 TX on GPIO
22 and RX on GPIO 23. Use the EVB console connection or compatible logic-level
serial connection to those pins with a common ground.

```text
> i2c_read 0x50 0x10 0x04
OK queued
> i2c_write 0x50 0x10 0xAB 0xCD 0x00
OK queued
> help
```

Rules:

- Commands are case sensitive. Numeric tokens must have exactly four
  characters: lowercase `0x` followed by two hex digits. Both `0xAB` and
  `0xab` work. `0XAB`, `AB`, `0xA`, signed values and values over one byte fail.
- `i2c_read` requires exactly three arguments. Length is `0x01`–`0xFF`.
- `i2c_write` requires an address, a register and **1–16 data bytes**.
- Address and register are each one byte. The address is passed unchanged,
  without shifting or limiting it to seven bits. The future I²C worker must
  establish and validate the address convention before using the IOM HAL.
- Spaces and tabs separate arguments; leading/trailing whitespace is allowed.
- Enter accepts CR, LF or CRLF. Backspace/Delete erase the preceding character.
  Ctrl-C cancels the current input; it does not cancel already queued requests.
- Input is limited to 127 characters; a full 16-byte write with single spaces
  takes 99. An overlong line is discarded in full through the next newline.
  Unsupported control bytes, including terminal arrow-key escapes, also reject
  the whole line. History and cursor navigation are not implemented.

`OK queued` acknowledges queue acceptance only. It does **not** mean that an
I²C transaction has completed. The demonstration has eight queue slots and no
consumer, so after eight accepted commands it reports
`ERR I2C queue full; command not queued`. Reset the demonstration to empty it,
or integrate the intended worker. No I²C pins or peripheral are configured.

## Integrate into your firmware

Add `src/cli_task.c`, `src/cli_command.c`, `src/cli_line.c` and `include/` to your
Apollo3 FreeRTOS application. `example/main.c` shows board initialization and
startup. After board initialization, before starting the scheduler:

```c
#include "cli_task.h"

QueueHandle_t requests = xQueueCreate(8, sizeof(cli_i2c_request_t));
configASSERT(requests != NULL);
cli_start_result_t result = cli_start(requests);
configASSERT(result == CLI_START_OK);

/* Create your I2C worker here, supplying the same queue. */
vTaskStartScheduler();
```

In your future worker, receive into a local request:

```c
cli_i2c_request_t request;
if (xQueueReceive(requests, &request, portMAX_DELAY) == pdPASS) {
    /* Interpret request.operation, addr, reg, len and data here. */
}
```

`cli_i2c_request_t` is defined in `include/cli_command.h`. The queue copies the
entire structure, including its inline 16-byte write payload. It contains no
pointers or caller-owned buffers. For reads, `len` is the requested byte count
and `data` is zeroed/unused; the worker must supply its own read buffer. This
structure is an in-process queue item, not a packed serial protocol.

Keep the request queue alive for the lifetime of the CLI. Its item size **must**
be `sizeof(cli_i2c_request_t)`; FreeRTOS does not expose a public item-size check
here. Call `cli_start()` once, before the scheduler. Failed startup returns a
status and releases CLI resources; a second successful start is rejected.

UART0 and the SDK vector handler `am_uart_isr` belong exclusively to this module.
Do not also enable BSP UART printing or another UART0 driver. RX interrupts
drain at most one hardware FIFO into a 256-byte FreeRTOS queue. Parsing, echo
and request dispatch happen only in `cli_task`. The task uses a 512-word stack
(2048 bytes on this Cortex-M4) at idle priority + 1. Adjust those constants in
`cli_task.h` for your application and measure stack headroom on hardware.

The UART ISR uses `xQueueSendFromISR` and `portYIELD_FROM_ISR`. Its CMSIS priority
is derived from `configKERNEL_INTERRUPT_PRIORITY`, so it must remain within
the FreeRTOS API-safe interrupt range. The example uses priority 7 and a maximum
syscall priority of 4. The task needs dynamic allocation, `vTaskDelay`, and
`vTaskSuspend` enabled. No FreeRTOS+CLI package is required.

On an RX queue overflow or UART receive error, the task drops buffered input
and rejects the current line. Press Enter and retry after the error. This can
discard otherwise valid buffered commands; it avoids executing a damaged or
truncated write. Hosts should send one command and wait for the response/prompt
before sending another, especially with no hardware flow control.

TX uses nonblocking HAL writes, yielding between partial writes, with a one-second
deadline per output string. A TX HAL failure or timeout records
`cli_last_hal_error`, disables the UART interrupt and suspends the CLI so it
cannot continue accepting commands without visible responses.

The standalone example uses a 48 MHz core and a 1 kHz SysTick with tickless idle
disabled. If you integrate into the SDK's low-power example, adapt its sleep
policy for an active UART before enabling deep sleep. The included stack and
allocation hooks stop with debugger-visible assertion information.

## Useful next additions

The essentials already included are help, strict validation, bounded input,
editing/cancel, queue-full errors, RX loss recovery, allocation checks and stack
overflow checking.

The most useful next step is a **response queue from the I²C worker to the CLI**:
return read bytes, completion status and errors such as NACK or timeout. If
multiple requests may be outstanding, add a request ID to both request and
response structures. Keep UART output in the CLI task so asynchronous responses
and other logs cannot interleave with input or corrupt the prompt.

For integration, also define I²C transaction timeouts and address semantics in
the worker. Optional CLI additions include firmware-version/status commands,
error counters and command history. Measure task stack headroom and exercise
UART overrun, queue-full and bus failure recovery on the real board.

## SDK references used

These paths are relative to `SDK_ROOT`:

- `boards/apollo3_evb/examples/hello_world_uart/src/hello_world_uart.c`
- `boards/apollo3_evb/bsp/am_bsp_pins.h` and `am_bsp_pins.c`
- `mcu/apollo3/hal/am_hal_uart.h` and `am_hal_uart.c`
- `boards/apollo3_evb/examples/freertos_lowpower/src/FreeRTOSConfig.h`
- `boards/apollo3_evb/examples/freertos_lowpower/gcc/Makefile`,
  `startup_gcc.c` and `freertos_lowpower.ld`
- `third_party/FreeRTOSv10.5.1/Source/portable/GCC/AMapollo2/port.c`

SDK sources and prebuilt libraries retain their original license terms; see
the SDK files and its `docs/licenses` directory.
