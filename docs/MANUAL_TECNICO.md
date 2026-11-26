# Manual Técnico — CCS (Console Contra Sony)

**Projeto Final de Sistemas Embarcados · Raspberry Pi 4 · Grupo 01 · Prof. Sergio Schina**

| Item | Valor |
| --- | --- |
| Repositório | https://github.com/bruno-gc/sistemas_embarcados2026 |
| Branch / tag | `console-final` / `ccs-final` |
| Defconfig | `br2-external-tp9/configs/ccs_console_raspberrypi4_defconfig` |
| Buildroot usado | 2026.02.3 |
| Plataforma | Raspberry Pi 4 Model B (AArch64) |
| Imagem entregue | `sdcard.img.gz` (4 partes no Drive), SHA-256 `6f6d082a4bc7bfe6ca626687da51b5f307c6751dbbfc1ae3d9e382ac35c50398` |
| Data | 25/11/2026 |

| Integrante | RA | Papel principal |
| --- | --- | --- |
| Vinicius Alves Santana | 232710 | Buildroot, menu e gerenciador de janelas, painel web, scripts e documentação |
| Bruno Garcia Carvalho | 226790 | Repositório, testes na Raspberry Pi, controle de PS5 |
| Matheus Correia Caumo | 232577 | Painel web e container (base do TP9), testes |
| Wecton | 228550 | Compilação dos 6 jogos para ARM64 |

Este manual segue as 12 seções da seção 8 do edital. A seção 4 traz a tabela de cada opção do Buildroot alterada pelo grupo, com caminho no menuconfig, símbolo, valor, justificativa, dependências e impacto.

---

## 1. Visão geral

### Problema e proposta

Um console de videogame comercial é um computador fechado: sistema próprio, inicialização rápida, um menu na TV e nada mais. O CCS mostra que dá para construir o mesmo tipo de equipamento com uma Raspberry Pi 4 e uma imagem Linux feita do zero com Buildroot. Ao ligar, a TV mostra um menu de jogos; o jogo abre em tela cheia; um painel web, num container Docker, mostra o que está na TV e abre ou fecha jogos pelo celular.

### Usuários

| Usuário | O que faz |
| --- | --- |
| Jogador | liga o console, escolhe o jogo na TV com teclado ou controle de PS5 |
| Pessoa no celular | acompanha e controla o console pelo painel web |
| Mantenedor | acessa o terminal (Ctrl+Alt+F1), lê logs e gera novas imagens |

### Escopo

Dentro do escopo: imagem Linux completa gerada pelo Buildroot; menu gráfico na HDMI; 6 jogos ARM64 (Counter-Strike 1.6, Super Mario 64, Zelda: Ocarina of Time, Crash Bandicoot, DOOM, Quake); painel web em container; rede Wi-Fi própria; operação sem Internet.

Fora do escopo: loja ou download de jogos, contas de usuário, jogo em rede pela Internet, Bluetooth.

### Requisitos

| ID | Requisito | Como é atendido |
| --- | --- | --- |
| RF1 | Ao ligar, mostrar o menu de jogos na TV sem intervenção | `S95ccs-menu` sobe X + `ccs-wm` + `ccs-menu` |
| RF2 | Abrir cada jogo em tela cheia, com as teclas no padrão de PC | `ccs-wm`, `ccs-preparar`, configs por jogo |
| RF3 | Voltar ao menu a partir de qualquer jogo | Home segurado 1 s, Select + Start por 2 s ou painel web |
| RF4 | Painel web para ver o estado e abrir ou fechar jogos | container `ccs-console`, porta 80 |
| RNF1 | Imagem 100% Buildroot, mínima e reproduzível | BR2_EXTERNAL + defconfig versionados (seções 4 e 9) |
| RNF2 | Docker e aplicação sobem sozinhos, sem Internet | `S60dockerd`, `S99ccs-console`, imagem `.tar` no cartão |
| RNF3 | Aplicação disponível em menos de 15 s após ligar | medição na seção 11.2 |
| RNF4 | Container sem modo privilegiado | apenas `/run/ccs`, `/etc/ccs:ro` e `/dev/hcsr04` |

### Relação com o TCC

Não há relação direta com o TCC dos integrantes. O tema foi escolhido pelo grupo e aprovado pelo professor no início do projeto (TP8).

### Critérios de sucesso

1. A Raspberry Pi 4 liga direto no menu do console, sem login nem comando.
2. Os 6 jogos abrem em tela cheia e voltam ao menu.
3. O painel web responde em `http://192.168.50.1` num celular conectado à rede `TP9-GRUPO01`.
4. Tudo funciona sem Internet e volta sozinho após um reboot ou queda de energia.
5. A imagem é reconstruída a partir do repositório seguindo este manual.

---

## 2. Arquitetura

### Diagrama

```
┌──────────────────────────── Raspberry Pi 4 (AArch64) ───────────────────────────┐
│                                                                                  │
│  Espaço de usuário (host Buildroot, BusyBox init)                                │
│  ┌──────────────────────────────┐     ┌──────────────────────────────────────┐   │
│  │ Xorg :0 (vt2, modesetting)   │     │ Docker Engine 28.3.3 (containerd,    │   │
│  │  ├─ ccs-wm  (gerenc. janelas)│     │ runc, overlay2, bridge docker0)      │   │
│  │  └─ ccs-menu (SDL2 + GL)     │     │  └─ container ccs-console:1.0        │   │
│  │       │ fork/exec            │     │      Flask :5000  ← -p 80:5000       │   │
│  │       ▼                      │     │      volumes: /run/ccs (rw)          │   │
│  │  /opt/jogos/<jogo>/start.sh  │     │               /etc/ccs (ro)          │   │
│  └──────────┬───────────────────┘     │      device:  /dev/hcsr04 (opcional) │   │
│             │  /run/ccs (tmpfs): status · pedido · sair   └───────┬──────────┘   │
│             └─────────────────────────────────────────────────────┘              │
│  hostapd + dnsmasq (wlan0 192.168.50.1)  ·  pulseaudio  ·  eudev                 │
├──────────────────────────────────────────────────────────────────────────────────┤
│  Kernel Linux 6.12.61 (bcm2711): vc4/v3d (DRM/KMS), brcmfmac (Wi-Fi),            │
│  hid-playstation, snd-bcm2835, cgroups v2, namespaces, overlayfs, bridge, veth,  │
│  netfilter, hcsr04.ko (driver do grupo, TP9)                                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│  Firmware (start4.elf) + config.txt + Device Tree bcm2711-rpi-4-b + overlays     │
└──────────────────────────────────────────────────────────────────────────────────┘
   HDMI → TV        USB → teclado, mouse, controle PS5        Wi-Fi → celular
```

### Fluxo de inicialização

```
Energia → firmware da GPU lê config.txt → carrega kernel (Image) + DTB + overlays
 → kernel monta /dev/mmcblk0p2 (ext4) → BusyBox init (/etc/inittab)
 → /etc/init.d/rcS executa S* em ordem:
     S01..S0x  (syslog, montagens do Buildroot)
     S10udevd  eudev: teclado, mouse, controle
     S30hcsr04 driver do sensor (TP9)
     S40wifi-ap  wlan0 = 192.168.50.1, hostapd, dnsmasq
     S50pulseaudio
     S60dockerd  Docker Engine
     S95ccs-menu X no vt2 + ccs-wm + ccs-menu (em segundo plano)
     S99ccs-console  docker load (1º boot) + docker run do painel
 → menu na TV + painel em http://192.168.50.1
```

### Comunicação entre menu e painel

O menu roda no host e o painel roda no container. Eles conversam só por arquivos em `/run/ccs`, um tmpfs:

| Arquivo | Quem escreve | Quem lê | Conteúdo |
| --- | --- | --- | --- |
| `/run/ccs/status` | menu | painel | `{"estado":"menu"/"jogando"/"terminal","jogo":…,"nome":…,"desde":…}` |
| `/run/ccs/pedido` | painel, `jogar-*`, `ccs-wm` | menu | `jogar <id>` ou `parar` |
| `/run/ccs/sair` | menu | `S95ccs-menu` | existe quando o usuário escolhe "Sair para o terminal" |

