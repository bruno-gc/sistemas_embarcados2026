################################################################################
#
# ccs-menu - menu grafico do CCS (Console Contra Sony)
#
################################################################################

CCS_MENU_VERSION = 1.0
CCS_MENU_SITE = $(BR2_EXTERNAL_TP9_PATH)/package/ccs-menu/src
CCS_MENU_SITE_METHOD = local
CCS_MENU_LICENSE = MIT
CCS_MENU_DEPENDENCIES = host-pkgconf sdl2 sdl2_ttf sdl2_image xlib_libX11 xlib_libXrandr

define CCS_MENU_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -Wall -o $(@D)/ccs-menu $(@D)/ccs-menu.c \
		$$($(PKG_CONFIG_HOST_BINARY) --cflags --libs sdl2 SDL2_ttf SDL2_image) \
		-lm $(TARGET_LDFLAGS)
	$(TARGET_CC) $(TARGET_CFLAGS) -Wall -o $(@D)/ccs-wm $(@D)/ccs-wm.c \
		$$($(PKG_CONFIG_HOST_BINARY) --cflags --libs x11 xrandr) $(TARGET_LDFLAGS)
endef

define CCS_MENU_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/ccs-menu $(TARGET_DIR)/usr/bin/ccs-menu
	$(INSTALL) -D -m 0755 $(@D)/ccs-wm $(TARGET_DIR)/usr/bin/ccs-wm
endef

$(eval $(generic-package))
