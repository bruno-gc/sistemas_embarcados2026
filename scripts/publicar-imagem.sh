#!/bin/bash
# publicar-imagem.sh - compacta a imagem, divide em partes de 360 MB (para
# subir no Drive) e serve a pasta na porta 8090.
set -e
OUTPUT="${OUTPUT:-$HOME/output-console}"
DEST="${DEST:-$HOME/publicar}"
rm -rf "$DEST" && mkdir -p "$DEST"
echo "==> compactando $OUTPUT/images/sdcard.img"
gzip -c "$OUTPUT/images/sdcard.img" > "$DEST/sdcard.img.gz"
cd "$DEST"
split -b 360M --numeric-suffixes=1 -a 3 sdcard.img.gz sdcard.img.gz.
sha256sum sdcard.img.gz "$OUTPUT/images/sdcard.img" | sed "s#$OUTPUT/images/##" > sha256.txt
rm sdcard.img.gz
ls -l
cat sha256.txt
echo "==> servindo $DEST em http://0.0.0.0:8090"
exec python3 -m http.server 8090
