#ifndef _MMEM_H
#define _MMEM_H

char *mnewstr(char *addbuf);
char *mconcat(char **bufptr, char *addbuf);
void mfree(char **bufptr);

#endif // _MMEM_H