### Interface gráfica local: justificativa (edital, seção 4.1)

A função do projeto é jogar na TV, então ela exige saída de vídeo local acelerada. A imagem **não tem ambiente de desktop, display manager, tela de login gráfica, painel, área de trabalho nem gerenciador de arquivos**. A pilha gráfica é a mínima para os jogos:

| Camada | Por que existe |
| --- | --- |
| DRM/KMS (`vc4-kms-v3d`) no kernel | driver da GPU e da HDMI da Pi 4; sem ele não há OpenGL acelerado |
| Mesa 26.0.1 (V3D/VC4), OpenGL, OpenGL ES, EGL, GBM | os 6 motores de jogo usam OpenGL ou OpenGL ES |
| Xorg 21.1.23 com o driver `modesetting` | os motores foram compilados com SDL2 para X11 (Xash3D, sm64ex, Ship of Harkinian, PCSX-ReARMed, Chocolate Doom, QuakeSpasm); o X serve só de tela para eles, em modo quiosque |
| `ccs-wm` (Xlib, ~300 linhas, do grupo) | põe cada janela em tela cheia e captura a tecla Home; não desenha bordas, menus nem área de trabalho |
| SDL2 2.32.10 | biblioteca de vídeo, áudio e controle de todos os jogos e do menu |

A interface de controle remoto é Web, hospedada no container, como o edital prefere. As bibliotecas gráficas ficam no host porque os jogos precisam de acesso direto à GPU, a `/dev/dri`, ao áudio e aos dispositivos de entrada. Levá-las para um container exigiria `--privileged` ou vários `--device`, o que pioraria o isolamento sem ganho para o projeto.

---

## 3. Ambiente de build

| Item | Valor |
| --- | --- |
| Máquina | VM no VirtualBox, Ubuntu Server 26.04.1 LTS (x86_64), sem interface gráfica |
| Recursos | 6 CPUs, 8 GB de RAM, disco de 80 GB (reservar pelo menos 60 GB livres) |
| Buildroot | 2026.02.3 em `~/buildroot-2026.02.3` (baixado pelo script) |
| Toolchain | Bootlin externa `aarch64 glibc stable 2025.08-1` (GCC 14), baixada pelo Buildroot |
| Docker na máquina de build | `docker.io` do Ubuntu, com emulação ARM (`tonistiigi/binfmt`) |
| Jogos | `~/j/entrega_nino.zip` (~1,9 GB), fora do Git |

### Preparar uma máquina limpa

```bash
sudo apt update
sudo apt install -y build-essential git wget cpio unzip rsync bc file python3 \
    libncurses-dev patch bzip2 docker.io
sudo usermod -aG docker $USER
newgrp docker
```

No Ubuntu 26.04 o comando `install` padrão vem do uutils e é recusado pelo Buildroot. Troque pelo do GNU coreutils:

```bash
sudo update-alternatives --install /usr/bin/install install /usr/bin/gnuinstall 100   # se /usr/bin/gnuinstall existir
sudo apt install -y coreutils-from-gnu --allow-remove-essential                       # caso contrário
install --version | head -1      # deve mostrar "GNU coreutils"
```

Clonar o repositório e colocar os jogos:

```bash
git clone https://github.com/bruno-gc/sistemas_embarcados2026.git
cd sistemas_embarcados2026
git checkout ccs-final
mkdir -p ~/j && cp /caminho/do/entrega_nino.zip ~/j/
```

O primeiro build baixa cerca de 2 GB de fontes e leva algumas horas (toolchain, kernel, X.org, Mesa, Docker). Um rebuild só do que mudou leva poucos minutos.

---

## 4. Configuração do Buildroot

### Organização

Todas as customizações ficam num BR2_EXTERNAL, `br2-external-tp9/` (nome `TP9` em `external.desc`). O código do Buildroot não é alterado.

```
br2-external-tp9/
├── external.desc / Config.in / external.mk   ← árvore externa e menu "TP9 - pacotes do grupo"
├── xorg-lazy.mk                              ← -Wl,-z,lazy nos pacotes do X (seção 6)
├── configs/
│   ├── tp9_raspberrypi4_defconfig            ← TP9
│   └── ccs_console_raspberrypi4_defconfig    ← este projeto
├── package/
│   ├── ccs-menu/   (Config.in, ccs-menu.mk, src/ccs-menu.c, src/ccs-wm.c)
│   └── hcsr04-driver/                        ← driver de kernel do TP9
└── board/
    ├── rpi4/       hostapd.conf, dnsmasq.conf, users.txt, post-build.sh, rootfs-overlay/
    └── console/    config.txt, post-build.sh, arte/, rootfs-overlay/
```

A defconfig parte da `raspberrypi4_64_defconfig` oficial. Em relação a ela, o grupo **alterou 3 opções** (`RPI_FIRMWARE_CONFIG_FILE`, `ROOTFS_POST_BUILD_SCRIPT` e `TARGET_ROOTFS_EXT2_SIZE`) e **habilitou as demais** listadas abaixo. Nenhuma opção da configuração oficial foi desabilitada. As opções herdadas sem mudança (arquitetura, toolchain, kernel, firmware, imagem) estão no fim da seção.

Legenda de impacto: **T** = tamanho da imagem, **B** = tempo de boot, **S** = segurança, **L** = licença. Os tamanhos e tempos marcados com ~ são estimativas do grupo, não medições; a medição de boot está na seção 11.2.

### 4.1 Sistema

| Caminho no menuconfig | Símbolo = valor | Justificativa | Dependências | Impacto |
| --- | --- | --- | --- | --- |
| System configuration → System hostname | `BR2_TARGET_GENERIC_HOSTNAME="CSS-console"` | identificar a placa | — | nenhum. Obs.: o valor ficou com as letras trocadas (CSS) |
| System configuration → System banner | `BR2_TARGET_GENERIC_ISSUE="Console Contra Sony(CSS)"` | mensagem antes do login | — | nenhum |
| System configuration → Root password | `BR2_TARGET_GENERIC_ROOT_PASSWD="CCS1234"` | acesso de manutenção pelo terminal | `BR2_TARGET_ENABLE_ROOT_LOGIN` (padrão) | S: senha conhecida, em texto no repositório |
| System configuration → /dev management → Dynamic using devtmpfs + eudev | `BR2_ROOTFS_DEVICE_CREATION_DYNAMIC_EUDEV=y` | o X precisa saber quando teclado, mouse e controle aparecem (hotplug) | eudev | T: +~1 MB; B: `S10udevd` soma alguns décimos de segundo |
| System configuration → Path to the users tables | `BR2_ROOTFS_USERS_TABLES=".../board/rpi4/users.txt"` | cria o usuário `nino` no build (herdado do TP8) | — | S: senha em texto |
| System configuration → Root filesystem overlay directories | `BR2_ROOTFS_OVERLAY="board/rpi4/rootfs-overlay $(CCS_JOGOS_DIR) board/console/rootfs-overlay"` | scripts de init, configs e jogos entram na imagem nessa ordem; o último sobrescreve | variável `CCS_JOGOS_DIR` exportada pelo `build-console.sh` | T: +~1,9 GB (jogos); L: jogos com direitos autorais (seção 12) |
| System configuration → Custom scripts to run before creating filesystem images | `BR2_ROOTFS_POST_BUILD_SCRIPT="board/raspberrypi4-64/post-build.sh .../rpi4/post-build.sh .../console/post-build.sh"` (**alterado**) | instala hostapd.conf/dnsmasq.conf, remove init duplicados (S80dnsmasq, S80hostapd, S40xorg, S99tp9-distance) e dá permissão aos scripts | — | B: evita dois X e serviços fora de ordem |

### 4.2 Firmware e boot

