#!/bin/sh
# post-build.sh do console - roda depois do post-build do TP9.
# $1 = TARGET_DIR (rootfs que vira a imagem)
set -e
TARGET_DIR="$1"

# o console usa o painel ccs-console na porta 80, no lugar do tp9-distance
rm -f "$TARGET_DIR/etc/init.d/S99tp9-distance" \
      "$TARGET_DIR/opt/docker/tp9-distance.tar"

# scripts e binarios dos jogos executaveis (o zip pode perder o bit +x)
if [ -d "$TARGET_DIR/opt/jogos" ]; then
	chmod +x "$TARGET_DIR"/opt/jogos/*/start.sh 2>/dev/null || true
	for b in cs16/xash3d sm64/sm64_arm64 zelda/soh.elf crash/pcsx_arm64 \
	         doom/doom_arm64 quake/quakespasm_arm64; do
		[ -f "$TARGET_DIR/opt/jogos/$b" ] && chmod +x "$TARGET_DIR/opt/jogos/$b"
	done
	echo "post-build (console): jogos em /opt/jogos:"
	ls "$TARGET_DIR/opt/jogos"
else
	echo "post-build (console): AVISO - /opt/jogos nao existe (CCS_JOGOS_DIR vazio?)"
fi
chmod +x "$TARGET_DIR"/usr/bin/jogar-* "$TARGET_DIR/usr/bin/ccs-jogar"
