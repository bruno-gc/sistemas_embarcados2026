# CCS — Console Contra Sony (projeto final)

**Sistemas Embarcados · Raspberry Pi 4 · Grupo 01**

Imagem Linux embarcada, gerada integralmente pelo Buildroot para a Raspberry Pi 4, que transforma a placa num **console de videogame**. Ao ligar, a TV mostra um menu gráfico com 6 jogos compilados para ARM64. Cada jogo abre em tela cheia, com as teclas no padrão dos jogos de PC, e segurar **Home** por 1 segundo volta ao menu. Um **painel web** em Flask, dentro de um container Docker, mostra o que está na TV e abre ou encerra jogos pelo celular, em `http://192.168.50.1`.

Tudo sobe sozinho no boot, sem nenhum comando manual e sem depender de Internet. A imagem mantém o que foi entregue no TP9: driver `hcsr04.ko`, rede Wi-Fi `TP9-GRUPO01` e Docker.

---

## Identificação

| Item | Valor |
| --- | --- |
| Disciplina | Sistemas Embarcados |
| Professor | Sergio Schina |
| Versão do Buildroot | **2026.02.3** |
| Hardware | Raspberry Pi 4 Model B, TV ou monitor HDMI, teclado e mouse USB, controle de PS5 (cabo USB-C), microSD de 8 GB ou mais |
| Hostname | `ccs-console` |
| SSID | `TP9-GRUPO01` (senha `embarcados2026`) |
| URL do painel | `http://192.168.50.1` |
| Branch | `console-final` |
| Versão entregue | tag `ccs-final` |

### Integrantes

| Nome | RA | Responsabilidade |
| --- | --- | --- |
| Vinicius Alves Santana | 232710 | Buildroot, menu (`ccs-menu` e `ccs-wm`), painel web, scripts de boot e de build, imagem e documentação |
| Bruno Garcia Carvalho | 226790 | Repositório, testes na Raspberry Pi, controle de PS5 e evidências |
| Matheus Correia Caumo | 232577 | Painel web e container Docker (base do TP9), testes |
| Wecton | 228550 | Compilação dos 6 jogos para ARM64 (`entrega_nino.zip`) |

---

## Resultado funcional

1. A Raspberry Pi 4 inicia a partir da imagem gerada pelo Buildroot.
2. O X sobe no vt2, na saída HDMI, e abre o menu do console (`ccs-menu`).
3. O menu mostra um carrossel com as capas dos 6 jogos. Teclado, mouse e controle USB funcionam.
4. Ao escolher um jogo, aparece a tela "Abrindo…" com os controles dele, e o jogo abre em **tela cheia**.
5. Segurar **Home** por 1 s (ou **Select + Start** por 2 s no controle) fecha o jogo e volta ao menu.
6. A Raspberry cria a rede Wi-Fi `TP9-GRUPO01`, com o IP `192.168.50.1`.
7. O Docker Engine inicia e sobe o container `ccs-console`, com o painel web na porta 80.
8. Um celular conectado à rede abre `http://192.168.50.1`, vê o que está na TV e abre ou encerra jogos.
9. Depois de um reboot, tudo volta a funcionar sem comandos manuais.

---

## Jogos

| Jogo | Motor (ARM64) | Pasta na Raspberry | Atalho no terminal |
| --- | --- | --- | --- |
| Counter-Strike 1.6 | Xash3D FWGS | `/opt/jogos/cs16` | `jogar-cs` |
| Super Mario 64 | sm64ex | `/opt/jogos/sm64` | `jogar-mario` |
| Zelda: Ocarina of Time | Ship of Harkinian | `/opt/jogos/zelda` | `jogar-zelda` |
| Crash Bandicoot | PCSX-ReARMed | `/opt/jogos/crash` | `jogar-crash` |
| DOOM | Chocolate Doom | `/opt/jogos/doom` | `jogar-doom` |
| Quake | QuakeSpasm | `/opt/jogos/quake` | `jogar-quake` |