| Caminho no menuconfig | Símbolo = valor | Justificativa | Dependências | Impacto |
| --- | --- | --- | --- | --- |
| Target packages → Hardware handling → Firmware → rpi-firmware → Path to a file stored as boot/config.txt | `BR2_PACKAGE_RPI_FIRMWARE_CONFIG_FILE=".../board/console/config.txt"` (**alterado**) | liga a GPU (`vc4-kms-v3d`), o áudio e o overlay do sensor | rpi-firmware | B: o driver KMS soma ~1 s ao boot |
| Target packages → Hardware handling → Firmware → brcmfmac-sdio-firmware-rpi (+ wifi) | `BR2_PACKAGE_BRCMFMAC_SDIO_FIRMWARE_RPI=y`, `..._WIFI=y` | firmware do chip Wi-Fi da Pi 4 (faltava no TP9) | — | T: +~1 MB; L: proprietária (redistribuível) |

### 4.3 Gráficos

| Caminho no menuconfig | Símbolo = valor | Justificativa | Dependências | Impacto |
| --- | --- | --- | --- | --- |
| Target packages → Graphic libraries and applications → mesa3d | `BR2_PACKAGE_MESA3D=y` | OpenGL acelerado pela GPU | libdrm, expat, zlib (automáticas) | T: +~30 MB; L: MIT |
| … → mesa3d → Gallium v3d driver / Gallium vc4 driver | `BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_V3D=y`, `..._VC4=y` | V3D renderiza (Pi 4); VC4 é o controlador de display (KMS) | `BR2_PACKAGE_MESA3D_GBM` | T |
| … → mesa3d → gbm | `BR2_PACKAGE_MESA3D_GBM=y` | buffers para o X (`modesetting`/glamor) e o SDL KMSDRM | — | — |
| … → mesa3d → OpenGL GLX / OpenGL EGL / OpenGL ES | `BR2_PACKAGE_MESA3D_OPENGL_GLX=y`, `..._EGL=y`, `..._ES=y` | GLX: jogos OpenGL no X (CS, Quake, DOOM); EGL e ES: SDL e motores ES | X.org (GLX) | T |
| Target packages → Graphic libraries and applications → libglvnd | `BR2_PACKAGE_LIBGLVND=y` | despachante GL/EGL usado pela Mesa | — | L: MIT-like |
| Target packages → Graphic libraries and applications → X.org X Window System | `BR2_PACKAGE_XORG7=y` | servidor gráfico dos jogos (seção 2) | xkeyboard-config, libs X11 | T: +~15 MB; S: X só em `localhost` (`-nolisten tcp`) |
| … → X.org → X11R7 Servers → xorg-server | `BR2_PACKAGE_XSERVER_XORG_SERVER=y` | servidor X com o driver `modesetting` (KMS) | mesa3d (glamor) | B: X sobe em ~1–2 s |
| … → X.org → X11R7 Applications → xinit | `BR2_PACKAGE_XAPP_XINIT=y` | inicia o X com a sessão do console (`xinit /usr/bin/ccs-sessao`) | — | — |
| … → X.org → X11R7 Applications → xrandr | `BR2_PACKAGE_XAPP_XRANDR=y` | muda a resolução por jogo (`ccs-resolucao`) | libXrandr | — |
| … → X.org → X11R7 Drivers → xf86-input-evdev, xf86-input-libinput | `BR2_PACKAGE_XDRIVER_XF86_INPUT_EVDEV=y`, `..._LIBINPUT=y` | teclado, mouse e controle no X | eudev | — |
| … → X.org → X11R7 Drivers → xf86-video-fbdev | `BR2_PACKAGE_XDRIVER_XF86_VIDEO_FBDEV=y` | vídeo reserva se o KMS falhar | — | T: pequeno |
| … → X.org → X11R7 Libraries → libxshmfence; Target packages → Libraries → Graphics → libepoxy | `BR2_PACKAGE_XLIB_LIBXSHMFENCE=y`, `BR2_PACKAGE_LIBEPOXY=y` | DRI3 e glamor: OpenGL acelerado dentro do X | — | — |
| Target packages → Graphic libraries and applications → sdl2 | `BR2_PACKAGE_SDL2=y` | biblioteca de vídeo, áudio e controle dos jogos e do menu | — | L: Zlib |
| … → sdl2 → X11 video driver / KMS/DRM video driver / OpenGL (GLX) / OpenGL ES | `BR2_PACKAGE_SDL2_X11=y`, `..._KMSDRM=y`, `..._OPENGL=y`, `..._OPENGLES=y` | backends usados pelos motores dos jogos | X.org, mesa3d | — |
| Target packages → Graphic libraries and applications → sdl2_ttf | `BR2_PACKAGE_SDL2_TTF=y` | texto do menu (fonte Inter) | freetype | — |
| Target packages → Libraries → Graphics → freetype | `BR2_PACKAGE_FREETYPE=y` | renderização de fontes | — | L: FTL ou GPL-2.0+ |
| Target packages → Fonts, cursors, icons, sounds and themes → DejaVu fonts | `BR2_PACKAGE_DEJAVU=y` | fonte reserva do menu e do X | — | T: +~5 MB |

### 4.4 Áudio e bibliotecas dos jogos

| Caminho no menuconfig | Símbolo = valor | Justificativa | Dependências | Impacto |
| --- | --- | --- | --- | --- |
| Target packages → Audio and video applications → alsa-utils (amixer, aplay/arecord) | `BR2_PACKAGE_ALSA_UTILS=y`, `..._AMIXER=y`, `..._APLAY=y` | ajustar e testar o áudio HDMI | alsa-lib | — |
| Target packages → Audio and video applications → mpg123 | `BR2_PACKAGE_MPG123=y` | `libmpg123`, usada pelo motor do CS | — | L: LGPL-2.1 |
| Target packages → Audio and video applications → pulseaudio | `BR2_PACKAGE_PULSEAUDIO=y` | servidor de áudio usado pelo SDL nos jogos | libsndfile, dbus | B: `S50pulseaudio` |
| Target packages → Compressors and decompressors → bzip2 | `BR2_PACKAGE_BZIP2=y` | `libbz2.so.1.0`, exigida pela `libxash.so` (CS) | — | T: pequeno |
| Target packages → Compressors and decompressors → xz-utils | `BR2_PACKAGE_XZ=y` | descompactar arquivos `.xz` na manutenção | — | — |
| Target packages → Libraries → Multimedia → libogg | `BR2_PACKAGE_LIBOGG=y` | áudio Ogg do CS | — | L: BSD-3-Clause |
| Target packages → Libraries → Audio/Sound → libvorbis | `BR2_PACKAGE_LIBVORBIS=y` | `libvorbis`/`libvorbisfile` do CS | libogg | L: BSD-3-Clause |
| Target packages → Libraries → Hardware handling → libusb | `BR2_PACKAGE_LIBUSB=y` | acesso USB usado pelos emuladores/controle | — | — |
| Target packages → Libraries → Compression and decompression → zlib support | `BR2_PACKAGE_ZLIB=y` | dependência comum dos jogos (PNG, dados) | — | — |

### 4.5 Rede, Docker e manutenção

