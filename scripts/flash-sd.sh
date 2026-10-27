#!/bin/bash
# Grava a imagem do Buildroot no microSD (Linux). No Windows, use o Raspberry Pi Imager.
# Uso: sudo ./scripts/flash-sd.sh /dev/sdX [caminho/sdcard.img]
set -e
DEV="$1"
IMG="${2:-/home/${SUDO_USER:-$USER}/buildroot-2026.02.3/output/images/sdcard.img}"
[ -z "$DEV" ] && { echo "Uso: sudo $0 /dev/sdX (confira o dispositivo com lsblk)"; exit 1; }
echo "ATENCAO: tudo em $DEV sera apagado. Continuar? (s/N)"
read -r resp
[ "$resp" = "s" ] || exit 1
dd if="$IMG" of="$DEV" bs=4M status=progress conv=fsync
sync
echo "Gravacao concluida."