Os jogos **não ficam no GitHub**: são grandes (~1,9 GB) e incluem arquivos com direitos autorais (ROMs, BIOS, dados dos jogos). O `scripts/build-console.sh` usa a pasta dos jogos como mais um *rootfs overlay* no momento do build. Para tirar um jogo do menu (por exemplo, o CS), basta comentar a linha dele em `etc/ccs/jogos.conf`.

---

## Arquitetura

```
TV (HDMI)                                     Celular / notebook (TP9-GRUPO01)
   ▲                                                 │ http://192.168.50.1
   │                                                 ▼
Xorg (vt2) ── ccs-wm (janelas em tela cheia)    Container ccs-console
   │                                             (Flask :5000, -p 80:5000)
   └── ccs-menu (SDL2) ◄──── /run/ccs ────────►  -v /run/ccs  -v /etc/ccs:ro
          │  status · pedido · sair
          │ fork + exec
          ▼
   ccs-preparar → /opt/jogos/<jogo>/start.sh → ccs-restaurar
   (resolução e teclas do jogo)                 (volta ao padrão)
```

O menu e o painel conversam só por arquivos em `/run/ccs` (tmpfs):

| Arquivo | Quem escreve | Conteúdo |
| --- | --- | --- |
| `/run/ccs/status` | menu | `{"estado": "menu"/"jogando"/"terminal", "jogo": ..., "nome": ...}` |
| `/run/ccs/pedido` | painel web, `jogar-*` ou `ccs-wm` | `jogar <id>` ou `parar` |
| `/run/ccs/sair` | menu | criado quando o usuário escolhe "Sair para o terminal" |

O container **não usa `--privileged`**: recebe só a lista de jogos (somente leitura), a pasta `/run/ccs` e, se existir, o `/dev/hcsr04`.

---

## O menu e o gerenciador de janelas

**`ccs-menu`** (C, SDL2 + SDL2_ttf, pacote próprio em `br2-external-tp9/package/ccs-menu`):

- lê a lista de jogos de `/etc/ccs/jogos.conf` e os controles de `/etc/ccs/controles.conf`;
- desenha o carrossel: o jogo escolhido fica maior, com uma aura na cor dele, o fundo troca com fade e partículas sobem devagar;
- a tecla **C** (Triângulo no controle) mostra os controles do jogo; a tela "Abrindo…" também mostra;
- para jogar, roda `ccs-preparar` (resolução e teclas do jogo), faz `fork` + `exec` do `start.sh`, espera o jogo terminar e roda `ccs-restaurar`;
- escreve o estado em `/run/ccs/status` e atende os pedidos do painel em `/run/ccs/pedido`.

**`ccs-wm`** (C, Xlib, mesmo pacote): gerenciador de janelas mínimo. Coloca toda janela de jogo em tela cheia (`_NET_WM_STATE_FULLSCREEN`), dá o foco do teclado a ela, centraliza janelas de tamanho fixo e acompanha mudanças de resolução (RandR). Ele também captura a tecla **Home**: segurada por 1 s, escreve `parar` em `/run/ccs/pedido`, e o menu encerra o jogo.

### Teclas no padrão de PC

| Ação | Teclas |
| --- | --- |
| Andar | W/A/S/D (setas também) |
| Pular / confirmar | Espaço |
| Usar / interagir | E |
| Atirar | Clique ou Ctrl |
| Correr | Shift |
| Pausa / menu | Enter / Esc |
| Voltar ao menu do CCS | segurar **Home** por 1 s |

Cada motor lê as teclas de um jeito, então a troca é feita por jogo: arquivos de configuração do DOOM (`chocolate-doom/default.cfg`), do Quake (`id1/autoexec.cfg`) e do Mario 64 (`sm64config.txt`); no Crash, que lê o teclado pelo nome das teclas, o `ccs-preparar` aplica um `xmodmap` e o `ccs-restaurar` desfaz. O Crash também roda em 640x480 (`ccs-resolucao`), e o monitor amplia a imagem.

### Controle de PS5

