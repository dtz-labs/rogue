.DEFAULT_GOAL := all
ROOT := $(abspath ..)
Z88DK ?=
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

# GNU Make has no portable "-j auto" mode.  Start the actual build in a
# recursive make using all available logical CPUs, unless the caller already
# supplied a -j option.
AUTO_JOBS = $(if $(filter -j%,$(MAKEFLAGS)),,-j$(JOBS))

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

# The one place the port's version is written down. CI and the release step
# ask for it with `make print-version` / `print-dist-tap` rather than repeating
# it.
ZX128_VERSION ?= 0.1.2

BUILD := build
# z88dk's appmake locates the bank images by splitting the output name at the
# first dot, so a dotted version here silently yields a TAP holding only the
# loader and the fixed image -- it links and passes the object and layout
# checks, and only fails when the machine tries to boot it. Build under the
# plain name and stamp the version on at packaging time instead.
BASENAME := $(BUILD)/rogue-zx128
MAIN_BIN := $(BASENAME).bin
MAP := $(BASENAME).map
TAP := $(BASENAME).tap
DIST_TAP := $(BUILD)/rogue-zx128-$(ZX128_VERSION).tap
SMOKE_PBM ?= $(BUILD)/rogue-zx128-smoke.pbm

CPPFLAGS := -DZX128 -Iinclude -I$(ROOT)
OPT ?= -SO2 --max-allocs-per-node10000
WARNFLAGS ?= --less-pedantic
CFLAGS := +zx -compiler=sdcc --reserve-regs-iy $(OPT) $(WARNFLAGS) -vn $(CPPFLAGS)
LDFLAGS := +zx -compiler=sdcc $(OPT) $(WARNFLAGS) -vn \
	-pragma-define:CRT_ORG_CODE=0x6000 \
	-pragma-define:REGISTER_SP=0xbfff \
	-pragma-define:CRT_STACK_SIZE=3072 \
	-pragma-export:CLIB_BANKING_STACK_SIZE=256 \
	-pragma-define:CRT_ENABLE_STDIO=0 \
	-pragma-redirect:fputc_cons=zx_fputc_cons_null \
	-pragma-define:CLIB_OPT_PRINTF=0x40001601 \
	-pragma-define:CLIB_OPT_PRINTF_2=0 \
	-pragma-define:CLIB_EXIT_STACK_SIZE=0 \
	-lndos -m -create-app

FIXED_NAMES := extern io
FIXED_LOCAL_NAMES := platform curses_stub callback_thunks banked_data viewport
FIXED_ASM_NAMES := fputc_cons_null
BANK0_NAMES := command move armor rings help_zx
BANK1_NAMES := pack things options
BANK3_NAMES := init misc monsters wizard rip_zx
BANK4_NAMES := potions scrolls sticks weapons restart_storage
BANK6_NAMES := chase fight daemons renderer message
BANK7_NAMES := new_level rooms passages daemon list screen_snapshot restart_snapshot

FIXED_OBJS := $(addprefix $(BUILD)/fixed/,$(addsuffix .o,$(FIXED_NAMES))) \
	$(addprefix $(BUILD)/fixed/,$(addsuffix .o,$(FIXED_LOCAL_NAMES))) \
	$(addprefix $(BUILD)/fixed/,$(addsuffix .o,$(FIXED_ASM_NAMES)))
BANK0_OBJS := $(addprefix $(BUILD)/bank0/,$(addsuffix .o,$(BANK0_NAMES)))
BANK1_OBJS := $(addprefix $(BUILD)/bank1/,$(addsuffix .o,$(BANK1_NAMES)))
BANK3_OBJS := $(addprefix $(BUILD)/bank3/,$(addsuffix .o,$(BANK3_NAMES)))
BANK4_OBJS := $(addprefix $(BUILD)/bank4/,$(addsuffix .o,$(BANK4_NAMES)))
BANK6_OBJS := $(addprefix $(BUILD)/bank6/,$(addsuffix .o,$(BANK6_NAMES)))
BANK7_OBJS := $(addprefix $(BUILD)/bank7/,$(addsuffix .o,$(BANK7_NAMES)))
OBJS := $(FIXED_OBJS) $(BANK0_OBJS) $(BANK1_OBJS) $(BANK3_OBJS) \
	$(BANK4_OBJS) $(BANK6_OBJS) $(BANK7_OBJS) \
	$(BUILD)/bank0/bank_storage.o $(BUILD)/bank1/bank_storage.o \
	$(BUILD)/bank3/bank_storage.o $(BUILD)/bank4/bank_storage.o
COMMON_DEPS := Makefile $(ROOT)/rogue.h $(ROOT)/extern.h \
	include/curses.h include/zx_banking.h include/zx_message.h \
	include/zx_places.h include/zx_restart.h

