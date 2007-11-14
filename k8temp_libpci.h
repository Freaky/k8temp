
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

void k8_pci_init(void);
void k8_pci_close(void);

int k8_pci_vendor_device_list(uint32_t vendor_id, uint32_t device_id, k8_pcidev devs[], int max_dev);

int k8_pci_read(k8_pcidev dev, int offset, uint32_t *data, int width);
int k8_pci_read_byte(k8_pcidev dev, int offset, uint32_t *data);
int k8_pci_read_word(k8_pcidev dev, int offset, uint32_t *data);

int k8_pci_write(k8_pcidev dev, int offset, uint32_t data, int width);
int k8_pci_write_byte(k8_pcidev dev, int offset, uint32_t data);
int k8_pci_write_word(k8_pcidev dev, int offset, uint32_t data);

#endif
