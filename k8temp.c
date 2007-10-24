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

#define K8TEMP_VERSION "0.2.0"

/*
 * Usage: gcc -o k8temp k8temp.c && sudo ./k8temp
 *
 * Tested on FreeBSD 6-STABLE/amd64 on a dual dual core Opteron 275:
 * CPU 0 Core 0 Sensor 0: 44c
 * CPU 0 Core 1 Sensor 0: 45c
 * CPU 1 Core 0 Sensor 0: 41c
 * CPU 1 Core 1 Sensor 0: 41c
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

int debug = 0;
int correct = 0;

void usage(int exit_code)
{
	fprintf((exit_code == EXIT_SUCCESS ? stdout : stderr), "%s\n%s\n%s\n%s\n%s\n",
			"usage: k8temp [-dn | -v | -h]",
			"  -v    Display version information",
			"  -h    Display this help text",
			"  -d    Dump debugging info",
			"  -c    Apply diode offset correction");
	exit(exit_code);
}

void version(void)
{
	printf("k8temp v%s\nCopyright 2007 Thomas Hurst <tom@hur.st>\n", K8TEMP_VERSION);
	exit(EXIT_SUCCESS);
}

/*
 * See section 4.6.23, Thermtrip Status Register:
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/32559.pdf
 */
#define THERM_REG   0xe4
#define SEL_CORE    (1 << 2) /* ThermSenseCoreSel */
#define SEL_SENSOR  (1 << 6) /* ThermSenseSel */
#define CURTMP(val)     (((val) >> 16) & 0xff)
#define TJOFFSET(val)   (((val) >> 24) & 0xf)
#define DIODEOFFSET(val)   (((val) >> 8) & 0x3f)

#define OFFSET_MAX 11
#define TEMP_MIN -49
#define TEMP_ERR -255

int get_temp(int fd, struct pcisel dev, int core, int sensor)
{
	static int thermtp = 0;
	struct pci_io ctrl;
	int reg;
	bzero(&ctrl, sizeof(ctrl));

	ctrl.pi_sel = dev;
	ctrl.pi_reg = THERM_REG;
	ctrl.pi_width = 1;
	if (ioctl(fd, PCIOCREAD, &ctrl) == -1)
	{
		perror("ThermTrip register read failed");
		return(TEMP_ERR);
	}
	reg = ctrl.pi_data;

	if (core == 1) /* CPU0 has the bit set according to datasheet */
		reg &= ~SEL_CORE;
	else if (core == 0)
		reg |= SEL_CORE;
	else return(TEMP_ERR);

	if (sensor == 0)
		reg &= ~SEL_SENSOR;
	else if (sensor == 1)
		reg |= SEL_SENSOR;
	else return(TEMP_ERR);

	ctrl.pi_data = reg;
	if (ioctl(fd, PCIOCWRITE, &ctrl) == -1)
	{
		perror("ThermTrip register write failed");
		return(TEMP_ERR);
	}

	ctrl.pi_width = 4;
	if (ioctl(fd, PCIOCREAD, &ctrl) == -1)
	{
		perror("ThermTrip register read failed");
		return(TEMP_ERR);
	}

	/* verify the selection took */
	if ((reg & (SEL_CORE|SEL_SENSOR)) != (ctrl.pi_data & (SEL_CORE|SEL_SENSOR)))
		return(TEMP_ERR);

	/* Bit 1 is Thermtrip */
	if (reg & 0x1 && !thermtp)
	{
		fprintf(stderr, "Thermal trip bit set, system overheating?\n");
		thermtp = 1;
	}

	reg = ctrl.pi_data;
	if (debug)
		fprintf(stderr, "Thermtrip=0x%08x (CurTmp=0x%02x (%dc) TjOffset=0x%02x DiodeOffset=0x%02x (%dc))\n",
		        reg, CURTMP(reg), CURTMP(reg) + TEMP_MIN, TJOFFSET(reg),
		        DIODEOFFSET(reg), OFFSET_MAX - DIODEOFFSET(reg));

	if (correct && DIODEOFFSET(reg) > 0)
		return((CURTMP(reg) + TEMP_MIN) + (OFFSET_MAX - DIODEOFFSET(reg)));
	else
		return(CURTMP(reg) + TEMP_MIN);
}


int main(int argc, char *argv[])
{
	struct pci_conf_io pc;
	struct pci_match_conf pat;
	struct pci_conf conf[255], *p;
	int fd;
	int cpu,core,sensor,temp;
	int exit_code = EXIT_FAILURE;
	int opt;

	while ((opt = getopt(argc, argv, "dvch")) != -1)
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		case 'v':
			version();
			exit(EXIT_SUCCESS);
		case 'c':
			correct = 1;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		default:
			usage(EXIT_FAILURE);
		}

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
		exit_code = EXIT_FAILURE;
	}
	else for (p = conf; p < &conf[pc.num_matches]; p++)
	{
		if (debug)
			fprintf(stderr, "Probe device: %d:%d:%d\n",
			        p->pc_sel.pc_bus, p->pc_sel.pc_dev, p->pc_sel.pc_func);
		for (core = 0; core < 2; core++)
		{
			for (sensor = 0; sensor < 2; sensor++)
			{
				temp = get_temp(fd, p->pc_sel, core, sensor);
				if (temp > TEMP_MIN + OFFSET_MAX)
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

