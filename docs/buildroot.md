# Buildroot — decisões de configuração do TP9

- **Versão:** Buildroot 2026.02.3
- **Defconfig do projeto:** `br2-external-tp9/configs/tp9_raspberrypi4_defconfig`
- **Árvore externa:** `br2-external-tp9/`, com o nome `TP9` em `external.desc`. Dentro da configuração, o caminho dela é `$(BR2_EXTERNAL_TP9_PATH)`.

Cada decisão segue o padrão: configuração, símbolo, motivo, dependências e impacto. Os símbolos podem ser conferidos direto na defconfig.

Para reproduzir a configuração:

```bash
make -C ~/buildroot-2026.02.3 BR2_EXTERNAL=$PWD/br2-external-tp9 tp9_raspberrypi4_defconfig
```

---

## 1. Configuração base da Raspberry Pi 4

```
Configuração:  make raspberrypi4_64_defconfig
Símbolo:       BR2_aarch64=y (e demais símbolos da defconfig oficial)
Motivo:        partir da configuração oficial da placa; a Pi 4 tem Cortex-A72
               de 64 bits, e as imagens Docker oficiais existem para arm64.
Dependências:  kernel bcm2711, firmware e device trees da Raspberry Pi.
Impacto:       define a arquitetura do sistema; a imagem Docker precisa ser
               gerada para linux/arm64.
```

## 2. Identidade do sistema (herdada do TP8)

```
Configuração:  System configuration → System hostname / Root password /
               Path to the users tables
Símbolos:      BR2_TARGET_GENERIC_HOSTNAME="ccs-console"
               BR2_TARGET_GENERIC_ROOT_PASSWD="CCS1234"
               BR2_ROOTFS_USERS_TABLES="$(BR2_EXTERNAL_TP9_PATH)/board/rpi4/users.txt"
Motivo:        usuário nino e hostname criados no build, sem passo manual.
Dependências:  arquivo users.txt no formato da tabela de usuários do Buildroot.
Impacto:       login disponível no primeiro boot; senhas em texto puro no repositório.
```

## 3. Rootfs overlay

```
Configuração:  System configuration → Root filesystem overlay directories
Símbolo:       BR2_ROOTFS_OVERLAY="$(BR2_EXTERNAL_TP9_PATH)/board/rpi4/rootfs-overlay"
Motivo:        colocar no sistema os arquivos do projeto sem alterar o Buildroot:
               /etc/issue, os scripts S30hcsr04, S40wifi-ap e S99tp9-distance e
               a imagem Docker /opt/docker/tp9-distance.tar.
Dependências:  o tp9-distance.tar é gerado pelo scripts/build.sh antes do make.
Impacto:       mudanças nesses arquivos exigem só um build incremental.
```

## 4. Docker Engine

```
Configuração:  Target packages → System tools → docker-engine, docker-cli
Símbolos:      BR2_PACKAGE_DOCKER_ENGINE=y
               BR2_PACKAGE_DOCKER_CLI=y
               BR2_PACKAGE_CGROUPFS_V2_MOUNT=y (selecionado pelo docker-engine)
Motivo:        a aplicação roda em container; o cliente docker é usado pelo
               S99tp9-distance e na validação.
Dependências:  containerd e runc (selecionados automaticamente); cgroup v2.
               O cgroupfs-mount (v1) é incompatível e não deve ser marcado.
Impacto:       é o pacote mais pesado do projeto (toolchain Go); instala o
               S60dockerd, que sobe entre o Wi-Fi (S40) e o container (S99).
```

## 5. Wi-Fi em modo Access Point

```
Configuração:  Target packages → Networking applications → hostapd, dnsmasq, iw
               Target packages → Hardware handling → Firmware →
               brcmfmac-sdio-firmware-rpi → wifi
Símbolos:      BR2_PACKAGE_HOSTAPD=y
               BR2_PACKAGE_DNSMASQ=y
               BR2_PACKAGE_IW=y
               BR2_PACKAGE_BRCMFMAC_SDIO_FIRMWARE_RPI=y
               BR2_PACKAGE_BRCMFMAC_SDIO_FIRMWARE_RPI_WIFI=y
Motivo:        hostapd cria o Access Point TP9-GRUPO01; dnsmasq entrega DHCP e
               DNS; iw serve para diagnóstico; o firmware é exigido pelo chip
               Wi-Fi onboard da Raspberry Pi 4 (Broadcom).
Dependências:  driver brcmfmac e cfg80211 no kernel (já presentes na
               configuração oficial, como módulos).
Impacto:       a demonstração não depende de roteador externo.
```

