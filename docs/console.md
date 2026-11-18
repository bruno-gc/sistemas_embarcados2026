# CCS — Console Contra Sony (projeto final)

Imagem Buildroot 2026.02.3 para a Raspberry Pi 4 que liga direto num **menu gráfico de console** na HDMI. O menu abre 6 jogos compilados para ARM64. Um **painel web** em Flask, num container Docker, mostra o que está na TV e abre ou encerra jogos pelo celular.

A imagem também mantém o que foi feito no TP9: driver `hcsr04.ko`, rede `TP9-GRUPO01` e Docker. O firmware do Wi‑Fi agora vai na imagem.

## Jogos

| Jogo | Motor (ARM64) | Pasta na Raspberry | Atalho no terminal |
| --- | --- | --- | --- |
| Counter-Strike 1.6 | Xash3D FWGS | `/opt/jogos/cs16` | `jogar-cs` |
| Super Mario 64 | sm64ex | `/opt/jogos/sm64` | `jogar-mario` |
| Zelda: Ocarina of Time | Ship of Harkinian | `/opt/jogos/zelda` | `jogar-zelda` |
| Crash Bandicoot | PCSX-ReARMed | `/opt/jogos/crash` | `jogar-crash` |
| DOOM | Chocolate Doom | `/opt/jogos/doom` | `jogar-doom` |
| Quake | QuakeSpasm | `/opt/jogos/quake` | `jogar-quake` |

Os jogos foram compilados por Wecton e entregues no `entrega_nino.zip`. Eles **não ficam no GitHub**: são grandes (~1,9 GB) e incluem arquivos com direitos autorais (ROMs, BIOS, dados dos jogos). O `scripts/build-console.sh` usa os jogos como mais um *rootfs overlay* no momento do build.

Para tirar um jogo do menu (por exemplo, o CS), basta comentar a linha dele em `br2-external-tp9/board/console/rootfs-overlay/etc/ccs/jogos.conf`.

## Identidade visual

O menu e o painel web usam arte **original do grupo**, desenhada em vetor por `br2-external-tp9/board/console/arte/gerar.py`: o logo e o emblema do CCS, uma capa por jogo, fundos desfocados e texturas de efeito. As capas não copiam capas, logos ou personagens oficiais, porque esse material tem direitos autorais e não pode ser redistribuído; cada uma traz uma cena que lembra o gênero do jogo. A fonte é a Inter (licença OFL, que permite redistribuição). Os PNGs gerados ficam em `rootfs-overlay/usr/share/ccs` e `console-app/static/arte`.

Para mudar a arte, edite `capas.py` e rode `python3 gerar.py`. O script precisa de Playwright e Pillow e só roda na máquina de desenvolvimento.

O menu é um carrossel de capas. O jogo escolhido fica maior, com uma aura pulsando na cor dele, o fundo da tela troca com fade e partículas sobem devagar. Embaixo aparecem o nome, o gênero, o motor, uma descrição curta e o botão **JOGAR**. No rodapé ficam as teclas e o endereço do painel web.

## Arquitetura

```
TV (HDMI)                                  Celular / notebook
   ▲                                              │ http://IP_DA_RASPBERRY
   │                                              ▼
Xorg (vt2) ── ccs-menu (SDL2) ──┐       Container ccs-console (Flask :5000)
                  │             │           │  -p 80:5000
                  │ fork        │ /run/ccs  │  -v /run/ccs  -v /etc/ccs:ro
                  ▼             └───────────┘  --device=/dev/hcsr04 (se existir)
        /opt/jogos/<jogo>/start.sh
```

O menu e o painel conversam só por arquivos em `/run/ccs`:

| Arquivo | Quem escreve | Conteúdo |
| --- | --- | --- |
| `/run/ccs/status` | menu | `{"estado": "menu"/"jogando"/"terminal", "jogo": ..., "nome": ...}` |
| `/run/ccs/pedido` | painel web ou `jogar-*` | `jogar <id>` ou `parar` |
| `/run/ccs/sair` | menu | criado quando o usuário escolhe "Sair para o terminal" |