| Caminho no menuconfig | Símbolo = valor | Justificativa | Dependências | Impacto |
| --- | --- | --- | --- | --- |
| Target packages → Networking applications → hostapd | `BR2_PACKAGE_HOSTAPD=y` | a Pi cria a rede `TP9-GRUPO01` (Access Point) | libnl, openssl | B: associação Wi-Fi entra no tempo; S: WPA2-PSK |
| Target packages → Networking applications → dnsmasq | `BR2_PACKAGE_DNSMASQ=y` | DHCP e DNS para quem entra na rede | — | — |
| Target packages → Networking applications → iw | `BR2_PACKAGE_IW=y` | diagnóstico do Wi-Fi | libnl | — |
| Target packages → System tools → docker-engine | `BR2_PACKAGE_DOCKER_ENGINE=y` | Docker no host (exigência do edital) | seleciona containerd, runc, iptables, libseccomp, cgroupfs-v2-mount; ativa opções do kernel (seção 5) | T: +~120 MB; B: `dockerd` leva alguns segundos; L: Apache-2.0 |
| Target packages → System tools → docker-cli | `BR2_PACKAGE_DOCKER_CLI=y` | comando `docker` usado pelo `S99ccs-console` | — | L: Apache-2.0 |
| Target packages → System tools → kmod (+ kmod utilities) | `BR2_PACKAGE_KMOD=y`, `..._KMOD_TOOLS=y` | `modprobe` do driver `hcsr04` e dos módulos | — | — |
| Target packages → Libraries → Networking → libcurl (+ curl binary) | `BR2_PACKAGE_LIBCURL=y`, `..._CURL=y` | testes locais do painel (`curl 127.0.0.1/api/status`) | — | — |
| External options → TP9 - pacotes do grupo → ccs-menu | `BR2_PACKAGE_CCS_MENU=y` | menu e gerenciador de janelas (seção 8) | seleciona sdl2, sdl2_ttf, sdl2_image, libpng, jpeg, libX11, libXrandr, xrandr, xmodmap, dejavu | L: MIT (código do grupo) |
| External options → TP9 - pacotes do grupo → hcsr04-driver | `BR2_PACKAGE_HCSR04_DRIVER=y` | driver do sensor do TP9, mantido na imagem | kernel | L: GPL-2.0 |

### 4.6 Imagem

| Caminho no menuconfig | Símbolo = valor | Justificativa | Dependências | Impacto |
| --- | --- | --- | --- | --- |
| Filesystem images → ext2/3/4 root filesystem → exact size | `BR2_TARGET_ROOTFS_EXT2_SIZE="5G"` (**alterado**, era 120M) | jogos (~1,9 GB) + X/Mesa + Docker; cabe em microSD de 8 GB | `BR2_TARGET_ROOTFS_EXT2_4` | T: `sdcard.img` ≈ 5 GB (1,4 GB compactada) |

### 4.7 Opções herdadas da configuração oficial (sem mudança)

| Caminho no menuconfig | Símbolo = valor | Motivo |
| --- | --- | --- |
| Target options → Target Architecture / CPU / Floating point | `BR2_aarch64=y`, `BR2_cortex_a72=y`, `BR2_ARM_FPU_VFPV4=y` | CPU da Pi 4, 64 bits |
| Toolchain → Toolchain type → External toolchain → Bootlin toolchains → aarch64 glibc stable 2025.08-1 | `BR2_TOOLCHAIN_EXTERNAL_BOOTLIN_AARCH64_GLIBC_STABLE=y` | toolchain pronta e testada; glibc exigida pelo Docker |
| Build options → global patch and hash directories | `BR2_GLOBAL_PATCH_DIR="board/raspberrypi/patches"` | patches oficiais da placa |
| Build options → Advanced → Force all downloads to have a valid hash | `BR2_DOWNLOAD_FORCE_CHECK_HASHES=y` | reprodutibilidade e integridade das fontes |
| System configuration → Network interface to configure through DHCP | `BR2_SYSTEM_DHCP="eth0"` | Ethernet por DHCP para manutenção |
| System configuration → Custom scripts to run after creating filesystem images | `BR2_ROOTFS_POST_IMAGE_SCRIPT="board/raspberrypi4-64/post-image.sh"` | monta a `sdcard.img` com o genimage |
| Kernel → Linux Kernel → Custom tarball | `BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION=raspberrypi/linux@21b41014…` | kernel 6.12.61 da Raspberry, commit fixo |
| Kernel → Defconfig name | `BR2_LINUX_KERNEL_DEFCONFIG="bcm2711"` | configuração oficial da Pi 4 |
| Kernel → Build a Device Tree Blob → In-tree Device Tree Source file names | `BR2_LINUX_KERNEL_INTREE_DTS_NAME="broadcom/bcm2711-rpi-4-b …"` | DTB da Pi 4 |
| Target packages → Hardware handling → Firmware → rpi-firmware → rpi 4 | `BR2_PACKAGE_RPI_FIRMWARE_VARIANT_PI4=y` | firmware da GPU da Pi 4 |
| Host utilities → host dosfstools, host genimage, host mtools, host kmod (xz) | `BR2_PACKAGE_HOST_*=y` | criar a partição de boot e a imagem |
| Filesystem images → ext4; tar desligado | `BR2_TARGET_ROOTFS_EXT2_4=y`; `# BR2_TARGET_ROOTFS_TAR is not set` | rootfs ext4 com journal |

Para reproduzir a configuração ou abrir o menuconfig:

```bash
make -C ~/buildroot-2026.02.3 O=$HOME/output-console \
     BR2_EXTERNAL=$PWD/br2-external-tp9 ccs_console_raspberrypi4_defconfig
make -C ~/buildroot-2026.02.3 O=$HOME/output-console menuconfig
make -C ~/buildroot-2026.02.3 O=$HOME/output-console savedefconfig \
     BR2_DEFCONFIG=$PWD/br2-external-tp9/configs/ccs_console_raspberrypi4_defconfig
```

---

## 5. Kernel e drivers

### Kernel

| Item | Valor |
| --- | --- |
| Fonte | `github.com/raspberrypi/linux`, commit `21b410140c47ffab5668399f6f143c7d7b935c8b` |
| Versão | 6.12.61 (`-v8`) |
| Configuração base | `arch/arm64/configs/bcm2711_defconfig` |
| Fragmento próprio | nenhum |

O grupo não criou fragmento de kernel: a `bcm2711_defconfig` oficial já traz o que o console usa, e o pacote `docker-engine` do Buildroot ajusta sozinho as opções de container. Ao habilitar o Docker, o Buildroot força no `.config` do kernel (`DOCKER_ENGINE_LINUX_CONFIG_FIXUPS`, em `package/docker-engine/docker-engine.mk`):

```
CONFIG_POSIX_MQUEUE  CONFIG_CGROUPS  CONFIG_MEMCG  CONFIG_CGROUP_SCHED
CONFIG_CGROUP_FREEZER  CONFIG_CPUSETS  CONFIG_CGROUP_DEVICE  CONFIG_CGROUP_CPUACCT
CONFIG_NAMESPACES  CONFIG_UTS_NS  CONFIG_IPC_NS  CONFIG_PID_NS  CONFIG_NET_NS
CONFIG_NETFILTER  CONFIG_NETFILTER_ADVANCED  CONFIG_BRIDGE_NETFILTER  CONFIG_NF_CONNTRACK
CONFIG_NETFILTER_XTABLES  CONFIG_NETFILTER_XT_MATCH_ADDRTYPE  CONFIG_NETFILTER_XT_MATCH_CONNTRACK
CONFIG_IP_NF_IPTABLES  CONFIG_IP_NF_FILTER  CONFIG_IP_NF_NAT  CONFIG_IP_NF_TARGET_MASQUERADE
CONFIG_BRIDGE  CONFIG_VETH  CONFIG_OVERLAY_FS  CONFIG_KEYS
```

A configuração final do kernel gerada no build fica em `~/output-console/build/linux-21b41014…/.config`. Para versioná-la como fragmento mínimo: `make -C ~/buildroot-2026.02.3 O=$HOME/output-console linux-savedefconfig`, que grava `defconfig` na pasta de build do kernel.

### Drivers e módulos usados

| Driver | Configuração | Para quê |
| --- | --- | --- |
| `vc4` (DRM/KMS) | `CONFIG_DRM_VC4=m` | saída HDMI e modo de vídeo |
| `v3d` | `CONFIG_DRM_V3D=m` | renderização 3D (OpenGL/ES via Mesa) |
| `brcmfmac` | `CONFIG_BRCMFMAC=m` | Wi-Fi onboard (Access Point) |
| `hid-playstation` | `CONFIG_HID_PLAYSTATION=m` | controle de PS5 (DualSense) por USB |
| `snd-bcm2835` / áudio HDMI do vc4 | `CONFIG_SND_BCM2835=m` | som |
| `overlay`, `bridge`, `veth`, netfilter | ajustados pelo Buildroot (acima) | Docker: storage `overlay2` e rede bridge |
| `hcsr04.ko` | pacote `hcsr04-driver` (TP9) | sensor ultrassônico em `/dev/hcsr04` (opcional) |

