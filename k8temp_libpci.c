/*
 * k8temp functions for scanning PCI devices and reading/writing
 * to registers on them, via NetBSD-style libpci and /dev/pci0
 *
 * Untested, unfinished. Beware of dog.
 */

#include "k8temp_libpci.h"

int fd;

void k8_pci_init()
{
	fd = open(_PATH_DEVPCI, O_RDWR, 0);

	if (fd < 0)
		err(EXIT_FAILURE, "open(\"%s\")", _PATH_DEVPCI);
}

void k8_pci_close()
{
	if (fd) close(fd);
}

int k8_pci_vendor_device_list(int vendor_id, int device_id, k8_pcidev devs[], int maxdev)
{
	int matches = -1;
	u_int8_t dev,func;
	int pcireg;
	k8_pcidev sel;
	bzero(&sel, sizeof(k8_pcidev));
	sel.pc_bus = 0;
	for (dev=0; dev < 32; dev++)
	{
		sel.pc_dev = dev;
		for (func=0; func < 8; func++)
		{
			sel.pc_func = func;
			if (k8_pci_read_word(sel, 0, &pcireg) &&
			   (pcireg & 0xffff) == vendor_id &&
			   ((pcireg >> 16) & 0xffff) == device_id &&
			   matches < maxdev)
			{
				memcpy(&devs[++matches], &sel, sizeof(k8_pcidev));
			}
		}
	}
	return(matches);
}

int k8_pci_read(k8_pcidev dev, int offset, int *data, int width)
{
	if (pcibus_conf_read(fd, dev.pc_bus, dev.pc_dev, dev.pc_func, offset, (uint32_t *)&data) < 0)
	{
		warn("Register read %x, %d bytes failed", offset, width);
		return(0);
	}

	switch (width)
	{
	case 1:
		*data = *data & 0xff;
		break;
	case 4:
		break;
	default:
		warnx("Width %d register reads unsupported", width);
	}
	return(width);
}

int k8_pci_read_byte(k8_pcidev dev, int offset, int *data)
{
	return(k8_pci_read(dev, offset, data, 1));
}

int k8_pci_read_word(k8_pcidev dev, int offset, int *data)
{
	return(k8_pci_read(dev, offset, data, 4));
}

int k8_pci_write(k8_pcidev dev, int offset, int data, int width)
{
	/* XXX: Note, libpci doesn't support <4 byte writes.
	 * Luckily the upper 24 bits of Thermtrip is read-only, so it doesn't
	 * matter, but we should take care if we end up writing anywhere else.
	 */
	if (pcibus_conf_write(fd, dev.pc_bus, dev.pc_dev, dev.pc_func, offset, data) < 0)
	{
		warn("Register write %x, %d bytes failed", offset, width);
		return(0);
	}

	return(width);
}

int k8_pci_write_byte(k8_pcidev dev, int offset, int data)
{
	return(k8_pci_write(dev, offset, data, 1));
}

int k8_pci_write_word(k8_pcidev dev, int offset, int data)
{
	return(k8_pci_write(dev, offset, data, 4));
}

