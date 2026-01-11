# ryzen_smu_hwmon

Hardware monitoring driver for AMD Ryzen CPUs using the Ryzen SMU PM table.

## Requirements

- Linux kernel with `ryzen_smu` module installed
- Exported SMU API (PM table, CPUID helpers)

## Build (manual)

```bash
make SMU_INC=/path/to/ryzen_smu/include \
     SMU_SYMVERS=/path/to/ryzen_smu/Module.symvers
