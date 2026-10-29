#!/bin/sh
# post-build.sh - roda no fim do build, antes de gerar a imagem.
# O Buildroot passa a pasta do rootfs (TARGET_DIR) como primeiro argumento.
set -e

TARGET_DIR="$1"
BOARD_DIR="$(dirname "$0")"

# Configuracao do Wi-Fi AP. Os arquivos ficam em board/rpi4/, como pede a
# estrutura do enunciado, e sao copiados para o /etc da imagem.
install -D -m 0644 "$BOARD_DIR/hostapd.conf" "$TARGET_DIR/etc/hostapd.conf"
install -D -m 0644 "$BOARD_DIR/dnsmasq.conf" "$TARGET_DIR/etc/dnsmasq.conf"

# Quem sobe o hostapd e o dnsmasq e o nosso S40wifi-ap, depois que a wlan0
# ja tem o IP 192.168.50.1. Remove os scripts de init que os pacotes
# instalam, para os servicos nao subirem duas vezes nem fora de ordem.
rm -f "$TARGET_DIR/etc/init.d/S80dnsmasq" \
      "$TARGET_DIR/etc/init.d/S80hostapd"
