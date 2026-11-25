#!/bin/sh
# CS 1.6 (Xash3D FWGS). -log grava o engine.log nesta pasta, para descobrir
# por que o jogo nao abre; -dev 2 deixa o log mais detalhado.
cd /opt/jogos/cs16
export LD_LIBRARY_PATH=/opt/jogos/cs16:$LD_LIBRARY_PATH
exec ./xash3d -game cstrike -fullscreen -log -dev 2 +gl_vsync 1
