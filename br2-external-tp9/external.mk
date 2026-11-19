# Inclui o .mk de cada pacote em package/
include $(sort $(wildcard $(BR2_EXTERNAL_TP9_PATH)/package/*/*.mk))

# Ligacao preguicosa para os modulos do X.org (ver o arquivo)
include $(BR2_EXTERNAL_TP9_PATH)/xorg-lazy.mk
