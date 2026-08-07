#include "banked_probe.h"

#pragma bank 1

unsigned int bank1_add(unsigned int value)
{
    return value + 1u;
}
