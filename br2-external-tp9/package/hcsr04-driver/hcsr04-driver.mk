################################################################################
#
# hcsr04-driver
#
# Compila o modulo hcsr04.ko contra o kernel da imagem (infraestrutura
# kernel-module) e o Device Tree overlay hcsr04.dtbo, instalado junto com
# os overlays oficiais da Raspberry Pi na particao de boot.
#
################################################################################

HCSR04_DRIVER_VERSION = 1.0
HCSR04_DRIVER_SITE = $(BR2_EXTERNAL_TP9_PATH)/package/hcsr04-driver/src
HCSR04_DRIVER_SITE_METHOD = local
HCSR04_DRIVER_LICENSE = GPL-2.0

# host-dtc compila o overlay; rpi-firmware cria a pasta de overlays antes
HCSR04_DRIVER_DEPENDENCIES = host-dtc rpi-firmware

HCSR04_DRIVER_DTS = $(BR2_EXTERNAL_TP9_PATH)/board/rpi4/hcsr04-overlay.dts

# O .dts usa #include <dt-bindings/gpio/gpio.h> (GPIO_ACTIVE_HIGH), entao
# passa primeiro pelo pre-processador C, com os headers do kernel da imagem.
define HCSR04_DRIVER_BUILD_DTBO
	$(HOSTCC) -E -P -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
		-I $(LINUX_DIR)/include \
		-o $(@D)/hcsr04-overlay.pp.dts $(HCSR04_DRIVER_DTS)
	$(HOST_DIR)/bin/dtc -@ -I dts -O dtb \
		-o $(@D)/hcsr04.dtbo $(@D)/hcsr04-overlay.pp.dts
endef
HCSR04_DRIVER_POST_BUILD_HOOKS += HCSR04_DRIVER_BUILD_DTBO

# O overlay vai para a particao de boot; o config.txt carrega com
# "dtoverlay=hcsr04"
HCSR04_DRIVER_INSTALL_IMAGES = YES
define HCSR04_DRIVER_INSTALL_IMAGES_CMDS
	$(INSTALL) -D -m 0644 $(@D)/hcsr04.dtbo \
		$(BINARIES_DIR)/rpi-firmware/overlays/hcsr04.dtbo
endef

$(eval $(kernel-module))
$(eval $(generic-package))