Ligue por **cabo USB-C**. O kernel traz o driver `hid-playstation`, que o eudev carrega sozinho. No menu: D-pad escolhe, **X** joga, **Triângulo** mostra os controles e **Create + Options** por 2 s volta ao menu durante o jogo. Mario 64, Zelda, Crash e Quake leem o controle direto; o CS foi feito para teclado e mouse.

---

## Integração ao Buildroot

```
br2-external-tp9/
├── configs/
│   ├── tp9_raspberrypi4_defconfig          ← TP9
│   └── ccs_console_raspberrypi4_defconfig  ← projeto final
├── external.mk                             ← inclui os pacotes e o xorg-lazy.mk
├── xorg-lazy.mk                            ← drivers do X com -z lazy
├── package/
│   ├── hcsr04-driver/                      ← driver do TP9
│   └── ccs-menu/                           ← ccs-menu.c e ccs-wm.c
└── board/
    ├── rpi4/                               ← Wi-Fi AP, usuários, overlay do TP9
    └── console/
        ├── config.txt                      ← vc4-kms-v3d, áudio, hdmi
        ├── post-build.sh
        ├── arte/                           ← gera logo, capas e fundos
        └── rootfs-overlay/
            ├── etc/ccs/                    ← jogos.conf, controles.conf, teclas/
            ├── etc/X11/xorg.conf.d/        ← GPU vc4, teclado ABNT2
            ├── etc/init.d/                 ← S95ccs-menu, S99ccs-console
            └── usr/bin/                    ← ccs-sessao, ccs-preparar, jogar-*
```

Os rootfs overlays entram nesta ordem: `board/rpi4`, a pasta dos jogos (`CCS_JOGOS_DIR`) e `board/console`. Assim, os arquivos do grupo (por exemplo, o `start.sh` do CS e do Zelda) substituem os que vieram com os jogos.

### Pacotes adicionados em relação ao TP9

| Pacote | Para quê |
| --- | --- |
| `xorg7`, `xserver_xorg-server`, `xinit`, `xrandr`, `xmodmap`, `xf86-input-evdev/libinput`, `xf86-video-fbdev` | servidor gráfico, resolução e troca de teclas |
| `mesa3d` (V3D, VC4, GBM, GLX, EGL, GLES) + `libglvnd` | OpenGL acelerado pela GPU da Raspberry Pi 4 |
| `libxshmfence`, `libepoxy` | DRI3 e glamor: aceleração dentro do X |
| `sdl2` (X11, KMS/DRM, OpenGL, GLES), `sdl2_ttf` | jogos e menu |
| `alsa-utils`, `mpg123`, `pulseaudio` | áudio |
| `bzip2`, `libogg`, `libvorbis` | bibliotecas exigidas pelo motor do CS (`libxash.so`) |
| `eudev` | hotplug de teclado, mouse e controle |
| `dejavu` | fonte reserva (o menu usa a Inter, licença OFL) |
| `brcmfmac_sdio-firmware-rpi` (Wi-Fi) | firmware do Wi-Fi onboard |
| `ccs-menu` (pacote do grupo) | menu e gerenciador de janelas |

---

## Docker e painel web

```bash
docker run -d --name ccs-console \
  -p 80:5000 \
  -v /run/ccs:/run/ccs -v /etc/ccs:/etc/ccs:ro \
  --device=/dev/hcsr04:/dev/hcsr04 \
  --restart unless-stopped \
  ccs-console:1.0
```

O `--device` só entra se o `/dev/hcsr04` existir (sensor do TP9 ligado). A imagem `ccs-console:1.0` (Python 3.12 slim, **arm64**) é gerada no build, exportada com `docker save` para `/opt/docker/ccs-console.tar` e carregada com `docker load` no primeiro boot, sem Internet.

| Rota | O que faz |
| --- | --- |
| `GET /` | painel: jogo na TV e botões dos jogos |
| `GET /api/jogos` | lista de jogos |
| `GET /api/status` | o que está na TV agora |
| `POST /api/jogar/<id>` | abre o jogo na TV |
| `POST /api/parar` | fecha o jogo e volta ao menu |
| `GET /api/distance` | distância do HC-SR04 (TP9), se o sensor estiver ligado |