Os módulos são carregados pelo eudev conforme o hardware aparece; o `hcsr04` é carregado pelo `S30hcsr04`.

### Device Tree e `config.txt`

O firmware carrega `bcm2711-rpi-4-b.dtb` e os overlays pedidos em `board/console/config.txt`:

| Linha | Motivo |
| --- | --- |
| `arm_64bit=1`, `kernel=Image` | kernel AArch64 |
| `start_file=start4.elf`, `fixup_file=fixup4.dat` | firmware da Pi 4 |
| `dtoverlay=vc4-kms-v3d`, `max_framebuffers=2` | GPU em modo KMS (Mesa, X `modesetting`) |
| `gpu_mem=256` | memória da GPU para texturas dos jogos |
| `disable_overscan=1` | imagem ocupa a TV inteira |
| `dtparam=audio=on` | áudio |
| `dtoverlay=miniuart-bt` | herdado da configuração oficial |
| `dtoverlay=hcsr04` | pinos do sensor (GPIO23 TRIG, GPIO24 ECHO) — TP9 |

Linha de comando do kernel (`cmdline.txt` oficial): `root=/dev/mmcblk0p2 rootwait console=tty1 console=ttyAMA0,115200`.

### Correção de ligação do X.org (`xorg-lazy.mk`)

O Buildroot compila tudo com RELRO completo (`-Wl,-z,now`). Os módulos do X (`modesetting_drv.so`, `fbdev_drv.so`) usam símbolos que só existem depois de carregados pelo servidor, e com `-z now` o X falhava com "undefined symbol" e "no screens found". O `xorg-lazy.mk` acrescenta `-Wl,-z,lazy` só aos pacotes `xserver_xorg-server`, `xf86-video-fbdev`, `xf86-input-evdev` e `xf86-input-libinput`. O `build-console.sh` confere com `readelf -d` que os drivers não ficaram com `BIND_NOW`.

O arquivo `/etc/X11/xorg.conf.d/99-vc4.conf` faz o X usar o `card1` (vc4, que tem a HDMI) como GPU principal; o `card0` (v3d) só renderiza.

---

## 6. Boot e init

### Sequência

O init é o do BusyBox (`/sbin/init` + `/etc/inittab`), que roda `/etc/init.d/rcS` e executa os scripts `S??*` em ordem alfabética. Escolhemos o BusyBox init por ser o mais leve e previsível; o systemd somaria dezenas de MB e segundos de boot.

| Script | Origem | O que faz | Espera por (condição real) | Limite |
| --- | --- | --- | --- | --- |
| `S01syslogd`, `S02klogd`, `S02sysctl` | Buildroot | logs e sysctl | — | — |
| `S10udevd` | eudev | cria dispositivos e carrega módulos | — | — |
| `S30hcsr04` | grupo (TP9) | `modprobe hcsr04` | `/dev/hcsr04` existir | 15 s |
| `S40wifi-ap` | grupo (TP9) | `brcmfmac`, IP fixo, hostapd, dnsmasq | `wlan0` aparecer | 30 s |
| `S40network` | Buildroot | `eth0` por DHCP | — | — |
| `S50pulseaudio` | Buildroot | servidor de áudio | — | — |
| `S60dockerd` | Buildroot | Docker Engine (`dockerd` em segundo plano) | — | — |
| `S95ccs-menu` | grupo | X no vt2 + `ccs-wm` + `ccs-menu`, em segundo plano; reabre o menu se cair | — | — |
| `S99ccs-console` | grupo | garante cgroup v2 e `dockerd`, `docker load` (só no 1º boot), `docker run` | `docker info` responder | 60 s |

Os scripts do grupo esperam **condições reais** (dispositivo existir, interface aparecer, Docker responder), sempre com limite, em vez de `sleep` fixo. Se o limite estourar, o script imprime `FALHOU (motivo)` no console e o boot continua.

### Otimizações aplicadas

| Otimização | Efeito |
| --- | --- |
| BusyBox init em vez de systemd | menos processos e menos MB carregados |
| `S95ccs-menu` em segundo plano | o X e o menu não seguram o boot dos serviços seguintes |
| `docker load` só quando a imagem não existe | boots normais pulam a importação do `.tar` |
| remoção de `S40xorg`, `S80dnsmasq`, `S80hostapd`, `S99tp9-distance` no post-build | nenhum serviço duplicado nem fora de ordem |
| esperas por condição com limite | sem tempo perdido em `sleep` fixo |
| imagem do container `python:3.12-slim` com uma dependência (Flask) | container sobe rápido |

### Logs e medição

| Log | Onde |
| --- | --- |
| Mensagens do kernel com timestamps | `dmesg` |
| Serviços e Docker | `/var/log/messages` (syslog; o `dockerd` usa o `dockerd-syslog-wrapper.sh`) |
| X e menu | `/var/log/ccs-menu.log` |
| Painel | `docker logs ccs-console` |
| Tempo até a aplicação | `scripts/medir-boot.sh` (no notebook) e `uptime_s` em `/api/status` |

A comparação antes/depois e os 3 boots frios ficam registrados na seção 11.2.

---

## 7. Docker

### Instalação e configuração

| Item | Valor |
| --- | --- |
| Engine | docker-engine 28.3.3, containerd 2.0.7, runc 1.3.5 (pacotes do Buildroot) |
| Init | `S60dockerd` (Buildroot) + `S99ccs-console` (grupo) |
| Storage driver | `overlay2` (padrão; `CONFIG_OVERLAY_FS` no kernel, `/var/lib/docker` no ext4) |
| cgroups | v2, montado pelo `cgroupfs-v2-mount`; o `S99ccs-console` confere e monta se faltar |
| Rede | bridge padrão `docker0`; publicação `-p 80:5000` |
| Composição | sem docker-compose: um único container, iniciado pelo script `S99ccs-console` |

### Diagrama do container

```
Celular ──Wi-Fi──► wlan0 192.168.50.1:80 ──iptables (DNAT)──► docker0 ──► ccs-console :5000 (Flask)
                                                                             │
             /etc/ccs (ro) ◄────────── lista de jogos ───────────────────────┤
             /run/ccs (rw) ◄────────── status / pedido ──────────────────────┤
             /dev/hcsr04   ◄────────── sensor (opcional) ────────────────────┘
```

### Imagem

`console-app/Dockerfile`:

```dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
EXPOSE 5000
CMD ["python", "app.py"]
```

| Item | Valor |
| --- | --- |
| Imagem base | `python:3.12-slim` (arm64), sem `latest` |
| Imagem do projeto | `ccs-console:1.0` |
| Dependência | `flask>=3.0,<4` (`requirements.txt`) |
| Geração | `docker build --platform linux/arm64` na VM, com QEMU (`tonistiigi/binfmt`) |
| Distribuição | `docker save` → `/opt/docker/ccs-console.tar` dentro da imagem Buildroot |

### Execução

```bash
docker run -d --name ccs-console \
  [--device=/dev/hcsr04:/dev/hcsr04]      # só se o driver criou o dispositivo
  -v /etc/ccs:/etc/ccs:ro \
  -v /run/ccs:/run/ccs \
  -e CCS_HOSTNAME="$(hostname)" \
  -p 80:5000 \
  --restart unless-stopped \
  ccs-console:1.0
```

| Recurso | Valor | Justificativa |
| --- | --- | --- |
| Porta | 80 do host → 5000 do container | o celular abre só `http://192.168.50.1` |
| Volume `/etc/ccs` | somente leitura | o painel lê a lista de jogos, não altera |
| Volume `/run/ccs` | leitura e escrita, tmpfs | troca de estado com o menu; volátil de propósito |
| Dispositivo `/dev/hcsr04` | só se existir | leitura do sensor do TP9, sem `--privileged` |
| Variável `CCS_HOSTNAME` | nome da placa | exibido no painel |
| Política de reinício | `unless-stopped` | o Docker reinicia o container se o Flask cair e após o boot |
| Persistência | imagem e container em `/var/lib/docker` (cartão) | depois do 1º boot, não há novo `docker load` |

