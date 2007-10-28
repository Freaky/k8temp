/*
 * k8temp -- AMD K8 (AMD64, Opteron) on-die thermal sensor reader for FreeBSD.
 * Should work in DfBSD, since it has pretty much the same /dev/pci
 * OpenBSD feasable with USER_PCICONF support, but I don't see PCIOCGETCONF in pci(4)
 * NetBSD has -lpci, can be supported with a bit of effort.
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
#include <string.h>

#define _PATH_DEVPCI "/dev/pci"
#define PCI_VENDOR_ID_AMD              0x1022
#define PCI_DEVICE_ID_AMD_K8_MISC_CTRL 0x1103

int debug = 0;
int correct = 0;

void check_cpuid(void);
int get_temp(int fd, struct pcisel dev, int core, int sensor);
int main(int argc, char *argv[]);
void usage(int exit_code);
void version(void);

void usage(int exit_code)
{
	fprintf((exit_code == EXIT_SUCCESS ? stdout : stderr), "%s\n%s\n%s\n%s\n%s\n",
			"usage: k8temp [-dc | -v | -h] [cpu[:core[:sensor]] ...]",
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
#define THERMTRIP(val)  ((val) & 1)

#define OFFSET_MAX 11
#define TEMP_MIN -49
#define TEMP_ERR -255

#define CPUID_EXTENDED 0x80000000
#define CPUID_POWERMGT 0x80000007

const char *advPwrMgtFlags[] = {
	"Temperature sensor",
	"Frequency ID control",
	"Voltage ID control",
	"THERMTRIP support",
	"HW Thermal control",
	"SW Thermal control",
	"100MHz multipliers",
	"HW P-State control",
	"TSC Invariant",
	NULL,
};

void check_cpuid(void)
{
	unsigned int vendor[3];
	unsigned int maxeid,cpuid,pwrmgt,unused;
	int i;

	asm("cpuid": "=a" (unused), "=b" (vendor[0]), "=c" (vendor[2]), "=d" (vendor[1]) : "a" (0));
	asm("cpuid": "=a" (cpuid), "=b" (unused), "=c" (unused), "=d" (unused) : "a" (1));

	if (debug)
		fprintf(stderr, "CPUID: Vendor: %.12s, Id=0x%x Model=%d Family=%d Stepping=%d\n",
		        (char *)vendor, cpuid, (cpuid >> 4) & 0xf, (cpuid >> 8) & 0xf, cpuid & 0xf);

	if (0 != memcmp((char *)&vendor, "AuthenticAMD", 12))
		errx(EXIT_FAILURE, "Only AMD CPU's are supported by k8temp");

	asm("cpuid": "=a" (maxeid), "=b" (unused), "=c" (unused), "=d" (unused) : "a" (CPUID_EXTENDED));

	if (maxeid >= CPUID_POWERMGT)
	{
		asm("cpuid": "=a" (unused), "=b" (unused), "=c" (unused), "=d" (pwrmgt) : "a" (CPUID_POWERMGT));
		if (debug)
		{
			/* overkill \o/ */
			fprintf(stderr, "Advanced Power Management=0x%x\n", pwrmgt);
			i = 0;
			do
			{
				fprintf(stderr, " %20s: %s\n", advPwrMgtFlags[i],
				        (pwrmgt >> i) & 1 ? "Yes" : "No");
			}
			while (advPwrMgtFlags[++i]);
		}
		if (!(pwrmgt & 1))
			errx(EXIT_FAILURE, "Thermal sensor support not reported in CPUID");
	}
	else
		errx(EXIT_FAILURE, "CPU lacks Advanced Power Management support");

	/* Linux only checks these 3 */
	if (cpuid == 0xf40 || cpuid == 0xf50 || cpuid == 0xf51)
		errx(EXIT_FAILURE, "This CPU stepping does not support thermal sensors.");
}

int get_temp(int fd, struct pcisel dev, int core, int sensor)
{
	static int thermtp = 0;
	struct pci_io ctrl;
	unsigned int reg;
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

	if (THERMTRIP(reg) && !thermtp)
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

#define MAX_CPU    32
#define MAX_CORE    2
#define MAX_SENSOR  2

int main(int argc, char *argv[])
{
	struct pci_conf_io pc;
	struct pci_match_conf pat;
	struct pci_conf conf[MAX_CPU], *p;
	int fd;
	unsigned int cpu,core,sensor;
	int temp;
	int exit_code = EXIT_FAILURE;
	int opt;
	char selections[MAX_CPU][MAX_CORE][MAX_SENSOR];

	bzero(&selections, sizeof(selections));

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

	int selcount,selected = 0;
	for (int i = optind; i < argc; i++)
	{
		selcount = sscanf(argv[i], "%u:%u:%u", &cpu, &core, &sensor);
		selected += selcount;
		if (cpu > MAX_CPU - 1) errx(EXIT_FAILURE, "CPU selector %d out of range 0-%d", cpu, MAX_CPU - 1);
		if (core > MAX_CORE - 1) errx(EXIT_FAILURE, "Core selector %d out of range 0-%d", cpu, MAX_CORE - 1);
		if (sensor > MAX_SENSOR - 1) errx(EXIT_FAILURE, "Sensor selector %d out of range 0-%d", cpu, MAX_SENSOR - 1);
		switch (selcount)
		{
		case 1:
			memset(selections[cpu], 1, sizeof(selections[cpu]));
			break;
		case 2:
			memset(selections[cpu][core], 1, sizeof(selections[cpu][core]));
			break;
		case 3:
			selections[cpu][core][sensor] = 1;
			break;
		case 0:
		default:
			warnx("Illegal selector: %s", argv[i]);
			usage(EXIT_FAILURE);
		}
	}
	if (0 == selected)
		memset(selections, 1, sizeof(selections));

	check_cpuid();

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
				if (selections[cpu][core][sensor])
				{
					temp = get_temp(fd, p->pc_sel, core, sensor);
					if (temp > TEMP_MIN + OFFSET_MAX)
					{
						printf("CPU %d Core %d Sensor %d: %dc\n", cpu, core, sensor, temp);
						exit_code = EXIT_SUCCESS;
					}
				}
			}
		}
		cpu++;
	}

	close(fd);
	exit(exit_code);
}