O container não precisa de `--privileged`. Ele recebe só a lista de jogos (somente leitura), a pasta `/run/ccs` e, se existir, o `/dev/hcsr04`.

## Ordem de boot

| Script | O que faz |
| --- | --- |
| `S10udevd` | eudev: cria os dispositivos de entrada (teclado, mouse, controle) para o X |
| `S30hcsr04` | carrega o driver do sensor (TP9) |
| `S40wifi-ap` | sobe a rede `TP9-GRUPO01` em `192.168.50.1` |
| `S50pulseaudio` | servidor de áudio usado pelos jogos |
| `S60dockerd` | Docker Engine |
| `S95ccs-menu` | X no vt2 com o `ccs-menu`; se o menu cair, abre de novo |
| `S99ccs-console` | carrega `ccs-console.tar` e sobe o painel web na porta 80 |

O login em texto continua no vt1: `Ctrl+Alt+F1` mostra o terminal e `Ctrl+Alt+F2` volta ao menu. O usuário é `root`, e a senha é `CCS1234`.

## Controles do menu

| Ação | Teclado | Controle USB |
| --- | --- | --- |
| Escolher jogo | setas | D-pad |
| Jogar | Enter | A ou Start |
| Sair para o terminal | Esc e depois Enter | Select e depois A |

Para voltar ao menu, saia do jogo pelo menu do próprio jogo. Outra opção é o botão **Encerrar jogo** no painel web.

## Gerar a imagem

Na VM de build, com o repositório atualizado e o `entrega_nino.zip` em `~/j/`:

```bash
cd ~/sistemas_embarcados2026
git checkout console-final && git pull
./scripts/build-console.sh
```

O resultado fica em `~/output-console/images/sdcard.img` (~5 GB). Grave com o Raspberry Pi Imager ("Use custom") num cartão de 8 GB ou mais.

### Pacotes adicionados no Buildroot (em relação ao TP9)

| Pacote | Para quê |
| --- | --- |
| `xorg7`, `xserver_xorg-server`, `xinit`, `xrandr`, `xf86-input-evdev/libinput`, `xf86-video-fbdev` | servidor gráfico onde rodam o menu e os jogos |
| `mesa3d` (V3D, VC4, GBM, GLX, EGL, GLES) + `libglvnd` | OpenGL acelerado pela GPU da Raspberry Pi 4 |
| `libxshmfence`, `libepoxy` | DRI3 e glamor: aceleração dentro do X |
| `sdl2` (X11, KMS/DRM, OpenGL, GLES), `sdl2_ttf` | jogos e menu |
| `alsa-utils`, `mpg123`, `pulseaudio` | áudio |
| `eudev` | hotplug de teclado, mouse e controle no X |
| `dejavu` | fonte do menu |
| `brcmfmac_sdio-firmware-rpi` (Wi‑Fi) | firmware que faltou no TP9 |
| `ccs-menu` (pacote do grupo) | o menu gráfico, em `br2-external-tp9/package/ccs-menu` |

No `config.txt` entra `dtoverlay=vc4-kms-v3d`, que é o driver KMS da GPU, além de `dtparam=audio=on`.

## Testes rápidos na Raspberry

```bash
cat /run/ccs/status                 # estado do menu
docker ps                           # ccs-console Up, 0.0.0.0:80->5000/tcp
curl http://127.0.0.1/api/status    # mesmo estado, pelo painel
curl -X POST http://127.0.0.1/api/jogar/doom   # abre o DOOM na TV
curl -X POST http://127.0.0.1/api/parar        # fecha o jogo
ip addr show wlan0                  # 192.168.50.1 (rede TP9-GRUPO01)
cat /var/log/ccs-menu.log           # erros do X e do menu
```