### Primeiro boot (provisionamento)

No primeiro boot depois da gravação, o `S99ccs-console` não encontra `ccs-console:1.0` no Docker e faz `docker load -i /opt/docker/ccs-console.tar`. Isso acontece sem intervenção e aparece no console como `Iniciando ccs-console:`. Os boots seguintes pulam essa etapa. A medição de 15 s vale para esses boots seguintes, como prevê o edital.

### Segurança

| Ponto | Situação | Justificativa |
| --- | --- | --- |
| Modo privilegiado | não usado | — |
| Socket do Docker | não montado | o painel não controla containers |
| Dispositivos | só `/dev/hcsr04`, quando existe | leitura do sensor, função do TP9 |
| Usuário dentro do container | `root` (padrão da imagem) | o processo escreve em `/run/ccs`, de dono `root` no host; trocar para usuário próprio exige ajustar as permissões dessa pasta |
| Rede | só a porta 80 publicada | — |
| Healthcheck | não há `HEALTHCHECK` no Dockerfile | o estado é conferido por `GET /api/status`; recomendação na seção 11.3 |

---

## 8. Aplicação

O projeto tem duas partes: o **console na TV** (host) e o **painel web** (container).

### 8.1 Console na TV (`ccs-menu` e `ccs-wm`)

Pacote `ccs-menu` do BR2_EXTERNAL, em C, compilado pelo Buildroot (`generic-package`):

- **`ccs-menu`** (~1.200 linhas, SDL2, SDL2_ttf, SDL2_image): carrossel de capas a ~60 quadros/s com vsync; lê `/etc/ccs/jogos.conf` e `/etc/ccs/controles.conf`; para jogar, roda `ccs-preparar`, faz `fork()` + `exec` do `start.sh` do jogo, espera com `waitpid()` e roda `ccs-restaurar`. Para encerrar um jogo, manda `SIGTERM` ao grupo de processos e, se ele não fechar, `SIGKILL`.
- **`ccs-wm`** (~300 linhas, Xlib + RandR): gerenciador de janelas mínimo. Põe toda janela de jogo em tela cheia (`_NET_WM_STATE_FULLSCREEN`) com foco; centraliza janelas de tamanho fixo; acompanha mudança de resolução; captura **Home** (`XGrabKey`) e, segurada 1 s, escreve `parar` em `/run/ccs/pedido`.

Configuração:

| Arquivo | Formato / função |
| --- | --- |
| `/etc/ccs/jogos.conf` | `id|Nome|Motor|cor|sigla|script|gênero|descrição|resolução`; comentar a linha tira o jogo do menu |
| `/etc/ccs/controles.conf` | `id|teclas|ação`, mostrado no menu (tecla C) e na tela "Abrindo…" |
| `/etc/ccs/teclas/<id>.xmodmap` | troca de teclas de um jogo (Crash) |
| `/opt/jogos/<id>/start.sh` | como o jogo é aberto (CS: `-ref gl`; Zelda: `SHIP_HOME`) |

Manual de uso:

| Ação | Teclado | Controle de PS5 (USB-C) |
| --- | --- | --- |
| Escolher jogo | setas | direcional |
| Jogar | Enter | X |
| Ver controles do jogo | C | Triângulo |
| Voltar ao menu durante o jogo | segurar Home 1 s | Create + Options por 2 s |
| Sair para o terminal | Esc e Enter | Create e X |
| Terminal / voltar ao menu | Ctrl+Alt+F1 / Ctrl+Alt+F2 | — |

Teclas dentro dos jogos (padrão de PC): W/A/S/D andar, Espaço pular, E usar, Shift correr, Ctrl ou clique atirar, Esc menu. A troca é feita por jogo: `default.cfg` (DOOM), `autoexec.cfg` (Quake), `sm64config.txt` (Mario 64) e `xmodmap` (Crash).

### 8.2 Painel web (`console-app/`)

Flask 3, arquivo `app.py`. Acesso: conectar na rede Wi-Fi `TP9-GRUPO01` (senha `embarcados2026`) e abrir `http://192.168.50.1`. A página se atualiza sozinha.

| Rota | Método | Resposta |
| --- | --- | --- |
| `/` | GET | página do painel |
| `/api/status` | GET | **endpoint de saúde e prontidão**: `{"estado", "jogo", "nome", "desde", "uptime_s", "hostname", "sensor", "agora"}`, HTTP 200 |
| `/api/jogos` | GET | lista de jogos do `jogos.conf` |
| `/api/jogar/<id>` | POST | pede ao menu para abrir o jogo; 404 se o id não existe |
| `/api/parar` | POST | pede ao menu para fechar o jogo |
| `/api/distance` | GET | leitura do HC-SR04 (TP9); erro 503 se o sensor não existe |

| Variável de ambiente | Padrão | Função |
| --- | --- | --- |
| `CCS_JOGOS_CONF` | `/etc/ccs/jogos.conf` | lista de jogos |
| `CCS_RUN_DIR` | `/run/ccs` | pasta de troca com o menu |
| `HCSR04_DEVICE` | `/dev/hcsr04` | sensor |
| `CCS_HOSTNAME` | (definida no `docker run`) | nome exibido |

O pedido é escrito de forma atômica (`pedido.tmp` + `os.replace`), para o menu nunca ler um arquivo pela metade.

---

## 9. Geração da imagem

### Do zero até o artefato

```bash
cd ~/sistemas_embarcados2026
git checkout ccs-final
./scripts/build-console.sh
```

O `build-console.sh` executa, em ordem:

1. **Jogos:** extrai `~/j/entrega_nino.zip` em `~/j/e/entrega_nino` (se ainda não estiver extraído) e mostra o tamanho de cada jogo.
2. **Emulador ARM:** `docker run --privileged --rm tonistiigi/binfmt --install arm64` (some a cada reboot da máquina de build).
3. **Imagem do painel:** `docker build --platform linux/arm64 -t ccs-console:1.0 console-app` e `docker save` para `board/console/rootfs-overlay/opt/docker/ccs-console.tar`.
4. **Buildroot:** baixa e extrai o 2026.02.3 em `~/buildroot-2026.02.3`, se não existir.
5. **Configuração:** `make … O=~/output-console BR2_EXTERNAL=… ccs_console_raspberrypi4_defconfig`.
6. **Compilação:** `ccs-menu-dirclean` (sempre recompila o menu), recompila os pacotes do X se algum driver estiver com `BIND_NOW`, `make`, confere os drivers com `readelf` e imprime o SHA-256 da imagem.

Saída: `~/output-console/images/sdcard.img` (≈ 5 GB; partição de boot FAT de 32 MB + rootfs ext4 de 5 GB).

Variáveis opcionais: `JOGOS_ZIP`, `CCS_JOGOS_DIR`, `OUTPUT`, `BR_DIR`.

### Compactar, dividir e calcular o hash

```bash
./scripts/publicar-imagem.sh
```

Gera em `~/publicar/`: `sdcard.img.gz` dividido em partes de 360 MB (`sdcard.img.gz.001` a `.004`) e `sha256.txt`. No Windows, `JUNTAR-PARTES.bat` junta as partes.

### Limpeza e reconstrução

| Situação | Comando |
| --- | --- |
| Recompilar um pacote | `make -C ~/buildroot-2026.02.3 O=~/output-console <pacote>-dirclean` e depois `make …` |
| Mudou o `config.txt` | `make … rpi-firmware-reinstall` e depois `make …` |
| Reconstrução total | `rm -rf ~/output-console` e `./scripts/build-console.sh` |
| Limpar só o rootfs | `make … clean` (apaga a saída, mantém os downloads em `~/buildroot-2026.02.3/dl`) |

### Artefato entregue

| Arquivo | SHA-256 |
| --- | --- |
| `sdcard.img.gz` | `6f6d082a4bc7bfe6ca626687da51b5f307c6751dbbfc1ae3d9e382ac35c50398` |
| `sdcard.img` | `77ff363c2c63355bc2205168d0da42a9eb81bc5fca5db1bf97409f3de4d8099e` |

