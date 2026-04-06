# ryzen_smu_hwmon

Out-of-tree Linux **hwmon** driver for AMD Ryzen CPUs, built on top of the
exported **Ryzen SMU (System Management Unit) API**.

This driver exposes per-core temperatures, CCD temperatures, package / SoC
power, and core power via the standard `hwmon` interface.

---

## Status

- **CPU support (currently):**
  - Raphael (Zen 4)
  - Ryzen 9 7950X3D (CPUID-gated)
  - Ryzen 9 7950X

- **Driver type:** out-of-tree, DKMS-friendly  

---

## Requirements

This driver depends on a **modified `ryzen_smu` kernel module** that exports
a public SMU API.

You must install **one** of the following first:

- Source repository:  
  https://github.com/FrozenGalaxy/ryzen_smu

- DKMS package:  
  https://github.com/FrozenGalaxy/ryzen_smu-dkms

Required exported interfaces:
- PM table access
- CPUID helpers
- Device anchor (`struct device *`)

---

## Installation (DKMS – recommended)

A ready-to-use **PKGBUILD** is provided in this repository.

On Arch Linux or Arch-based distributions:

```bash
makepkg -si
```

## Build (manual)
```bash
make SMU_INC=/path/to/ryzen_smu/include \
     SMU_SYMVERS=/path/to/ryzen_smu/Module.symvers
```

## Load the module
### temporarily
```bash
sudo insmod ryzen_smu_hwmon.ko

# then verify working:
lsmod | grep ryzen_smu_hwmon
dmesg | grep -i ryzen_smu_hwmon
grep . /sys/class/hwmon/hwmon*/name

# verify in lm-sensors:
sensors
```
### permanent
```bash
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp ryzen_smu_hwmon.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
echo ryzen_smu_hwmon | sudo tee /etc/modules-load.d/ryzen_smu_hwmon.conf
```
```bash
# verify autoloaded after reboot
lsmod | grep -E 'ryzen_smu|ryzen_smu_hwmon'
```


### Example output:

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
