
#ifndef _K8TEMP_LIBPCI_H_
#define _K8_TEMP_LIBPCI_H

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <pci.h>

#define _PATH_DEVPCI "/dev/pci0"

typedef struct {
	uint8_t pc_bus;
	uint8_t pc_dev;
	uint8_t pc_func;
} k8_pcidev;

#endif
