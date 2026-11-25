#!/bin/sh
# Diagnostico rapido do Zelda (Ship of Harkinian) na pasta de jogos da VM.
Z="${CCS_JOGOS_DIR:-$HOME/j/e/entrega_nino}/opt/jogos/zelda"
echo "== $Z"; ls -la "$Z" | head -30
for f in "$Z"/*.o2r "$Z"/*.otr; do
  [ -f "$f" ] || continue
  printf '%s: ' "$(basename "$f")"; head -c 4 "$f" | od -An -c | tr -s ' '
done
echo "== versao soh.elf"; strings "$Z/soh.elf" 2>/dev/null | grep -m5 -iE 'ship of harkinian|^[0-9]+\.[0-9]+\.[0-9]+$|Ackbar|Blair|Bradley|MacReady|Dorkler|Rachael|Spock|Zhora|Roy|Khan|Kirk' 
echo "== roms"; find "$HOME" /mnt /media -maxdepth 6 \( -iname '*.z64' -o -iname '*.n64' -o -iname '*.v64' \) 2>/dev/null | head
echo "== assets/extractor"; ls -d "$Z"/assets "$Z"/assets/extractor 2>&1 | head -3
ls "$HOME"/j/e 2>/dev/null; ls "$HOME"/j 2>/dev/null
