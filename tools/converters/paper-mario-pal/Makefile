# Tested with pm64hh:decomp and its Miyoo ARM hard-float toolchain.
CROSS_COMPILE ?= /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
OUT ?= build/guide-pal-rom-converter
CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -ffp-contract=off

all: $(OUT)

$(OUT): guide_pal_rom_native.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -lm -o $@

.PHONY: all
