#ifndef ROGUE_ZX_BANKING_H
#define ROGUE_ZX_BANKING_H

/*
 * A declaration must be __banked for callers outside its 16K page.
 * The defining translation units see an empty suffix so SDCC can keep
 * direct calls inside a bank.  The linker still places their code in the
 * CODE_n section selected by the Makefile.
 */
#if defined(ZX128)
# if defined(ZX_BANK_0)
#  define ZX_BANKED_0
# else
#  define ZX_BANKED_0 __banked
# endif
# if defined(ZX_BANK_1)
#  define ZX_BANKED_1
# else
#  define ZX_BANKED_1 __banked
# endif
# if defined(ZX_BANK_3)
#  define ZX_BANKED_3
# else
#  define ZX_BANKED_3 __banked
# endif
# if defined(ZX_BANK_4)
#  define ZX_BANKED_4
# else
#  define ZX_BANKED_4 __banked
# endif
# if defined(ZX_BANK_6)
#  define ZX_BANKED_6
# else
#  define ZX_BANKED_6 __banked
# endif
# if defined(ZX_BANK_7)
#  define ZX_BANKED_7
# else
#  define ZX_BANKED_7 __banked
# endif
#else
# define ZX_BANKED_0
# define ZX_BANKED_1
# define ZX_BANKED_3
# define ZX_BANKED_4
# define ZX_BANKED_6
# define ZX_BANKED_7
#endif
#define ZX_FIXED

#endif
