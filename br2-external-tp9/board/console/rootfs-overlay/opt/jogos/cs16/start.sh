#!/bin/sh
# CS 1.6 (Xash3D FWGS). -log grava o engine.log nesta pasta, para descobrir
# por que o jogo nao abre; -dev 2 deixa o log mais detalhado.
# Renderizador: gles2 (OpenGL ES, nativo da GPU da Pi 4). Para voltar ao
# OpenGL de PC sem gerar outra imagem: echo gl > /etc/ccs/cs-ref
cd /opt/jogos/cs16
export LD_LIBRARY_PATH=/opt/jogos/cs16:$LD_LIBRARY_PATH
REF=gles2
[ -s /etc/ccs/cs-ref ] && REF="$(cat /etc/ccs/cs-ref)"
exec ./xash3d -game cstrike -ref "$REF" -fullscreen -log -dev 2 +gl_vsync 1
