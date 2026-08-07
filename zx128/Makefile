.DEFAULT_GOAL := all
ROOT := $(abspath ..)
Z88DK ?=

ifneq ($(strip $(Z88DK)),)
Z88DK_PREFIX := $(if $(filter /%,$(Z88DK)),$(abspath $(Z88DK)),$(abspath $(ROOT)/$(Z88DK)))
ZCC := $(Z88DK_PREFIX)/bin/zcc
ZCC_ENV := PATH=$(Z88DK_PREFIX)/bin:/usr/bin:/bin ZCCCFG=$(Z88DK_PREFIX)/lib/config
else
ZCC ?= zcc
ZCC_ENV :=
endif

ZESARUX ?= $(shell command -v zesarux 2>/dev/null || \
	test ! -x /Applications/ZEsarUX.app/Contents/MacOS/zesarux || \
	printf '%s' /Applications/ZEsarUX.app/Contents/MacOS/zesarux)
ZESARUX_FLAGS ?=

BUILD := build
BASENAME := $(BUILD)/rogue-zx128
MAIN_BIN := $(BASENAME).bin
MAP := $(BASENAME).map
TAP := $(BASENAME).tap

CPPFLAGS := -DZX128 -Iinclude -I$(ROOT)
OPT ?= -SO2 --max-allocs-per-node10000
WARNFLAGS ?= --less-pedantic
CFLAGS := +zx -compiler=sdcc $(OPT) $(WARNFLAGS) -vn $(CPPFLAGS)
LDFLAGS := +zx -compiler=sdcc $(OPT) $(WARNFLAGS) -vn \
	-pragma-define:CRT_ORG_CODE=0x6000 \
	-pragma-define:REGISTER_SP=0xbfff \
	-pragma-define:CRT_STACK_SIZE=3072 \
	-pragma-export:CLIB_BANKING_STACK_SIZE=256 \
	-lndos -m -create-app

FIXED_NAMES := extern daemon io list
FIXED_LOCAL_NAMES := platform curses_stub callback_thunks
BANK0_NAMES := command move armor rings
BANK1_NAMES := pack things options
BANK3_NAMES := init misc monsters wizard vers rip_zx
BANK4_NAMES := potions scrolls sticks weapons
BANK6_NAMES := chase fight daemons
BANK7_NAMES := new_level rooms passages

FIXED_OBJS := $(addprefix $(BUILD)/fixed/,$(addsuffix .o,$(FIXED_NAMES))) \
	$(addprefix $(BUILD)/fixed/,$(addsuffix .o,$(FIXED_LOCAL_NAMES)))
BANK0_OBJS := $(addprefix $(BUILD)/bank0/,$(addsuffix .o,$(BANK0_NAMES)))
BANK1_OBJS := $(addprefix $(BUILD)/bank1/,$(addsuffix .o,$(BANK1_NAMES)))
BANK3_OBJS := $(addprefix $(BUILD)/bank3/,$(addsuffix .o,$(BANK3_NAMES)))
BANK4_OBJS := $(addprefix $(BUILD)/bank4/,$(addsuffix .o,$(BANK4_NAMES)))
BANK6_OBJS := $(addprefix $(BUILD)/bank6/,$(addsuffix .o,$(BANK6_NAMES)))
BANK7_OBJS := $(addprefix $(BUILD)/bank7/,$(addsuffix .o,$(BANK7_NAMES)))
OBJS := $(FIXED_OBJS) $(BANK0_OBJS) $(BANK1_OBJS) $(BANK3_OBJS) \
	$(BANK4_OBJS) $(BANK6_OBJS) $(BANK7_OBJS)
COMMON_DEPS := Makefile $(ROOT)/rogue.h $(ROOT)/extern.h \
	include/curses.h include/zx_banking.h

$(OBJS): $(COMMON_DEPS)

.PHONY: all check-zesarux clean layout run-zesarux run-zesarux-zx128

all: $(TAP)

$(BUILD)/fixed/%.o: $(ROOT)/%.c
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -c $< -o $@

$(BUILD)/fixed/%.o: %.c
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -c $< -o $@

define BANK_RULE
$(BUILD)/bank$(1)/%.o: $(ROOT)/%.c
	@mkdir -p $$(@D)
	$$(ZCC_ENV) $$(ZCC) $$(CFLAGS) -DZX_BANK_$(1) \
		--codeseg CODE_$(1) --constseg RODATA_$(1) -c $$< -o $$@

$(BUILD)/bank$(1)/%.o: %.c
	@mkdir -p $$(@D)
	$$(ZCC_ENV) $$(ZCC) $$(CFLAGS) -DZX_BANK_$(1) \
		--codeseg CODE_$(1) --constseg RODATA_$(1) -c $$< -o $$@
endef

$(eval $(call BANK_RULE,0))
$(eval $(call BANK_RULE,1))
$(eval $(call BANK_RULE,3))
$(eval $(call BANK_RULE,4))
$(eval $(call BANK_RULE,6))
$(eval $(call BANK_RULE,7))

$(BUILD)/bank7/rooms.o: $(ROOT)/rooms.c
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -DZX_BANK_7 \
		--codeseg CODE_7 --constseg RODATA_7 \
		--dataseg DATA_7 --bssseg BSS_7 -c $< -o $@

$(TAP): $(OBJS)
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(LDFLAGS) -o $(MAIN_BIN) $(OBJS)
	@test -f "$@" || { echo "z88dk did not create expected TAP: $@" >&2; exit 1; }

layout: $(TAP)
	python3 tools/check_layout.py --map $(MAP) --main-bin $(MAIN_BIN) \
		--stack-reserve 3072 --heap-reserve 3072

check-zesarux:
	@command -v "$(ZESARUX)" >/dev/null 2>&1 || { \
		echo "ZEsarUX not found: $(ZESARUX)" >&2; \
		echo "Set ZESARUX=/path/to/zesarux." >&2; \
		exit 127; \
	}

run-zesarux: run-zesarux-zx128

run-zesarux-zx128: $(TAP) check-zesarux
	@echo "NOTE: this is still a ZX128 bring-up TAP; 'make layout' currently rejects it."
	"$(ZESARUX)" --noconfigfile --machine 128k \
		--tape "$(abspath $(TAP))" --fastautoload $(ZESARUX_FLAGS)

clean:
	rm -rf $(BUILD)
