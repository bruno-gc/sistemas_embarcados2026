#!/bin/sh
# CS 1.6 (Xash3D FWGS). -log grava o engine.log nesta pasta.
# Renderizador: gl (OpenGL de PC, pela Mesa V3D). O gles2 desenhava a imagem
# embaralhada na Pi 4 (GL_INVALID_ENUM). Para testar outro sem gerar imagem:
#   echo gles2 > /etc/ccs/cs-ref
cd /opt/jogos/cs16
export LD_LIBRARY_PATH=/opt/jogos/cs16:$LD_LIBRARY_PATH
REF=gl
[ -s /etc/ccs/cs-ref ] && REF="$(cat /etc/ccs/cs-ref)"
exec ./xash3d -game cstrike -ref "$REF" -fullscreen -log +gl_vsync 1
