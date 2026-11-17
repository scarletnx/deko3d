# tests/cmdwords: cmdwords.nro, which records deko3d calls in capture mode (dkCmdBufBeginCaptureCmds) and checks the
# command words they make, linked against this tree's lib/libdeko3dd.a, the debug library, which checks each call's
# input (build the library first); an error it reports ends the check that made it. Each source/t_*.c adds its
# checks. The NRO prints one line a check, then a summary, with svcOutputDebugString and to stdout, and returns 1 when
# a check failed. It records commands only: nothing is submitted to the GPU. Its __appInit opens only sm and the
# applet service, so it also runs where a program may not open the file system or the clock itself.
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif
include $(DEVKITPRO)/libnx/switch_rules

TOP     := $(abspath $(CURDIR)/../..)
BUILD   := build
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS  := $(ARCH) -g -O2 -Wall -Wextra -Werror -std=gnu11 -D__SWITCH__ -I$(TOP)/include -I$(LIBNX)/include
LDFLAGS := $(ARCH) -specs=$(LIBNX)/switch.specs -g
LIBS    := -L$(TOP)/lib -ldeko3dd -L$(LIBNX)/lib -lnx -lm

CFILES  := $(sort $(wildcard source/*.c))
OFILES  := $(patsubst source/%.c,$(BUILD)/%.o,$(CFILES))

.PHONY: all clean
all: cmdwords.nro

$(BUILD)/%.o: source/%.c source/cmdwords.h $(TOP)/include/deko3d.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

cmdwords.elf: $(OFILES) $(TOP)/lib/libdeko3dd.a
	$(CC) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

cmdwords.nacp:
	nacptool --create "cmdwords" "deko3d" "1.0.0" $@

cmdwords.nro: cmdwords.elf cmdwords.nacp
	elf2nro $< $@ --nacp=cmdwords.nacp

clean:
	rm -rf $(BUILD) cmdwords.elf cmdwords.nacp cmdwords.nro
