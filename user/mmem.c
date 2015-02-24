#include "mmem.h"
#include <c_types.h>
#include <mem.h>

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

void mfree(char **bufptr)
{
	if(*bufptr) {
		os_free(*bufptr);
		*bufptr = NULL;
	}
}
