
#ifndef _K8TEMP_H_
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#define _PATH_DEVPCI "/dev/pci"
#define PCI_VENDOR_ID_AMD              0x1022
#define PCI_DEVICE_ID_AMD_K8_MISC_CTRL 0x1103

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

#define MAX_CPU    32
#define MAX_CORE    2
#define MAX_SENSOR  2

#define OFFSET_MAX 11
#define TEMP_MIN -49
#define TEMP_ERR -255

#define CPUID_EXTENDED 0x80000000
#define CPUID_POWERMGT 0x80000007

void check_cpuid(void);
int get_temp(int fd, struct pcisel dev, int core, int sensor);
int main(int argc, char *argv[]);
void usage(int exit_code);
void version(void);

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
#endif
