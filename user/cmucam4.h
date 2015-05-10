#ifndef __CMUCAM4_H

#define __CMUCAM4_H

#include <c_types.h>

int ICACHE_FLASH_ATTR cmucam4_send(const char *cmd);
int ICACHE_FLASH_ATTR cmucam4_color(uint8_t buf[], int bufsize);
int ICACHE_FLASH_ATTR cmucam4_bw(uint8_t buf[], int bufsize);
int ICACHE_FLASH_ATTR cmucam4_tc(uint8_t buf[], int bufsize);
int ICACHE_FLASH_ATTR cmucam4_tw(uint8_t buf[], int bufsize);


#endif //CMUCAM4_H
