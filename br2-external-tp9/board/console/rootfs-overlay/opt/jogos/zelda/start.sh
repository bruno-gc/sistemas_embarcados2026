#!/bin/sh
# Zelda (Ship of Harkinian). Sem SHIP_HOME o SoH procura os arquivos .otr/.o2r
# na pasta de dados do usuario (~/.local/share/...), nao aqui, e para com
# "Extractor assets not found". SHIP_HOME aponta para a pasta do jogo.
cd /opt/jogos/zelda
export SHIP_HOME=/opt/jogos/zelda
export LD_LIBRARY_PATH=/opt/jogos/zelda/lib:$LD_LIBRARY_PATH
exec ./soh.elf
