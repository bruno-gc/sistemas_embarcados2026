# Makefile do modulo de kernel hcsr04.ko
#
# Dentro do Buildroot, a infraestrutura kernel-module chama o Kbuild do
# kernel da imagem (make -C $(LINUX_DIR) M=...), que le apenas a linha obj-m.
# O bloco "else" permite compilar fora do Buildroot so para testes:
#   make KDIR=/caminho/do/kernel

ifneq ($(KERNELRELEASE),)

obj-m := hcsr04.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

endif
