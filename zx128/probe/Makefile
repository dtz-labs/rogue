Z88DK ?=

ifneq ($(strip $(Z88DK)),)
ZCC := $(Z88DK)/bin/zcc
ZCC_ENV := PATH=$(Z88DK)/bin:/usr/bin:/bin ZCCCFG=$(Z88DK)/lib/config
else
ZCC ?= zcc
ZCC_ENV :=
endif

BUILD_DIR := build
APP := $(BUILD_DIR)/banked-probe.bin
MAP := $(BUILD_DIR)/banked-probe.map
TAP := $(BUILD_DIR)/banked-probe.tap
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/bank1.o

OPT ?= -SO2 --max-allocs-per-node10000
COMMON_FLAGS := +zx -vn -clib=default -compiler=sdcc $(OPT) \
	--less-pedantic -I.

.PHONY: all probe check clean

all: probe

probe: $(TAP) check

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.c banked_probe.h Makefile | $(BUILD_DIR)
	$(ZCC_ENV) $(ZCC) $(COMMON_FLAGS) -c $< -o $@

$(BUILD_DIR)/bank1.o: bank1.c banked_probe.h Makefile | $(BUILD_DIR)
	$(ZCC_ENV) $(ZCC) $(COMMON_FLAGS) -DPROBE_BANK_1 -c $< -o $@

$(TAP): $(OBJECTS)
	$(ZCC_ENV) $(ZCC) +zx -vn -clib=default -compiler=sdcc $(OPT) \
		--less-pedantic -m -o $(APP) $(OBJECTS) \
		-lndos -create-app

check: $(TAP)
	python3 ../tools/check_layout.py --map $(MAP) --main-bin $(APP)

clean:
	$(RM) $(OBJECTS) $(APP) $(MAP) $(TAP) \
		$(BUILD_DIR)/banked-probe_BANK_0.bin \
		$(BUILD_DIR)/banked-probe_BANK_1.bin \
		$(BUILD_DIR)/banked-probe_BANK_2.bin \
		$(BUILD_DIR)/banked-probe_BANK_3.bin \
		$(BUILD_DIR)/banked-probe_BANK_4.bin \
		$(BUILD_DIR)/banked-probe_BANK_5.bin \
		$(BUILD_DIR)/banked-probe_BANK_6.bin \
		$(BUILD_DIR)/banked-probe_BANK_7.bin
