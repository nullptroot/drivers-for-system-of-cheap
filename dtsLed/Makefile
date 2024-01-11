KERNELDIR := /home/mr4/stm32/linux/linux-st/linux-5.4.31
CURRENT_PATH := $(shell pwd)
obj-m := dtsLed.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean