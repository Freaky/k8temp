/*
 * k8temp functions for scanning PCI devices and reading/writing
 * to registers on them, via FreeBSD-style /dev/pci
 *
 * Supported on FreeBSD and DragonFlyBSD.
 *
 * OpenBSD should work with USER_PCI and -DWITHOUT_PCIOCGETCONF
 *
 * NetBSD needs libpci, using pcibus_conf_read()/write()
 *
 * Linux can probably work with /proc/pci or something?
 *
 * No idea about Solaris.
 */

#include "k8temp_devpci.h"

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
#ifndef WITHOUT_PCIOCGETCONF
	struct pci_conf_io pc;
	struct pci_match_conf pat;
	struct pci_conf conf[255], *p;

	bzero(&pc, sizeof(struct pci_conf_io));
	bzero(&pat, sizeof(struct pci_match_conf));

	pat.pc_vendor = vendor_id;
	pat.pc_device = device_id;
	pat.flags = PCI_GETCONF_MATCH_VENDOR | PCI_GETCONF_MATCH_DEVICE;
	pc.patterns = &pat;
	pc.num_patterns = 1;
	pc.pat_buf_len = sizeof(pat);

	pc.match_buf_len = sizeof(conf);
	pc.matches = conf;

	if (ioctl(fd, PCIOCGETCONF, &pc) == -1 || pc.status == PCI_GETCONF_ERROR)
		return(-1);

	for (p = conf; p < &conf[pc.num_matches]; p++)
	{
		if (matches < maxdev)
			memcpy(&devs[++matches], &p->pc_sel, sizeof(k8_pcidev));
	}
#else
	/* should work with OpenBSD's USER_PCI */
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
			if (k8_pci_read_word(sel, 0, &pcireg))
			{
				if ((pcireg & 0xffff) == vendor_id &&
				   ((pcireg >> 16) & 0xffff) == device_id &&
				   matches < maxdev)
				{
					memcpy(&devs[++matches], &sel, sizeof(k8_pcidev));
				}
			}
		}
	}
#endif
	return(matches);
}

int k8_pci_read(k8_pcidev dev, int offset, int *data, int width)
{
	struct pci_io ctrl;
	bzero(&ctrl, sizeof(ctrl));

	ctrl.pi_sel = dev;
	ctrl.pi_reg = offset;
	ctrl.pi_width = width;

	if (ioctl(fd, PCIOCREAD, &ctrl) == -1)
	{
		warn("Register read %x, %d bytes failed", offset, width);
		return(0);
	}

	*data = ctrl.pi_data;
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
	struct pci_io ctrl;
	bzero(&ctrl, sizeof(ctrl));

	ctrl.pi_sel = dev;
	ctrl.pi_reg = offset;
	ctrl.pi_width = width;
	ctrl.pi_data = data;

	if (ioctl(fd, PCIOCWRITE, &ctrl) == -1)
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

