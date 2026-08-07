#include "banked_probe.h"

volatile unsigned int banked_probe_result;

int main(void)
{
    banked_probe_result = bank1_add(41u);

    for (;;) {
    }
}
