#ifndef ZX128_BANKED_PROBE_H
#define ZX128_BANKED_PROBE_H

#ifdef PROBE_BANK_1
#define PROBE_BANKED_1
#else
#define PROBE_BANKED_1 __banked
#endif

extern unsigned int bank1_add(unsigned int value) PROBE_BANKED_1;

#endif
