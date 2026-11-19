#!/bin/bash
# build-console.sh - gera a imagem final do CCS (console com os 6 jogos).
#
# Uso (na maquina de build):
#   ./scripts/build-console.sh
#
# Variaveis opcionais:
#   JOGOS_ZIP      zip com os jogos do Wecton  (padrao: ~/j/entrega_nino.zip)
#   CCS_JOGOS_DIR  pasta ja extraida, com opt/jogos e usr/bin
#                  (padrao: ~/j/e/entrega_nino)
#   OUTPUT         pasta de saida do Buildroot (padrao: ~/output-console)
#
# Os jogos NAO vao para o GitHub (sao grandes e tem conteudo com direitos
# autorais): entram so na imagem, como mais um rootfs overlay.
set -e

BR_VERSION="2026.02.3"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BR_DIR="${BR_DIR:-$HOME/buildroot-$BR_VERSION}"
EXTERNAL="$REPO_DIR/br2-external-tp9"
OVERLAY="$EXTERNAL/board/console/rootfs-overlay"
DEFCONFIG="ccs_console_raspberrypi4_defconfig"
OUTPUT="${OUTPUT:-$HOME/output-console}"
JOGOS_ZIP="${JOGOS_ZIP:-$HOME/j/entrega_nino.zip}"
export CCS_JOGOS_DIR="${CCS_JOGOS_DIR:-$HOME/j/e/entrega_nino}"

echo "==> 1/6 Jogos ($CCS_JOGOS_DIR)"
if [ ! -d "$CCS_JOGOS_DIR/opt/jogos" ]; then
    [ -f "$JOGOS_ZIP" ] || { echo "ERRO: nao achei $CCS_JOGOS_DIR nem $JOGOS_ZIP"; exit 1; }
    mkdir -p "$(dirname "$CCS_JOGOS_DIR")"
    unzip -q -o "$JOGOS_ZIP" -d "$(dirname "$CCS_JOGOS_DIR")"
fi
for j in cs16 sm64 zelda crash doom quake; do
    if [ -f "$CCS_JOGOS_DIR/opt/jogos/$j/start.sh" ]; then
        printf "    %-6s %s\n" "$j" "$(du -sh "$CCS_JOGOS_DIR/opt/jogos/$j" | cut -f1)"
    else
        echo "    $j     FALTANDO (o card aparece, mas o jogo nao abre)"
    fi
done

echo "==> 2/6 Emulador ARM (some a cada reboot da maquina de build)"
docker run --privileged --rm tonistiigi/binfmt --install arm64 >/dev/null

echo "==> 3/6 Imagem Docker do painel web (arm64)"
# garante a imagem base arm64 (uma python:3.12-slim x86 no cache quebra o build)
docker pull -q --platform linux/arm64 python:3.12-slim >/dev/null
if docker buildx version >/dev/null 2>&1; then
    docker build --platform linux/arm64 -t ccs-console:1.0 "$REPO_DIR/console-app"
else
    # Sem o buildx, o builder antigo do Docker falha com imagem de outra
    # arquitetura ("does not provide the specified platform"). Faz o mesmo que
    # o Dockerfile, a mao: instala o Flask num container arm64 e salva com
    # docker commit.
    docker rm -f ccs-console-build >/dev/null 2>&1 || true
    docker run --platform linux/arm64 --name ccs-console-build \
        -v "$REPO_DIR/console-app:/src:ro" python:3.12-slim \
        sh -c "mkdir -p /app && cp -r /src/. /app && pip install --no-cache-dir -q -r /app/requirements.txt"
    docker commit \
        --change 'WORKDIR /app' --change 'EXPOSE 5000' \
        --change 'CMD ["python", "app.py"]' \
        ccs-console-build ccs-console:1.0 >/dev/null
    docker rm ccs-console-build >/dev/null
fi
docker image inspect ccs-console:1.0 --format '    imagem ccs-console:1.0 ({{.Architecture}})'
mkdir -p "$OVERLAY/opt/docker"
docker save ccs-console:1.0 -o "$OVERLAY/opt/docker/ccs-console.tar"

echo "==> 4/6 Buildroot $BR_VERSION"
if [ ! -d "$BR_DIR" ]; then
    wget -O /tmp/buildroot.tar.gz "https://buildroot.org/downloads/buildroot-$BR_VERSION.tar.gz"
    tar xf /tmp/buildroot.tar.gz -C "$(dirname "$BR_DIR")"
fi

echo "==> 5/6 Configuracao ($DEFCONFIG)"
make -C "$BR_DIR" O="$OUTPUT" BR2_EXTERNAL="$EXTERNAL" "$DEFCONFIG"

echo "==> 6/6 Compilacao (a primeira vez demora: X.org, Mesa, SDL2...)"
# o menu e um pacote local: recompila sempre, para pegar as mudancas do repo
make -C "$BR_DIR" O="$OUTPUT" ccs-menu-dirclean

# Drivers do X compilados antes do xorg-lazy.mk ficam com BIND_NOW e nao
# carregam ("undefined symbol" / "no screens found"): recompila esses pacotes.
PACOTES_X="xserver_xorg-server xdriver_xf86-video-fbdev xdriver_xf86-input-evdev xdriver_xf86-input-libinput"
MODESETTING="$OUTPUT/target/usr/lib/xorg/modules/drivers/modesetting_drv.so"
bind_now() { readelf -d "$1" 2>/dev/null | grep -Eq 'BIND_NOW|FLAGS_1.*NOW'; }
if [ -f "$MODESETTING" ] && bind_now "$MODESETTING"; then
    echo "    drivers do X com BIND_NOW: recompilando com -z lazy"
    for p in $PACOTES_X; do make -C "$BR_DIR" O="$OUTPUT" "$p-dirclean"; done
fi

make -C "$BR_DIR" O="$OUTPUT"

for m in "$MODESETTING" "$OUTPUT/target/usr/lib/xorg/modules/drivers/fbdev_drv.so"; do
    if bind_now "$m"; then
        echo "ERRO: $m ainda tem BIND_NOW; o X nao vai subir na Pi."
        exit 1
    fi
done
echo "    drivers do X sem BIND_NOW (ok)"

echo
echo "Imagem pronta: $OUTPUT/images/sdcard.img ($(du -h "$OUTPUT/images/sdcard.img" | cut -f1))"
echo "Hashes (vao no README):"
sha256sum "$OUTPUT/images/sdcard.img" "$OVERLAY/opt/docker/ccs-console.tar"
