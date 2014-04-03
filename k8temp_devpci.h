
#ifndef _K8TEMP_DEVPCI_H_
#define _K8TEMP_DEVPCI_H_

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/pciio.h>

#define _PATH_DEVPCI "/dev/pci"

typedef struct pcisel k8_pcidev;

#endif
