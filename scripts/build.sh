#!/bin/bash
# Reconstrói a imagem completa do TP8 a partir do repositório
set -e

BR_VERSION="2026.02.3"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BR_DIR="${BR_DIR:-$HOME/buildroot-$BR_VERSION}"
OVERLAY="$REPO_DIR/br2-external/board/rpi4/overlay"

echo "==> 1/4 Imagem Docker da aplicação (arm64)"
docker build --platform linux/arm64 -t flask-tp8:1.0 "$REPO_DIR/flask-app"
mkdir -p "$OVERLAY/opt/docker"
docker save flask-tp8:1.0 -o "$OVERLAY/opt/docker/flask-tp8.tar"

echo "==> 2/4 Buildroot $BR_VERSION"
if [ ! -d "$BR_DIR" ]; then
    wget -O /tmp/buildroot.tar.gz "https://buildroot.org/downloads/buildroot-$BR_VERSION.tar.gz"
    tar xf /tmp/buildroot.tar.gz -C "$(dirname "$BR_DIR")"
fi

echo "==> 3/4 Configuração"
make -C "$BR_DIR" BR2_EXTERNAL="$REPO_DIR/br2-external" tp8_raspberrypi4_defconfig

echo "==> 4/4 Compilação"
make -C "$BR_DIR"

echo "Imagem pronta: $BR_DIR/output/images/sdcard.img"
