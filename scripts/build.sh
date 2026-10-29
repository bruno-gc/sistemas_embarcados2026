#!/bin/bash
# build.sh - reconstroi a imagem completa do TP9 a partir do repositorio.
#
# Uso:
#   ./scripts/build.sh                       imagem do TP9 (a que vale nota)
#   DEFCONFIG=tp9_jogos_raspberrypi4_defconfig OUTPUT=$HOME/output-jogos \
#     ./scripts/build.sh                     variante com jogos (extra)
set -e

BR_VERSION="2026.02.3"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BR_DIR="${BR_DIR:-$HOME/buildroot-$BR_VERSION}"
EXTERNAL="$REPO_DIR/br2-external-tp9"
OVERLAY="$EXTERNAL/board/rpi4/rootfs-overlay"
DEFCONFIG="${DEFCONFIG:-tp9_raspberrypi4_defconfig}"
OUTPUT="${OUTPUT:-$BR_DIR/output}"

echo "==> 1/5 Emulador ARM (some a cada reboot da maquina de build)"
docker run --privileged --rm tonistiigi/binfmt --install arm64 >/dev/null

echo "==> 2/5 Imagem Docker da aplicacao (arm64)"
docker build --platform linux/arm64 -t tp9-distance:1.0 "$REPO_DIR/flask-app"
mkdir -p "$OVERLAY/opt/docker"
docker save tp9-distance:1.0 -o "$OVERLAY/opt/docker/tp9-distance.tar"

echo "==> 3/5 Buildroot $BR_VERSION"
if [ ! -d "$BR_DIR" ]; then
    wget -O /tmp/buildroot.tar.gz "https://buildroot.org/downloads/buildroot-$BR_VERSION.tar.gz"
    tar xf /tmp/buildroot.tar.gz -C "$(dirname "$BR_DIR")"
fi

echo "==> 4/5 Configuracao ($DEFCONFIG)"
make -C "$BR_DIR" O="$OUTPUT" BR2_EXTERNAL="$EXTERNAL" "$DEFCONFIG"

echo "==> 5/5 Compilacao"
make -C "$BR_DIR" O="$OUTPUT"

echo
echo "Imagem pronta: $OUTPUT/images/sdcard.img"
echo "Hashes dos artefatos (vao no README):"
sha256sum "$OUTPUT/images/sdcard.img" \
          "$OUTPUT/images/rpi-firmware/overlays/hcsr04.dtbo" \
          "$OVERLAY/opt/docker/tp9-distance.tar"
find "$OUTPUT/target/lib/modules" -name hcsr04.ko -exec sha256sum {} \;