Gerado no commit `a9dd78b` da branch `console-final` em 25/11/2026. Os commits seguintes alteram apenas documentação e scripts de medição. O arquivo `sdcard.img.gz.sha256` acompanha a imagem no Drive. Um novo build gera hashes diferentes, porque a imagem inclui datas de criação dos arquivos.

---

## 10. Gravação e recuperação

### Gravar o microSD

**Windows:**

1. Baixar as 4 partes e o `JUNTAR-PARTES.bat` para a mesma pasta e executar o `.bat` (gera `sdcard.img.gz`).
2. Conferir o hash: `certutil -hashfile sdcard.img.gz SHA256` deve mostrar o valor da seção 9.
3. Raspberry Pi Imager → Raspberry Pi 4 → **Usar personalizado** → `sdcard.img.gz` → cartão → Gravar. Não aplicar personalização.

**Linux:**

```bash
cat sdcard.img.gz.0* > sdcard.img.gz
sha256sum -c sdcard.img.gz.sha256
gunzip -k sdcard.img.gz
lsblk                                            # identificar o cartão
sudo ./scripts/flash-sd.sh /dev/sdX ./sdcard.img
```

Cartão mínimo: 8 GB.

### Primeiro boot

1. Ligar HDMI, teclado/controle e alimentação (fonte 5 V 3 A).
2. O primeiro boot importa a imagem Docker (`docker load`) e demora mais. O menu aparece na TV antes do painel ficar pronto.
3. A partir do segundo boot, menu e painel sobem no tempo normal.

### Restauração

O sistema não guarda dados do usuário: restaurar é regravar o cartão com a mesma imagem. Se só o painel estiver com problema, `docker rm -f ccs-console && docker rmi ccs-console:1.0 && /etc/init.d/S99ccs-console start` refaz o provisionamento a partir do `.tar`.

### Diagnóstico (troubleshooting)

Acesso: Ctrl+Alt+F1 na TV, usuário `root`, senha `CCS1234`.

| Sintoma | Verificar | Causa e solução |
| --- | --- | --- |
| Tela preta, sem menu | `cat /var/log/ccs-menu.log` | "no screens found": conferir `99-vc4.conf` e se os drivers do X foram gerados sem `BIND_NOW` |
| Menu abre, jogo não | `cd /opt/jogos/<id>; DISPLAY=:0 sh start.sh` | mostra o erro do jogo no terminal |
| CS: "cannot open shared object file" | `ldd` em `/opt/jogos/cs16/libxash.so` | biblioteca faltando: conferir `bzip2`, `libogg`, `libvorbis` na defconfig |
| CS com imagem embaralhada | `cat /opt/jogos/cs16/start.sh` | usar `-ref gl` (`echo gl > /etc/ccs/cs-ref`) |
| Zelda: "Extractor assets not found" | `cat /opt/jogos/zelda/start.sh` | falta `export SHIP_HOME=/opt/jogos/zelda` |
| Painel não abre | `docker ps`, `docker logs ccs-console` | container parado: `/etc/init.d/S99ccs-console restart` |
| Rede `TP9-GRUPO01` não aparece | `ip addr show wlan0`, `ps | grep hostapd` | `/etc/init.d/S40wifi-ap restart` |
| Sem som | `amixer`, `aplay -l` | volume ou saída HDMI |
| Jogo travado | segurar Home 1 s | se não voltar: `kill` do processo no terminal |

---

## 11. Testes

### 11.1 Plano de testes e resultados

| # | Teste | Procedimento | Esperado | Resultado (25/11/2026) |
| --- | --- | --- | --- | --- |
| 1 | Boot até o menu | ligar a Pi com HDMI | menu do CCS na TV, sem login | OK |
| 2 | Jogos | abrir cada um dos 6 jogos pelo menu | jogo em tela cheia, com som | OK (CS e Zelda após as correções da imagem final) |
| 3 | Voltar ao menu | segurar Home 1 s em cada jogo | volta ao menu, resolução e teclas restauradas | OK |
| 4 | Controles | tecla C no menu | lista de controles do jogo | OK |
| 5 | Rede | celular em `TP9-GRUPO01` | recebe IP `192.168.50.x` | OK |
| 6 | Painel | abrir `http://192.168.50.1` | jogo atual e botões | OK |
| 7 | Comandos pelo painel | botão de um jogo / Encerrar jogo | o jogo abre e fecha na TV | OK |
| 8 | Saúde | `curl http://192.168.50.1/api/status` | HTTP 200 com JSON | OK |
| 9 | Operação offline | sem cabo de rede e sem Internet | tudo funciona | OK (a rede é a própria Pi) |
| 10 | Reboot | `reboot` | menu e painel voltam sozinhos | OK |
| 11 | Reinício do container após falha | `kill -9 $(docker inspect -f '{{.State.Pid}}' ccs-console)` e aguardar | o Docker reinicia o container; painel volta | a registrar |
| 12 | Perda de energia | tirar da tomada com um jogo aberto e religar | sistema sobe normalmente (ext4 com journal) | a registrar |
| 13 | Persistência | 2º boot após gravar | não repete o `docker load` | a registrar |
| 14 | Boot frio < 15 s (3×) | seção 11.2 | 3 tempos < 15,0 s | a registrar |

### 11.2 Medição do tempo de boot

Critério de fim: primeira resposta HTTP 200 de `GET /api/status`, pedida por outro equipamento na rede da Pi (edital, seção 7). Como a aplicação é acessada pelo Wi-Fi da própria Pi, o tempo de associação à rede está incluído.

Procedimento:

1. Fazer um boot de provisionamento e desligar com `poweroff`; tirar da tomada.
2. No notebook (já configurado para entrar na rede `TP9-GRUPO01` automaticamente), rodar `./scripts/medir-boot.sh` e ligar a Pi na tomada no mesmo instante, filmando a tomada e a tela do notebook no mesmo vídeo, sem cortes.
3. O script para na primeira resposta e grava `boot-<data>.log` com o tempo total e o `uptime_s` da Pi.
4. Repetir 3 vezes.

| Boot | Tempo total (tomada → HTTP 200) | `uptime_s` da Pi | Log |
| --- | --- | --- | --- |
| 1 | a registrar | | |
| 2 | a registrar | | |
| 3 | a registrar | | |

Análise de onde vai o tempo: `dmesg` (kernel), `/var/log/messages` (serviços) e os horários impressos pelos scripts. Se algum boot passar de 15 s, as próximas otimizações, em ordem de ganho esperado, são: subir o `S99ccs-console` antes do `S95ccs-menu` (o X e o Mesa disputam CPU e cartão com o `dockerd`); `boot_delay=0` e `disable_splash=1` no `config.txt`; `quiet` no `cmdline.txt`; adiar o `S50pulseaudio`.

### 11.3 Limitações conhecidas

- Os jogos não estão no repositório (direitos autorais); sem o `entrega_nino.zip` a imagem é gerada, mas os jogos não abrem.
- O Zelda depende do `oot.o2r`, gerado a partir de uma ROM própria.
- O CS foi feito para teclado e mouse; no controle o suporte é parcial. O DOOM usa o teclado por padrão.
- Sem Bluetooth: o controle de PS5 precisa de cabo USB-C.
- O terminal (tty) usa layout americano; o X usa ABNT2.
- O container não tem `HEALTHCHECK` no Dockerfile. Melhoria prevista: `HEALTHCHECK --interval=10s CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:5000/api/status')"`.
- A versão da imagem base usa a tag `3.12-slim` (versão menor fixa) e o Flask por faixa (`>=3.0,<4`). Fixar com digest (`python:3.12-slim@sha256:…`) e `flask==<versão>` deixaria o build totalmente determinístico.
- O painel usa HTTP sem TLS e o servidor de desenvolvimento do Flask; as senhas estão em texto no repositório; os processos rodam como `root`. Aceitável num trabalho acadêmico, não num produto.
- O hostname ficou `CSS-console` (letras trocadas na defconfig).

