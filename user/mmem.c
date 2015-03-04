#include "mmem.h"
#include <c_types.h>
#include <mem.h>
#include <osapi.h>
#include <spi_flash.h>
#include "espmissingincludes.h"

#define NL "\n"
#define FF_END 0x3FFFF
#define FF_START (FF_END + 1 - FF_SIZE)

char *mnewstr(char *addbuf)
{
	int alen = strlen(addbuf);
	char *rs = os_malloc(alen+1);
	memcpy(rs, addbuf, alen);
	rs[alen] = 0;
	return rs;
}

char *mconcat(char **bufptr, char *addbuf)
{
	int blen = (*bufptr) ? strlen(*bufptr) : 0;
	int alen = strlen(addbuf);
	char *rs = os_malloc(blen + alen + 1);
	if(blen) {
		memcpy(rs, *bufptr, blen);
	}
	memcpy(rs+blen, addbuf, alen);
	rs[blen + alen] = 0;
	os_free(*bufptr);
	*bufptr = rs;
	return rs;
}

char *madd_ip(char **bufptr, uint32_t ip)
{
	char buf[5];
	int i;
	for(i=0; i<4; i++) {
		if(i > 0) mconcat(bufptr, ".");
		os_sprintf(buf, "%d", (uint8_t)(ip & 0xff));
		mconcat(bufptr, buf);
		ip = ip >> 8;
	}
	return *bufptr;
}

void mfree(char **bufptr)
{
	if(*bufptr) {
		os_free(*bufptr);
		*bufptr = NULL;
	}
}

static int ff_cpos = 0;
static int ff_csize = 0;

#define CACHE_SIZE 128
static char cache[CACHE_SIZE + 1];

void ff_seek(int pos)
{
	if(pos >= ff_csize) pos = ff_csize - 1;
	if(pos < 0) pos = 0;
	ff_cpos = pos;
}

void ff_reset()
{
	ff_seek(0);
}

void ff_erase()
{
	int i;
	ff_csize = 0;
	ff_reset();
	for(i = 0; i<FF_SIZE / SPI_FLASH_SEC_SIZE; i++) {
		spi_flash_erase_sector(FF_START / SPI_FLASH_SEC_SIZE + i);
	}
}

int ff_tell()
{
	return ff_cpos;
}

char *ff_mread_alloc(int size)
{
	char *rs = os_malloc(size);
	spi_flash_read(FF_START + ff_cpos, (uint32 *)rs, size);
	ff_cpos += size;
	return rs;
}

char *ff_mread_str()
{
	char *rs = NULL;
	int i;
	int found = 0;
	while(ff_cpos < ff_csize && !found) {
		spi_flash_read(FF_START + ff_cpos, (uint32 *)cache, CACHE_SIZE);

//		cache[CACHE_SIZE] = 0;
//		os_printf("Read(from %d):[%s]\n", ff_cpos, cache);
		for(i=0; i<CACHE_SIZE; i++) {
			if(cache[i] == 0xa || cache[i] == 0xd || ff_cpos + i >= ff_csize) {
				cache[i++] = 0;
				if(cache[i] == 0xa || cache[i] == 0xd) {
					cache[i++] = 0;
				}
				found = 1;
				break;
			}
		}
		ff_cpos += i;
		mconcat(&rs, cache);
	}
	return rs;
}

void ff_write_str(const char *buf)
{
	ff_write_bytes(buf, strlen(buf));
	ff_write_bytes(NL, sizeof(NL) - 1);
}

void ff_write_bytes(const uint8_t *buf, int size)
{
	spi_flash_write(FF_START + ff_cpos, (uint32 *)buf, size);
//		int sz = (CACHE_SIZE > size) ? size : CACHE_SIZE;
//		memcpy(cache, buf, sz);
//		cache[sz] = 0;
//		os_printf("Write(%d):[%s]\n", ff_cpos, cache);
//	spi_flash_read(FF_START + ff_cpos, (uint32 *)cache, sz);
//		cache[sz] = 0;
//		os_printf("Read(%d):[%s]\n", ff_cpos, cache);
	ff_cpos += size;
	ff_csize = ff_cpos;
}

