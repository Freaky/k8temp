/*-
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

#define K8TEMP_VERSION "0.3.0pre2"

/*
 * k8temp -- AMD K8 (AMD64, Opteron) on-die thermal sensor reader for FreeBSD.
 *
 * DragonFlyBSD should work fine, since it has pretty much the same /dev/pci.
 * OpenBSD should work with -DWITHOUT_PCIOCGETCONF using a kernel with USER_PCI.
 * NetBSD might work with -DWITH_LIBPCI, but it's only been vaguely built tested.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <unistd.h>

#ifdef WITH_LIBPCI
#include "k8temp_libpci.h"
#else
#include "k8temp_devpci.h"
#endif

#include "k8temp.h"

int debug = 0;

void
usage(int exit_code)
{
	fprintf((exit_code == EX_OK ? stdout : stderr), "%s\n%s\n%s\n%s\n%s\n",
	        "usage: k8temp [-nd | -v | -h] [cpu[:core[:sensor]] ...]",
	        "  -d    Dump debugging info",
	        "  -h    Display this help text",
	        "  -n    Only display number or UNKNOWN",
	        "  -v    Display version information");
	exit(exit_code);
}

void
version(void)
{
	printf("k8temp v%s\nCopyright 2007 Thomas Hurst <tom@hur.st>\n", K8TEMP_VERSION);
	exit(EX_OK);
}

void
check_cpuid(void)
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
		errx(EX_UNAVAILABLE, "Only AMD CPU's are supported by k8temp");

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
			errx(EX_UNAVAILABLE, "Thermal sensor support not reported in CPUID");
	}
	else
		errx(EX_UNAVAILABLE, "CPU lacks Advanced Power Management support");

	/* Linux only checks these 3 */
	if (cpuid == 0xf40 || cpuid == 0xf50 || cpuid == 0xf51)
		errx(EX_UNAVAILABLE, "This CPU stepping does not support thermal sensors.");
}

int
get_temp(k8_pcidev dev, int core, int sensor)
{
	static int thermtp = 0;
	unsigned int ctrl,therm;

	if (!k8_pci_read_byte(dev, THERM_REG, &ctrl))
		return(TEMP_ERR);

	if (core == 1) /* CPU0 has the bit set according to datasheet */
		ctrl &= ~SEL_CORE;
	else if (core == 0)
		ctrl |= SEL_CORE;
	else return(TEMP_ERR);

	if (sensor == 0)
		ctrl &= ~SEL_SENSOR;
	else if (sensor == 1)
		ctrl |= SEL_SENSOR;
	else return(TEMP_ERR);

	if (!k8_pci_write_byte(dev, THERM_REG, ctrl))
		return(TEMP_ERR);

	if (!k8_pci_read_word(dev, THERM_REG, &therm))
		return(TEMP_ERR);

	/* verify the selection took */
	if ((ctrl & (SEL_CORE|SEL_SENSOR)) != (therm & (SEL_CORE|SEL_SENSOR)))
		return(TEMP_ERR);

	if (THERMTRIP(therm) && !thermtp)
	{
		fprintf(stderr, "Thermal trip bit set, system overheating?\n");
		thermtp = 1;
	}

	if (debug)
		fprintf(stderr, "Thermtrip=0x%08x (CurTmp=0x%02x (%dc) TjOffset=0x%02x DiodeOffset=0x%02x (%dc))\n",
		        therm, CURTMP(therm), CURTMP(therm) + TEMP_MIN, TJOFFSET(therm),
		        DIODEOFFSET(therm), OFFSET_MAX - DIODEOFFSET(therm));

	return(CURTMP(therm) + TEMP_MIN);
}

int
main(int argc, char *argv[])
{
	k8_pcidev devs[MAX_CPU];
	unsigned int cpucount,cpu,core,sensor;
	int temp;
	int exit_code = EX_UNAVAILABLE;
	int opt;
	int value_only = 0;
	int i;
	char selections[MAX_CPU][MAX_CORE][MAX_SENSOR];
	int selcount,selected = 0;

	while ((opt = getopt(argc, argv, "dvchn")) != -1)
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		case 'v':
			version();
			exit(EX_OK);
		case 'n':
			value_only = 1;
			break;
		case 'h':
			usage(EX_OK);
		default:
			usage(EX_USAGE);
		}

	bzero(&selections, sizeof(selections));
	for (i = optind; i < argc; i++)
	{
		selcount = sscanf(argv[i], "%u:%u:%u", &cpu, &core, &sensor);
		selected += selcount;
		if (cpu >= MAX_CPU)
			errx(EX_USAGE, "CPU selector %d out of range 0-%d", cpu, MAX_CPU - 1);
		if (core >= MAX_CORE)
			errx(EX_USAGE, "Core selector %d out of range 0-%d", cpu, MAX_CORE - 1);
		if (sensor >= MAX_SENSOR)
			errx(EX_USAGE, "Sensor selector %d out of range 0-%d", cpu, MAX_SENSOR - 1);
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
			usage(EX_USAGE);
		}
	}

	check_cpuid();
	k8_pci_init();

	cpu = 0;
	cpucount = k8_pci_vendor_device_list(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_K8_MISC_CTRL,
	                                     devs, MAX_CPU);
	for (cpu=0; cpu <= cpucount; cpu++)
	{
		for (core = 0; core < MAX_CORE; core++)
		{
			for (sensor = 0; sensor < MAX_SENSOR; sensor++)
			{
				if (selected > 0 && !selections[cpu][core][sensor])
					continue;

				temp = get_temp(devs[cpu], core, sensor);
				if (temp > TEMP_MIN)
				{
					if (value_only)
						printf("%d\n", temp);
					else
						printf("CPU %d Core %d Sensor %d: %dc\n", cpu, core, sensor, temp);
					exit_code = EX_OK;
				}
				else if (value_only)
				{
					printf("UNKNOWN\n");
				}
			}
		}
	}

	k8_pci_close();
	exit(exit_code);
}