---

## 12. Licenças e versões

### Software da imagem (versões do Buildroot 2026.02.3)

| Componente | Versão | Licença | Fonte |
| --- | --- | --- | --- |
| Buildroot | 2026.02.3 | GPL-2.0 | buildroot.org |
| Linux (Raspberry Pi) | 6.12.61, commit `21b41014` | GPL-2.0 | github.com/raspberrypi/linux |
| Toolchain Bootlin (GCC 14, glibc) | aarch64 glibc stable 2025.08-1 | GPL-3.0 (GCC), LGPL-2.1 (glibc) | toolchains.bootlin.com |
| rpi-firmware | 063bcab6 | BSD-3-Clause (com binários da Broadcom) | github.com/raspberrypi/firmware |
| brcmfmac-sdio-firmware-rpi | 6596d332 | proprietária (redistribuível) | github.com/RPi-Distro/firmware-nonfree |
| BusyBox | 1.37.0 | GPL-2.0 | busybox.net |
| eudev | 3.2.14 | GPL-2.0+ / LGPL-2.1+ | github.com/eudev-project/eudev |
| docker-engine / docker-cli | 28.3.3 | Apache-2.0 | github.com/moby/moby |
| containerd | 2.0.7 | Apache-2.0 | github.com/containerd/containerd |
| runc | 1.3.5 | Apache-2.0 | github.com/opencontainers/runc |
| Mesa 3D | 26.0.1 | MIT | mesa3d.org |
| libdrm | 2.4.131 | MIT | dri.freedesktop.org |
| libglvnd | 1.7.0 | licença libglvnd (estilo MIT) | gitlab.freedesktop.org/glvnd |
| X.org server | 21.1.23 | MIT/X11 | x.org |
| xinit / xrandr / xmodmap | 1.4.4 / 1.5.3 / 1.0.11 | MIT/X11 | x.org |
| xf86-input-evdev / libinput / video-fbdev | 2.11.0 / 1.5.0 / 0.5.1 | MIT/X11 | x.org |
| libX11 / libXrandr / libxshmfence | 1.8.12 / 1.5.4 / 1.3.3 | MIT/X11 | x.org |
| libepoxy | 1.5.10 | MIT | github.com/anholt/libepoxy |
| SDL2 / SDL2_ttf / SDL2_image | 2.32.10 / 2.24.0 / 2.8.12 | Zlib | libsdl.org |
| FreeType | 2.14.3 | FTL ou GPL-2.0+ | freetype.org |
| libpng | 1.6.58 | Libpng-2.0 | libpng.org |
| libjpeg-turbo | 3.1.2 | IJG / BSD-3-Clause | libjpeg-turbo.org |
| DejaVu fonts | 2.37 | Bitstream Vera / domínio público | dejavu-fonts.github.io |
| Inter (fonte do menu) | — | SIL OFL 1.1 | rsms.me/inter |
| PulseAudio | 17.0 | LGPL-2.1+ | freedesktop.org |
| alsa-utils | 1.2.15.2 | GPL-2.0 | alsa-project.org |
| mpg123 | 1.33.4 | LGPL-2.1 | mpg123.de |
| libogg / libvorbis | 1.3.6 / 1.3.7 | BSD-3-Clause | xiph.org |
| bzip2 | 1.0.8 | licença bzip2 | sourceware.org/bzip2 |
| xz-utils | 5.8.3 | 0BSD / domínio público / GPL | tukaani.org/xz |
| zlib | 1.3.2 | Zlib | zlib.net |
| libusb | 1.0.30 | LGPL-2.1+ | libusb.info |
| libcurl / curl | 8.20.0 | curl (MIT-like) | curl.se |
| hostapd | 2.11 | BSD-3-Clause | w1.fi |
| dnsmasq | 2.92rel2 | GPL-2.0 ou GPL-3.0 | thekelleys.org.uk |
| iw | 6.17 | ISC | kernel.org |
| kmod | 33 | LGPL-2.1+ | kernel.org |

### Software do grupo

| Componente | Licença | Local |
| --- | --- | --- |
| `ccs-menu`, `ccs-wm` | MIT (declarada no `ccs-menu.mk`) | `br2-external-tp9/package/ccs-menu` |
| `hcsr04-driver` | GPL-2.0 (módulo de kernel) | `br2-external-tp9/package/hcsr04-driver` |
| scripts, init, painel web | do grupo | `br2-external-tp9/board`, `scripts`, `console-app` |
| Arte (logo e capas) | original do grupo | `br2-external-tp9/board/console/arte` |

### Container

| Componente | Versão | Licença |
| --- | --- | --- |
| Imagem base `python:3.12-slim` (Debian) | Python 3.12 | PSF-2.0 (Python), licenças Debian |
| Flask | 3.x (`>=3.0,<4`) | BSD-3-Clause |

### Jogos (fora do repositório)

| Jogo | Motor | Licença do motor | Dados do jogo |
| --- | --- | --- | --- |
| Counter-Strike 1.6 | Xash3D FWGS (github.com/FWGS/xash3d-fwgs) | ver repositório oficial | Valve, protegidos |
| Super Mario 64 | sm64ex (github.com/sm64pc/sm64ex) | ver repositório oficial | Nintendo, protegidos |
| Zelda: Ocarina of Time | Ship of Harkinian (github.com/HarbourMasters/Shipwright) | ver repositório oficial | Nintendo, protegidos (`oot.o2r`) |
| Crash Bandicoot | PCSX-ReARMed (github.com/notaz/pcsx_rearmed) | GPL-2.0 | Sony, protegidos (ROM e BIOS) |
| DOOM | Chocolate Doom (github.com/chocolate-doom/chocolate-doom) | GPL-2.0 | id Software |
| Quake | QuakeSpasm (github.com/sezero/quakespasm) | GPL-2.0 | id Software |

Os dados dos jogos não são redistribuídos pelo repositório; foram usados apenas para a demonstração acadêmica.

---

## Anexo A — Componentes externos

| Componente | Uso | Ligação |
| --- | --- | --- |
| Raspberry Pi 4 Model B 4 GB | plataforma | — |
| Fonte USB-C 5 V / 3 A | alimentação | USB-C |
| microSD 8 GB ou mais (classe 10) | armazenamento | slot microSD |
| TV ou monitor HDMI | saída de vídeo e áudio | micro-HDMI 0 |
| Teclado e mouse USB | controle do menu e dos jogos | USB |
| Controle Sony DualSense (PS5) | controle dos jogos | cabo USB-C → USB |
| Celular ou notebook | painel web e medição de boot | Wi-Fi `TP9-GRUPO01` |
| Sensor HC-SR04 + 3 resistores de 1 kΩ (opcional, TP9) | leitura de distância em `/api/distance` | GPIO23 (TRIG), GPIO24 (ECHO via divisor), 5 V, GND |

## Anexo B — Estrutura do repositório e correspondência com o edital (5.1)

| Recomendado no edital | Neste repositório |
| --- | --- |
| `buildroot-external/` | `br2-external-tp9/` |
| `configs/<projeto>_rpi4_defconfig` | `br2-external-tp9/configs/ccs_console_raspberrypi4_defconfig` |
| `package/` | `br2-external-tp9/package/` (`ccs-menu`, `hcsr04-driver`) |
| `rootfs-overlay/` | `br2-external-tp9/board/rpi4/rootfs-overlay`, `br2-external-tp9/board/console/rootfs-overlay` |
| `kernel-config/` | não há fragmento próprio (seção 5) |
| `docker/` | `console-app/Dockerfile` + `board/console/rootfs-overlay/etc/init.d/S99ccs-console` |
| `application/` | `console-app/` (painel) e `br2-external-tp9/package/ccs-menu/src` (menu) |
| `scripts/` | `scripts/` (`build-console.sh`, `publicar-imagem.sh`, `flash-sd.sh`, `medir-boot.sh`, `test.sh`) |
| `docs/` | `docs/` (este manual, `console.md`, documentação do TP9) |
| `README.md` | `README.md` |