---

## Inicialização automática e ordem de boot

```
Boot
 ↓
S10udevd        → dispositivos de entrada (teclado, mouse, controle)
 ↓
S30hcsr04       → driver do sensor (TP9)
 ↓
S40wifi-ap      → rede TP9-GRUPO01 em 192.168.50.1
 ↓
S50pulseaudio   → áudio dos jogos
 ↓
S60dockerd      → Docker Engine
 ↓
S95ccs-menu     → xinit + ccs-sessao: ccs-wm e ccs-menu no vt2 (reabre se cair)
 ↓
S99ccs-console  → docker load (se preciso) e docker run do painel
```

O terminal continua no vt1: **Ctrl+Alt+F1** mostra o login e **Ctrl+Alt+F2** volta ao menu. Usuário `root`, senha `CCS1234`.

---

## Procedimento de compilação

Máquina de build: VM no VirtualBox com Ubuntu Server 26.04.1 LTS (x86_64), a mesma do TP9 (preparação em [docs/tp9.md](docs/tp9.md#procedimento-de-compilação)). Os jogos ficam em `~/j/entrega_nino.zip`.

```bash
git clone https://github.com/bruno-gc/sistemas_embarcados2026.git
cd sistemas_embarcados2026
git checkout console-final
./scripts/build-console.sh
```

O `build-console.sh`:

1. extrai os jogos (`CCS_JOGOS_DIR`, padrão `~/j/e/entrega_nino`) e mostra o tamanho de cada um;
2. registra o emulador ARM (`tonistiigi/binfmt`);
3. gera a imagem Docker `ccs-console:1.0` para `linux/arm64` e a exporta para o rootfs overlay;
4. baixa o Buildroot 2026.02.3, se ainda não existir;
5. aplica a `ccs_console_raspberrypi4_defconfig` com o `BR2_EXTERNAL` do projeto;
6. recompila o `ccs-menu`, compila a imagem, confere com `readelf` que os drivers do X não ficaram com BIND_NOW e imprime os hashes.

A imagem final fica em `~/output-console/images/sdcard.img` (2,3 GB). O `scripts/publicar-imagem.sh` compacta e divide a imagem em partes de 360 MB para o Drive.

---

## Procedimento de gravação da imagem

**Windows:** juntar as partes com o `JUNTAR-PARTES.bat` (gera `sdcard.img.gz`) → Raspberry Pi Imager → *Dispositivo:* Raspberry Pi 4 → *Sistema operacional:* **Usar personalizado** → `sdcard.img.gz` → escolher o microSD → *Gravar*. Não aplicar personalização.

**Linux:**

```bash
lsblk
sudo ./scripts/flash-sd.sh /dev/sdX
```

---

## Procedimento de teste

| # | Onde | Comando ou ação | Esperado |
| --- | --- | --- | --- |
| 1 | Raspberry | ligar com HDMI, teclado e controle | menu do CCS na TV |
| 2 | Menu | setas e **C** | escolhe o jogo e mostra os controles |
| 3 | Menu | Enter num jogo | tela "Abrindo…" e jogo em tela cheia |
| 4 | Jogo | segurar **Home** por 1 s | volta ao menu |
| 5 | Raspberry | Ctrl+Alt+F1, login `root` | terminal (Ctrl+Alt+F2 volta ao menu) |
| 6 | Raspberry | `docker ps` | `ccs-console` ativo, `0.0.0.0:80->5000/tcp` |
| 7 | Raspberry | `cat /run/ccs/status` | estado do menu em JSON |
| 8 | Cliente | conectar em `TP9-GRUPO01` e abrir `http://192.168.50.1` | painel com o jogo atual |
| 9 | Cliente | botão de um jogo / **Encerrar jogo** | o jogo abre e fecha na TV |
| 10 | Raspberry | `reboot` | menu e painel voltam sozinhos |

---

## Problemas encontrados e soluções adotadas

| Problema | Solução |
| --- | --- |
| X com "no screens found": na Pi 4 o `card0` é o v3d (só renderiza) e a HDMI é o `card1` (vc4) | `99-vc4.conf` marca o vc4 como GPU principal (`PrimaryGPU`) |
| Drivers do X falhando com "undefined symbol": o Buildroot linka com RELRO completo (`-z now`) | `xorg-lazy.mk` compila os pacotes do X com `-Wl,-z,lazy`; o build confere com `readelf` |
| Cada jogo abria de um tamanho (cortado, pequeno ou em janela) | `ccs-wm` põe toda janela em tela cheia; `ccs-resolucao` muda a resolução por jogo (Crash em 640x480) |
| Teclas diferentes em cada jogo (no Crash, o Z confirmava) e nenhuma lista na tela | padrão de PC (WASD, Espaço, E, Shift, Esc) por arquivo de config ou `xmodmap`; lista de controles no menu e na tela "Abrindo…" |
| Sem um jeito comum de sair dos jogos | `ccs-wm` captura **Home** segurado 1 s e pede ao menu para fechar o jogo |
| Janelas do SDL tratadas como diálogos pelo `ccs-wm` | leitura direta da propriedade `WM_TRANSIENT_FOR` em vez de `XGetTransientForHint` |
| CS não abria: faltavam `libbz2.so.1.0`, `libogg` e `libvorbis` | pacotes `bzip2`, `libogg` e `libvorbis` no defconfig |
| CS com imagem embaralhada no renderizador `gles2` (`GL_INVALID_ENUM`) | `start.sh` usa o renderizador `gl` (OpenGL pela Mesa V3D) |
| Zelda: "Extractor assets not found", porque procurava o `oot.o2r` fora da pasta do jogo | `start.sh` exporta `SHIP_HOME=/opt/jogos/zelda` |
| O firmware do Wi-Fi não estava na imagem do TP9 | pacote `brcmfmac_sdio-firmware-rpi` |
| A VM de build travava durante builds longos e derrubava a sessão | build em segundo plano com `nohup` e log em arquivo |

---

## Limitações conhecidas

- Os jogos não estão no repositório, por direitos autorais. Sem o `entrega_nino.zip`, a imagem é gerada, mas os jogos não abrem.
- O Zelda depende do `oot.o2r`, gerado a partir de uma ROM própria.
- O CS foi feito para teclado e mouse; no controle o suporte é parcial. O DOOM usa o teclado por padrão.
- A imagem não tem Bluetooth: o controle de PS5 precisa de cabo USB-C.
- O terminal (tty) usa layout americano; o X usa ABNT2.
- O painel usa HTTP sem criptografia e o servidor de desenvolvimento do Flask. As senhas estão em texto puro no repositório, aceitável num trabalho acadêmico, mas não num produto.

---

## Hashes dos artefatos finais (SHA-256)

| Artefato | SHA-256 |
| --- | --- |
| `sdcard.img` | `77ff363c2c63355bc2205168d0da42a9eb81bc5fca5db1bf97409f3de4d8099e` |
| `sdcard.img.gz` | `6f6d082a4bc7bfe6ca626687da51b5f307c6751dbbfc1ae3d9e382ac35c50398` |

Imagem gerada no commit `a9dd78b` (25/11/2026), publicada no Drive do grupo em 4 partes. Os commits seguintes até a tag `ccs-final` só mudam documentação. Um novo build gera hashes diferentes, porque a imagem inclui datas de criação dos arquivos.

---

## Versão final entregue

| Item | Valor |
| --- | --- |
| Repositório | https://github.com/bruno-gc/sistemas_embarcados2026 |
| Branch | `console-final` |
| Tag | `ccs-final` |
| Buildroot | 2026.02.3 |
| Defconfig | `ccs_console_raspberrypi4_defconfig` |
| SSID | `TP9-GRUPO01` |
| URL | `http://192.168.50.1` |

Documentação detalhada do console em [docs/console.md](docs/console.md). As entregas anteriores continuam disponíveis: TP8 na tag `tp8-final` e TP9 em [docs/tp9.md](docs/tp9.md).
