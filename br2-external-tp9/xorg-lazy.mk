# O Buildroot usa RELRO completo por padrao, e o wrapper do compilador passa
# -Wl,-z,now em todo link. Os modulos do X.org (modesetting, fbdev, glamor,
# shadow...) dependem de "lazy binding": um driver usa funcoes de outros
# modulos que o servidor so carrega depois. Com -z now o dlopen falha com
# "undefined symbol" (gbm_bo_get_plane_count, fbdevHWSave) e o X termina com
# "no screens found". Aqui voltamos a ligacao preguicosa so nos pacotes do X;
# o -z lazy vem depois do -z now do wrapper, entao prevalece.
CCS_XORG_LAZY_LDFLAGS = LDFLAGS="$(TARGET_LDFLAGS) -Wl,-z,lazy"

XSERVER_XORG_SERVER_CONF_ENV += $(CCS_XORG_LAZY_LDFLAGS)
XDRIVER_XF86_VIDEO_FBDEV_CONF_ENV += $(CCS_XORG_LAZY_LDFLAGS)
XDRIVER_XF86_INPUT_EVDEV_CONF_ENV += $(CCS_XORG_LAZY_LDFLAGS)
XDRIVER_XF86_INPUT_LIBINPUT_CONF_ENV += $(CCS_XORG_LAZY_LDFLAGS)
