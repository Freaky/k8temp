/*
 * k8temp -- AMD K8 (AMD64, Opteron) on-die thermal sensor reader for FreeBSD.
 * Might work in the other BSD's if they have a compatible /dev/pci
 * Single core systems may get a bogus second core reading.
 *
 * Copyright (c) 2007 Thomas Hurst <tom@hur.st>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define K8TEMP_VERSION "0.1.1"

/*
 * Usage: gcc -o k8temp k8temp.c && sudo ./k8temp
 *
 * Tested on FreeBSD 6-STABLE/amd64 on a dual dual core Opteron 275:
 * CPU 0 Core 0 Sensor 0: 50c
 * CPU 1 Core 0 Sensor 0: 48c
 *
 * 6.1-RELEASE/amd64 on a dual core Opteron 175:
 * CPU 0 Core 0 Sensor 0: 36c
 * CPU 0 Core 1 Sensor 0: 32c
 *
 * 6.2-RELEASE/amd64 on a dual single core Opteron 248:
 * CPU 0 Core 1 Sensor 0: 32c
 * CPU 1 Core 1 Sensor 0: 38c
 * 
 * 6.2-RELEASE/amd64 on a dual dual core Opteron 2216:
 * CPU 0 Core 0 Sensor 0: 46c
 * CPU 0 Core 1 Sensor 0: 46c
 * CPU 1 Core 0 Sensor 0: 38c
 * CPU 1 Core 1 Sensor 0: 38c
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/pciio.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define _PATH_DEVPCI "/dev/pci"
#define PCI_VENDOR_ID_AMD              0x1022
#define PCI_DEVICE_ID_AMD_K8_MISC_CTRL 0x1103

/*
 * See section 2.6.23, Thermtrip Status Register:
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/32559.pdf
 */
#define CTRL_REG    0xe4
#define SEL_CORE    (1 << 2) /* ThermSenseCoreSel */
#define SEL_SENSOR  (1 << 6) /* ThermSenseSel */
#define CURTMP(val) (((val) >> 16) & 0xff)

int get_temp(int fd, struct pcisel dev, int core, int sensor)
{
	char reg;
	struct pci_io ctrl;
	int tcontrol,curtmp,tjoffset;
	bzero(&ctrl, sizeof(ctrl));

	ctrl.pi_sel = dev;
	ctrl.pi_reg = CTRL_REG;
	ctrl.pi_width = 1;
	if (ioctl(fd, PCIOCREAD, &ctrl) == -1)
	{
		perror("ThermTrip register read failed");
		return(-1);
	}
	reg = ctrl.pi_data;

	if (core == 1) /* CPU0 has the bit set according to datasheet */
		reg &= ~SEL_CORE;
	else if (core == 0)
		reg |= SEL_CORE;
	else return(-1);

	if (sensor == 0)
		reg &= ~SEL_SENSOR;
	else if (sensor == 1)
		reg |= SEL_SENSOR;
	else return(-1);

	ctrl.pi_data = reg;
	if (ioctl(fd, PCIOCWRITE, &ctrl) == -1)
	{
		perror("ThermTrip register write failed");
		return(-1);
	}

	/* verify the selection took */
	if (ioctl(fd, PCIOCREAD, &ctrl) == -1)
	{
		perror("ThermTrip register read failed");
		return(-1);
	}
	if ((reg & (SEL_CORE|SEL_SENSOR)) != (ctrl.pi_data & (SEL_CORE|SEL_SENSOR)))
		return(0);

	ctrl.pi_width = 4;
	if (ioctl(fd, PCIOCREAD, &ctrl) == -1)
	{
		perror("ThermTrip register read failed");
		return(-1);
	}
	/*
	 * TODO: See about correcting with TjOffset
	 */
	return(CURTMP(ctrl.pi_data) - 49);
}


int main(int argc, char *argv[])
{
	struct pci_conf_io pc;
	struct pci_match_conf pat;
	struct pci_conf conf[255], *p;
	int fd;
	int cpu,core,sensor,cores,temp;
	int exit_code = EXIT_FAILURE;

	fd = open(_PATH_DEVPCI, O_RDWR, 0);

	if (fd < 0)
		err(EXIT_FAILURE, "open(\"%s\")", _PATH_DEVPCI);

	bzero(&pc, sizeof(struct pci_conf_io));
	bzero(&pat, sizeof(struct pci_match_conf));

	pat.pc_vendor = PCI_VENDOR_ID_AMD;
	pat.pc_device = PCI_DEVICE_ID_AMD_K8_MISC_CTRL;
	pat.flags = PCI_GETCONF_MATCH_VENDOR | PCI_GETCONF_MATCH_DEVICE;
	pc.patterns = &pat;
	pc.num_patterns = 1;
	pc.pat_buf_len = sizeof(pat);

	pc.match_buf_len = sizeof(conf);
	pc.matches = conf;

	cpu = 0; /* XXX: Are the PCI devices going to come out in CPU-number order? */
	if (ioctl(fd, PCIOCGETCONF, &pc) == -1 || pc.status == PCI_GETCONF_ERROR)
	{
		perror("ioctl(PCIOCGETCONF)");
		close(fd);
		exit(EXIT_FAILURE);
	}

	for (p = conf; p < &conf[pc.num_matches]; p++)
	{
		for (core = 0; core < 2; core++)
		{
			for (sensor = 0; sensor < 2; sensor++)
			{
				temp = get_temp(fd, p->pc_sel, core, sensor);
				if (temp > 0)
				{
					printf("CPU %d Core %d Sensor %d: %dc\n", cpu, core, sensor, temp);
					exit_code = EXIT_SUCCESS;
				}
			}
		}
		cpu++;
	}

	close(fd);
	exit(exit_code);
}

