#ifndef _MMEM_H
#define _MMEM_H

#include <stdint.h>

char *mnewstr(char *addbuf);
char *mconcat(char **bufptr, char *addbuf);
char *madd_ip(char **bufptr, uint32_t ip);
void mfree(char **bufptr);

#define FF_SIZE 0x11000

void ff_reset();
void ff_seek(int pos);
void ff_erase();
int ff_tell();
char *ff_mread_str();
char *ff_mread_alloc(int size);
void ff_write_str(const char *buf);
void ff_write_bytes(const uint8_t *buf, int size);

#endif // _MMEM_H
