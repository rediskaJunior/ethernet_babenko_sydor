#ifndef _MDNS_H_
#define _MDNS_H_

#include <stdint.h>
#include "main.h"

#define MDNS_SOCKET     2

void mdns_init(const char* hostname);
void mdns_process(void);

#endif
