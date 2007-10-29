
#include "k8temp_devpci.c"

#define PCI_VENDOR_ID_AMD              0x1022
#define PCI_DEVICE_ID_AMD_K8_MISC_CTRL 0x1103


/*
 *  * See section 4.6.23, Thermtrip Status Register:
 *   * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/32559.pdf
 *    */
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

int get_temp(k8_pcidev_t dev, int core, int sensor)
{
	unsigned int ctrl, therm;

	if (!k8_pci_read_byte(dev, THERM_REG, &ctrl)) return(TEMP_ERR);

	if (core) ctrl &= ~SEL_CORE; // etc
	else ctrl |= SEL_CORE;

	if (sensor) ctrl |= SEL_SENSOR;
	else ctrl &= ~SEL_SENSOR;

	if (!k8_pci_write_byte(dev, THERM_REG, ctrl)) return(TEMP_ERR);
	if (!k8_pci_read_word(dev, THERM_REG, &therm)) return(TEMP_ERR);

	if ((therm & (SEL_CORE|SEL_SENSOR)) != (ctrl & (SEL_CORE|SEL_SENSOR)))
		return(TEMP_ERR);

	return(CURTMP(therm) + TEMP_MIN);
}


int main(int argc, char *argv[])
{
	// ...

	int cpucount,cpu,core,sensor,temp;
	k8_pcidev_t dev, devs[255];
	k8_pci_init();

	cpucount = k8_pci_vendor_device_list(PCI_VENDOR_ID_AMD,
	                                     PCI_DEVICE_ID_AMD_K8_MISC_CTRL,
	                                     devs);

	for (cpu=0; cpu <= cpucount; cpu++)
	{

		for (core=0; core < 2; core++)
		{
			for (sensor=0; sensor < 2; sensor++)
			{
				temp = get_temp(devs[cpu], core, sensor);
				if (temp > TEMP_MIN + OFFSET_MAX)
					printf("cpu%d core%d sensor%d: %dc\n",
					       cpu, core, sensor, temp);
			}
		}
	}
	exit(0);
}
