# Inclui o .mk de cada pacote em package/
include $(sort $(wildcard $(BR2_EXTERNAL_TP9_PATH)/package/*/*.mk))
