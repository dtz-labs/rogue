#include "zx_restart.h"

/* Bank 4 has room for the pristine copies of all pageable DATA sections. */
unsigned char zx_restart_banked_data_bank4[ZX_RESTART_BANKED_DATA_SIZE];
