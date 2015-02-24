#ifndef __CONFIG_H__
#define __CONFIG_H__

typedef struct espconn *p_espconn;

void config_parse(p_espconn conn, char *buf, int len);

#endif /* __CONFIG_H__ */
