KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all modules clean

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/src modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/src clean
