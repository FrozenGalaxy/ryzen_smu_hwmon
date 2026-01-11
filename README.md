# ryzen_smu_hwmon

Hardware monitoring driver for AMD Ryzen CPUs using the Ryzen SMU PM table.

## Requirements

- Linux kernel with `ryzen_smu` module installed
- Exported SMU API (PM table, CPUID helpers)

## Build (manual)

```bash
make SMU_INC=/path/to/ryzen_smu/include \
     SMU_SYMVERS=/path/to/ryzen_smu/Module.symvers
```
Example output:
```bash
ryzen_smu_hwmon-pci-0000
Adapter: PCI adapter
Vcore:           1.39 V  
VSOC:            1.25 V  
Tctl:           +31.2°C  
Tccd1:          +30.9°C  
Tccd2:          +30.0°C  
Core0 Temp:     +31.3°C  
Core1 Temp:     +29.5°C  
Core2 Temp:     +30.3°C  
Core3 Temp:     +28.6°C  
Core4 Temp:     +30.2°C  
Core5 Temp:     +28.6°C  
Core6 Temp:     +30.4°C  
Core7 Temp:     +29.0°C  
Core8 Temp:     +30.9°C  
Core9 Temp:     +30.0°C  
Core10 Temp:    +30.5°C  
Core11 Temp:    +29.9°C  
Core12 Temp:    +30.1°C  
Core13 Temp:    +29.6°C  
Package Power:  43.53 W  
SoC Power:      15.81 W  
Core0 Power:     4.03 W  
Core1 Power:     3.64 W  
Core2 Power:     3.43 W  
Core3 Power:     3.42 W  
Core4 Power:     3.37 W  
Core5 Power:     3.44 W  
Core6 Power:     3.70 W  
Core7 Power:     3.57 W  
Core8 Power:     4.07 W  
Core9 Power:     3.86 W  
Core10 Power:    4.34 W  
Core11 Power:    4.16 W  
Core12 Power:    4.16 W  
Core13 Power:    4.12 W  
Core14 Power:    3.94 W  
Core15 Power:    4.17 W 
```
