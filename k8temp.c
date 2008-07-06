/*-
 * Copyright (c) 2007-2008 Thomas Hurst <tom@hur.st>
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

#define K8TEMP_VERSION "0.4.0"

/*
 * k8temp -- AMD K8 (AMD64, Opteron) on-die thermal sensor reader for FreeBSD.
 *
 * DragonFlyBSD should work fine, since it has pretty much the same /dev/pci.
 * OpenBSD should work with -DWITHOUT_PCIOCGETCONF using a kernel with USER_PCI.
 * NetBSD might work with -DWITH_LIBPCI, but it's only been vaguely build tested.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <unistd.h>

#include "k8temp_pci.h"
#include "k8temp.h"

static int debug = 0;
static int k10   = 0;

static void
usage(int exit_code)
{
	fprintf((exit_code == EX_OK ? stdout : stderr),
	        "%s\n%s\n%s\n%s\n%s\n",
	        "usage: k8temp [-nd] [cpu[:core[:sensor]] ...] | [-v | -h]",
	        "  -d    Dump debugging info",
	        "  -h    Display this help text",
	        "  -n    Only display values",
	        "  -v    Display version information");
	exit(exit_code);
}

static void
version(void)
{
	printf("k8temp v%s\nCopyright 2007-2008 Thomas Hurst <tom@hur.st>\n", K8TEMP_VERSION);
	exit(EX_OK);
}

static void
check_cpuid(void)
{
	unsigned int vendor[3];
	unsigned int maxeid,cpuid,pwrmgt,unused;
	int i, model, extmodel, family, extfamily, stepping;

	asm("cpuid": "=a" (unused), "=b" (vendor[0]), "=c" (vendor[2]), "=d" (vendor[1]) : "a" (0));
	asm("cpuid": "=a" (cpuid), "=b" (unused), "=c" (unused), "=d" (unused) : "a" (1));

	model = (cpuid >> 4) & 0xf;
	family = (cpuid >> 8) & 0xf;
	stepping = cpuid & 0xf;

	extmodel = (cpuid >> 16) & 0xf;
	extfamily = (cpuid >> 20) & 0xff;

	k10 = (extfamily == 1 && family == 0xf);

	if (debug)
		fprintf(stderr, "CPUID: Vendor: %.12s, 0x%x: Model=%x%x Family=%x+%x "
		                "Stepping=%d\n",
		        (char *)vendor, cpuid, extmodel, model, family, extfamily, stepping);

	if (0 != memcmp((char *)&vendor, "AuthenticAMD", (size_t) 12))
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

static int
get_temp_k10(k8_pcidev dev, int core, int sensor)
{
	static int thermtp = 0;
	uint32_t temp, therm;

	(void)core;
	(void)sensor;

	if (core > 0 || sensor > 0)
		return(TEMP_ERR);

	if (!k8_pci_read_word(dev, K10_THERM_REG, &temp))
		return(TEMP_ERR);

	if (debug)
		fprintf(stderr, "Temp=%x\n", temp);


	if (!k8_pci_read_word(dev, K10_THERMTRIP_REG, &therm))
		return(TEMP_ERR);

	if (debug)
		fprintf(stderr, "ThermTrip=%x\n", therm);

	if (K10_THERMTRIP(therm) && !thermtp)
	{
		fprintf(stderr, "Thermal trip bit set, system overheating?\n");
		thermtp = 1;
	}

	return(K10_CURTMP(temp) / 8);
}

static int
get_temp(k8_pcidev dev, int core, int sensor)
{
	static int thermtp = 0;
	uint32_t ctrl,therm;

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
	int cpucount,cpu,core,sensor;
	int temp;
	int exit_code = EX_UNAVAILABLE;
	int opt;
	int value_only = 0;
	int i;
	char selections[MAX_CPU][MAX_CORE][MAX_SENSOR];
	int selcount,selected = 0;

	while ((opt = getopt(argc, argv, "dhnv")) != -1)
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			value_only = 1;
			break;
		case 'v':
			version();
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
		if (selcount > 0 && cpu >= MAX_CPU)
			errx(EX_USAGE, "CPU selector %d out of range 0-%d", cpu, MAX_CPU - 1);
		if (selcount > 1 && core >= MAX_CORE)
			errx(EX_USAGE, "Core selector %d out of range 0-%d", cpu, MAX_CORE - 1);
		if (selcount > 2 && sensor >= MAX_SENSOR)
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

	int devid = k10 ? PCI_DEVICE_ID_AMD_K10_MISC_CTRL : PCI_DEVICE_ID_AMD_K8_MISC_CTRL;

	cpucount = k8_pci_vendor_device_list(PCI_VENDOR_ID_AMD, devid,
	                                     devs, MAX_CPU);
	for (cpu=0; cpu <= cpucount; cpu++)
	{
		for (core = 0; core < MAX_CORE; core++)
		{
			for (sensor = 0; sensor < MAX_SENSOR; sensor++)
			{
				if (selected > 0 && !selections[cpu][core][sensor])
					continue;

				if (k10)
					temp = get_temp_k10(devs[cpu], core, sensor);
				else
					temp = get_temp(devs[cpu], core, sensor);

				if (temp > TEMP_MIN)
				{
					if (value_only)
						printf("%d\n", temp);
					else
						printf("CPU %d Core %d Sensor %d: %dc\n", cpu, core, sensor, temp);
					exit_code = EX_OK;
				}
			}
		}
	}

	k8_pci_close();
	exit(exit_code);
}