## 6. Driver do HC-SR04

```
Configuração:  External options → TP9 → hcsr04-driver
Símbolo:       BR2_PACKAGE_HCSR04_DRIVER=y
Motivo:        o enunciado exige que o driver faça parte do processo de build,
               como pacote próprio do BR2_EXTERNAL.
Dependências:  BR2_LINUX_KERNEL; host-dtc (compila o overlay) e rpi-firmware
               (cria a pasta de overlays da partição de boot).
Impacto:       gera o hcsr04.ko, compilado contra o mesmo kernel da imagem e
               instalado no rootfs, e o hcsr04.dtbo, instalado em
               rpi-firmware/overlays/.
```

O pacote (`package/hcsr04-driver/`):

- **`Config.in`** declara o símbolo `BR2_PACKAGE_HCSR04_DRIVER`, que depende de `BR2_LINUX_KERNEL`.
- **`hcsr04-driver.mk`** define a versão e o código local (`SITE_METHOD = local`) e usa as infraestruturas `kernel-module` e `generic-package`. Um *post-build hook* passa o `.dts` pelo pré-processador C, com os headers do kernel da imagem, e compila com o `dtc`. O passo de *install images* copia o `.dtbo` para a partição de boot.
- **`src/Makefile`** contém `obj-m := hcsr04.o`, que é o que o Kbuild do kernel lê quando a infraestrutura chama `make -C $(LINUX_DIR) M=…`.

## 7. `config.txt` próprio

```
Configuração:  Target packages → Hardware handling → Firmware → rpi-firmware →
               Path to a file stored as config.txt
Símbolo:       BR2_PACKAGE_RPI_FIRMWARE_CONFIG_FILE="$(BR2_EXTERNAL_TP9_PATH)/board/rpi4/config.txt"
Motivo:        o firmware precisa carregar o overlay do sensor antes do boot do
               kernel. O arquivo é o config_4_64bit.txt oficial com a linha
               dtoverlay=hcsr04 acrescentada no final.
Dependências:  hcsr04.dtbo presente em rpi-firmware/overlays/.
Impacto:       ao mudar esta opção num build já existente, é preciso rodar
               make rpi-firmware-reinstall, porque o pacote já compilado não é
               reinstalado sozinho.
```

## 8. Script de post-build

```
Configuração:  System configuration → Custom scripts to run before creating
               filesystem images
Símbolo:       BR2_ROOTFS_POST_BUILD_SCRIPT="board/raspberrypi4-64/post-build.sh
               $(BR2_EXTERNAL_TP9_PATH)/board/rpi4/post-build.sh"
Motivo:        instalar hostapd.conf e dnsmasq.conf em /etc (os arquivos ficam
               em board/rpi4/, como pede a estrutura do enunciado) e remover os
               scripts de init dos pacotes dnsmasq e hostapd.
Dependências:  o script oficial da Raspberry Pi continua sendo executado antes
               do nosso.
Impacto:       o S40wifi-ap passa a ser o único responsável pela ordem do Wi-Fi.
```

## 9. curl

```
Configuração:  Target packages → Libraries → Networking → libcurl → curl binary
Símbolos:      BR2_PACKAGE_LIBCURL=y
               BR2_PACKAGE_LIBCURL_CURL=y
Motivo:        testes locais da API na própria Raspberry.
Dependências:  nenhuma relevante.
Impacto:       aumento pequeno do rootfs.
```

## 10. Tamanho do sistema de arquivos

```
Configuração:  Filesystem images → ext2/3/4 root filesystem → exact size
Símbolo:       BR2_TARGET_ROOTFS_EXT2_SIZE="2G"
Motivo:        comportar o Docker Engine, o tp9-distance.tar e a imagem
               descompactada em /var/lib/docker depois do docker load.
Dependências:  microSD de pelo menos 4 GB.
Impacto:       gravação do cartão mais demorada.
```

## 11. Kernel

```
Configuração:  Kernel → configuração oficial bcm2711, sem fragmento próprio
Motivo:        a configuração oficial já inclui o que o projeto usa:
               CONFIG_BRCMFMAC e CONFIG_CFG80211 (Wi-Fi),
               CONFIG_CGROUP_BPF e CONFIG_BPF_SYSCALL (o --device do Docker no
               cgroup v2), além de GPIO, IRQ e overlays de Device Tree.
Validação:     grep -E "^CONFIG_(BRCMFMAC|CFG80211|CGROUP_BPF|BPF_SYSCALL)=" \
                 output/build/linux-*/.config
```