$(OBJS): $(COMMON_DEPS)

.PHONY: all build check-banked-objects check-zesarux clean dist layout print-dist-tap \
	print-version run-zesarux run-zesarux-zx128 smoke-zesarux smoke-zesarux-zx128

# Consumed by CI and the release step so the version lives here only.
print-version:
	@printf '%s\n' '$(ZX128_VERSION)'

print-dist-tap:
	@printf '%s\n' '$(notdir $(DIST_TAP))'

# The distributable copy carries the version in its name so a downloaded TAP
# identifies itself. CI runs this only after the smoke job, so an unbootable
# TAP cannot reach a release.
dist: $(DIST_TAP)

$(DIST_TAP): $(TAP)
	cp $< $@

all: build

build:
	+$(MAKE) $(AUTO_JOBS) $(TAP)

$(BUILD)/fixed/%.o: $(ROOT)/%.c
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -c $< -o $@

$(BUILD)/fixed/%.o: %.c
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -c $< -o $@

$(BUILD)/fixed/%.o: %.asm
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -c $< -o $@

define BANK_RULE
$(BUILD)/bank$(1)/%.o: $(ROOT)/%.c
	@mkdir -p $$(@D)
	$$(ZCC_ENV) $$(ZCC) $$(CFLAGS) -DZX_BANK_$(1) \
		--codeseg CODE_$(1) --constseg RODATA_$(1) \
		--dataseg DATA_$(1) --bssseg BSS_$(1) -c $$< -o $$@

$(BUILD)/bank$(1)/%.o: %.c
	@mkdir -p $$(@D)
	$$(ZCC_ENV) $$(ZCC) $$(CFLAGS) -DZX_BANK_$(1) \
		--codeseg CODE_$(1) --constseg RODATA_$(1) \
		--dataseg DATA_$(1) --bssseg BSS_$(1) -c $$< -o $$@
endef

$(eval $(call BANK_RULE,0))
$(eval $(call BANK_RULE,1))
$(eval $(call BANK_RULE,3))
$(eval $(call BANK_RULE,4))
$(eval $(call BANK_RULE,6))
$(eval $(call BANK_RULE,7))

define STORAGE_RULE
$(BUILD)/bank$(1)/bank_storage.o: bank_storage.c
	@mkdir -p $$(@D)
	$$(ZCC_ENV) $$(ZCC) $$(CFLAGS) -DZX_STORAGE_BANK=$(1) \
		--bssseg BSS_$(1) -c $$< -o $$@
endef

$(eval $(call STORAGE_RULE,0))
$(eval $(call STORAGE_RULE,1))
$(eval $(call STORAGE_RULE,3))
$(eval $(call STORAGE_RULE,4))

$(BUILD)/bank7/rooms.o: $(ROOT)/rooms.c
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(CFLAGS) -DZX_BANK_7 \
		--codeseg CODE_7 --constseg RODATA_7 \
		--dataseg DATA_7 --bssseg BSS_7 -c $< -o $@

$(TAP): $(OBJS)
	@mkdir -p $(@D)
	$(ZCC_ENV) $(ZCC) $(LDFLAGS) -o $(MAIN_BIN) $(OBJS)
	@test -f "$@" || { echo "z88dk did not create expected TAP: $@" >&2; exit 1; }

check-banked-objects: build
	$(ZCC_ENV) sh tools/check_banked_objects.sh z88dk-z80nm $(BUILD)

layout: check-banked-objects
	python3 tools/check_layout.py --map $(MAP) --main-bin $(MAIN_BIN) \
		--stack-reserve 3072 --heap-reserve 3072
	python3 tools/check_bank_calls.py --map $(MAP) --root $(ROOT)

check-zesarux:
	@command -v "$(ZESARUX)" >/dev/null 2>&1 || { \
		echo "ZEsarUX not found: $(ZESARUX)" >&2; \
		echo "Set ZESARUX=/path/to/zesarux." >&2; \
		exit 127; \
	}

run-zesarux: run-zesarux-zx128

run-zesarux-zx128: layout check-zesarux
	"$(ZESARUX)" --noconfigfile --machine 128k \
		--tape "$(abspath $(TAP))" --fastautoload $(ZESARUX_FLAGS)

smoke-zesarux: smoke-zesarux-zx128

smoke-zesarux-zx128: layout check-zesarux
	python3 -u tools/zesarux_smoke.py \
		--zesarux "$(ZESARUX)" \
		--tap "$(abspath $(TAP))" \
		--map "$(abspath $(MAP))" \
		--screenshot "$(abspath $(SMOKE_PBM))"

clean:
	rm -rf $(BUILD)
