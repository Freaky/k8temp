
#ifndef _K8TEMP_H_

#define PCI_VENDOR_ID_AMD               0x1022
#define PCI_DEVICE_ID_AMD_K8_MISC_CTRL  0x1103
#define PCI_DEVICE_ID_AMD_K10_MISC_CTRL 0x1203

/*
 * BIOS and Kernel Developer's Guide (BKDG) For AMD Family 10h Processors
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/31116.pdf
 *
 * These aren't used anywhere yet.
 */
#define K10_THERM_REG      0xa4
#define K10_THERMTRIP_REG  0xe4
#define K10_CURTMP(val)    (((val) >> 21) & 0xfff)
#define K10_THERMTRIP(val) ((val >> 1) & 1)

/*
 * See section 4.6.23, Thermtrip Status Register:
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/32559.pdf
 */
#define THERM_REG   0xe4
#define SEL_CORE    (1 << 2) /* ThermSenseCoreSel */
#define SEL_SENSOR  (1 << 6) /* ThermSenseSel */
#define CURTMP(val)      (((val) >> 16) & 0xff)
#define TJOFFSET(val)    (((val) >> 24) & 0xf)
#define DIODEOFFSET(val) (((val) >> 8) & 0x3f)
#define THERMTRIP(val)   ((val) & 1)

#define MAX_CPU    32
#define MAX_CORE    2
#define MAX_SENSOR  2

#define OFFSET_MAX 11
#define TEMP_MIN  -49
#define TEMP_ERR -255

#define CPUID_EXTENDED 0x80000000
#define CPUID_POWERMGT 0x80000007

static void check_cpuid(void);
static int get_temp(k8_pcidev dev, int core, int sensor);
static void usage(int exit_code);
static void version(void);
int main(int argc, char *argv[]);

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
