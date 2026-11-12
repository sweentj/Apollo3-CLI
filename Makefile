SDK_ROOT ?= /Users/tjswe/Solinst/Software/AmbiqSuite_R3.2.0
TOOLCHAIN ?= arm-none-eabi
HOST_CC ?= cc
ARM_CC := $(TOOLCHAIN)-gcc
ARM_OBJCOPY := $(TOOLCHAIN)-objcopy
ARM_SIZE := $(TOOLCHAIN)-size

FREERTOS := $(SDK_ROOT)/third_party/FreeRTOSv10.5.1/Source
SDK_EXAMPLE := $(SDK_ROOT)/boards/apollo3_evb/examples/freertos_lowpower/gcc
SDK_LIBS := $(SDK_ROOT)/mcu/apollo3/hal/gcc/bin/libam_hal.a \
            $(SDK_ROOT)/boards/apollo3_evb/bsp/gcc/bin/libam_bsp.a
INCLUDES := -Iinclude -Iexample -IFreeRTOS-Plus-CLI -I$(SDK_ROOT) \
            -I$(SDK_ROOT)/CMSIS/ARM/Include -I$(SDK_ROOT)/CMSIS/AmbiqMicro/Include \
            -I$(SDK_ROOT)/mcu/apollo3 -I$(SDK_ROOT)/boards/apollo3_evb/bsp \
            -I$(SDK_ROOT)/utils -I$(SDK_ROOT)/devices \
            -I$(FREERTOS)/include -I$(FREERTOS)/portable/GCC/AMapollo2
ARCH := -mthumb -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFINES := -DPART_apollo3 -DAM_PART_APOLLO3 -DAM_PACKAGE_BGA -DAM_FREERTOS -Dgcc
CFLAGS := $(ARCH) $(DEFINES) $(INCLUDES) -std=c99 -Os -g3 \
          -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP
APP_OBJS := build/cli_command.o build/cli_line.o build/cli_task.o build/main.o
SDK_OBJS := build/tasks.o build/queue.o build/list.o build/heap_4.o build/port.o build/startup_gcc.o
OBJS := $(APP_OBJS) $(SDK_OBJS) build/FreeRTOS_CLI.o

.PHONY: all test clean
all: build/amb3_cli.bin

build:
	mkdir -p $@

build/%.o: src/%.c | build
	$(ARM_CC) $(CFLAGS) -Werror -c $< -o $@

build/main.o: example/main.c | build
	$(ARM_CC) $(CFLAGS) -Werror -c $< -o $@

build/FreeRTOS_CLI.o: FreeRTOS-Plus-CLI/FreeRTOS_CLI.c | build
	$(ARM_CC) $(CFLAGS) -c $< -o $@

build/%.o: $(FREERTOS)/%.c | build
	$(ARM_CC) $(CFLAGS) -c $< -o $@

build/heap_4.o: $(FREERTOS)/portable/MemMang/heap_4.c | build
	$(ARM_CC) $(CFLAGS) -c $< -o $@

build/port.o: $(FREERTOS)/portable/GCC/AMapollo2/port.c | build
	$(ARM_CC) $(CFLAGS) -c $< -o $@

build/startup_gcc.o: $(SDK_EXAMPLE)/startup_gcc.c | build
	$(ARM_CC) $(CFLAGS) -c $< -o $@

build/amb3_cli.elf: $(OBJS) $(SDK_LIBS) $(SDK_EXAMPLE)/freertos_lowpower.ld
	$(ARM_CC) $(ARCH) -nostartfiles -static \
	    -T$(SDK_EXAMPLE)/freertos_lowpower.ld -Wl,--gc-sections,-Map,build/amb3_cli.map \
	    $(OBJS) -Wl,--start-group $(SDK_LIBS) -lc -lm -lgcc -Wl,--end-group -o $@
	$(ARM_SIZE) $@

build/amb3_cli.bin: build/amb3_cli.elf
	$(ARM_OBJCOPY) -O binary $< $@

HOST_SOURCES := tests/test_cli.c tests/support/freertos_host.c src/cli_command.c \
                src/cli_line.c FreeRTOS-Plus-CLI/FreeRTOS_CLI.c
HOST_HEADERS := include/cli_command.h include/cli_line.h FreeRTOS-Plus-CLI/FreeRTOS_CLI.h \
                example/FreeRTOSConfig.h $(wildcard tests/support/*.h)

build/test_cli: $(HOST_SOURCES) $(HOST_HEADERS) | build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -pedantic -g \
	    -fsanitize=address,undefined -Itests/support -Iexample -Iinclude -IFreeRTOS-Plus-CLI \
	    $(HOST_SOURCES) -o $@

test: build/test_cli
	./build/test_cli

clean:
	rm -rf build

-include $(OBJS:.o=.d)
