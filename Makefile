ccflags-y := -std=gnu99 -Wno-declaration-after-statement
obj-m += rfsend.o

KVER := $(shell uname -r)

all:
	make -C /lib/modules/$(KVER)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVER)/build/ M=$(PWD) clean
