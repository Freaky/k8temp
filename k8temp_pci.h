
#ifdef WITH_LIBPCI
#include "k8temp_libpci.h"
#else
#include "k8temp_devpci.h"
#endif

/* PCI access functions */
void k8_pci_init(void);
void k8_pci_close(void);

int k8_pci_vendor_device_list(uint32_t vendor_id, uint32_t device_id, k8_pcidev devs[], int max_dev);

int k8_pci_read(k8_pcidev dev, int offset, uint32_t *data, int width);
int k8_pci_read_byte(k8_pcidev dev, int offset, uint32_t *data);
int k8_pci_read_word(k8_pcidev dev, int offset, uint32_t *data);

int k8_pci_write(k8_pcidev dev, int offset, uint32_t data, int width);
int k8_pci_write_byte(k8_pcidev dev, int offset, uint32_t data);
int k8_pci_write_word(k8_pcidev dev, int offset, uint32_t data);

