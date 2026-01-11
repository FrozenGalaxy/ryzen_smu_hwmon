obj-m := ryzen_smu_hwmon.o

SMU_INC ?= $(firstword $(wildcard /usr/src/ryzen_smu-*/include))
SMU_SYMVERS ?= $(firstword $(wildcard /var/lib/dkms/ryzen_smu/*/*/Module.symvers))

ccflags-y += -I$(SMU_INC)
KBUILD_EXTRA_SYMBOLS := $(SMU_SYMVERS)
