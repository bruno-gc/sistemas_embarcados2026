# 02 — Configuração do Buildroot

Este documento registra as decisões de configuração do Buildroot. Cada decisão segue o padrão pedido no enunciado: configuração, símbolo, motivo, dependências e impacto.

- **Versão:** Buildroot 2026.02.3 (série LTS 2026.02)
- **Defconfig do projeto:** `br2-external/configs/tp8_raspberrypi4_defconfig` (cópia em `buildroot-config/raspberrypi4_tp8_defconfig`)
- **Árvore externa:** `br2-external/`, com o nome `TP8` definido em `external.desc`. Dentro da configuração, o caminho dela é referenciado como `$(BR2_EXTERNAL_TP8_PATH)`.

Os símbolos abaixo podem ser conferidos diretamente na defconfig.

---

## 1. Configuração base da Raspberry Pi 4

```
Configuração:
  make raspberrypi4_64_defconfig

Símbolo:
  BR2_aarch64=y (e demais símbolos da defconfig oficial)

Motivo:
  O enunciado exige partir da configuração oficial correspondente à
  Raspberry Pi 4. Escolhemos a versão de 64 bits porque a Pi 4 tem
  processador ARM Cortex-A72 de 64 bits e porque as imagens Docker
  oficiais (como python:3.12-slim) são publicadas para arm64.

Dependências:
  Kernel, firmware e device trees da Raspberry Pi já vêm configurados
  pela defconfig oficial (kernel bcm2711).

Impacto:
  Define a arquitetura de todo o sistema. Por isso a imagem Docker da
  aplicação também precisa ser gerada para linux/arm64.
```

## 2. Hostname

```
Configuração:
  System configuration
   → System hostname

Símbolo:
  BR2_TARGET_GENERIC_HOSTNAME="ccs-console"

Motivo:
  Identificar o equipamento na rede e no prompt. O nome do projeto,
  "Console Contra Sony (CCS)", não pode ser usado como hostname porque
  contém espaços e parênteses, que não são permitidos.

Dependências:
  Nenhuma.

Impacto:
  O valor é gravado em /etc/hostname e aparece no prompt após o login.
```

## 3. Senha do root

```
Configuração:
  System configuration
   → Root password

Símbolo:
  BR2_TARGET_GENERIC_ROOT_PASSWD="CCS1234"

Motivo:
  Permitir login administrativo sem configuração manual depois do boot.

Dependências:
  Nenhuma.

Impacto:
  A senha fica em texto puro na defconfig. É aceitável para o trabalho,
  mas não para um produto.
```

## 4. Usuário do sistema

```
Configuração:
  System configuration
   → Path to the users tables

Símbolo:
  BR2_ROOTFS_USERS_TABLES="$(BR2_EXTERNAL_TP8_PATH)/board/rpi4/users.txt"

Motivo:
  O enunciado exige que o usuário seja criado durante a construção da
  imagem, sem passos manuais depois do boot.

Dependências:
  O arquivo users.txt, no formato da tabela de usuários do Buildroot
  (ver docs/03-linux.md).

Impacto:
  O usuário nino, com home /home/nino, já existe no primeiro boot.
```

## 5. Rootfs overlay

```
Configuração:
  System configuration
   → Root filesystem overlay directories

Símbolo:
  BR2_ROOTFS_OVERLAY="$(BR2_EXTERNAL_TP8_PATH)/board/rpi4/overlay"

Motivo:
  Colocar no sistema os arquivos próprios do projeto sem modificar o
  Buildroot: a mensagem de boas-vindas (/etc/issue), o script de boot
  (/etc/init.d/S99docker-flask) e a imagem Docker da aplicação
  (/opt/docker/flask-tp8.tar).

Dependências:
  O flask-tp8.tar precisa ser gerado pelo scripts/build.sh antes do make.

Impacto:
  O overlay é copiado para o rootfs a cada make, então qualquer mudança
  nesses arquivos exige apenas um build incremental, de poucos minutos.
```

## 6. Docker Engine

```
Configuração:
  Target packages
   → System tools
   → docker-engine

Símbolo:
  BR2_PACKAGE_DOCKER_ENGINE=y

Motivo:
  A aplicação principal do sistema é executada dentro de um container
  Docker.

Dependências:
  O docker-engine seleciona automaticamente o containerd e o runc, que
  efetivamente criam e executam os containers. O kernel precisa ter
  suporte a cgroups, namespaces, overlayfs, veth, bridge e netfilter.

Impacto:
  É o pacote mais pesado do projeto: exige compilar o toolchain de Go,
  aumenta bastante o primeiro build e o tamanho do root filesystem, e
  adiciona alguns segundos ao boot.
```

## 7. Cliente Docker

```
Configuração:
  Target packages
   → System tools
   → docker-cli

Símbolo:
  BR2_PACKAGE_DOCKER_CLI=y

Motivo:
  Fornecer o comando docker, usado pelo script de boot (docker load,
  docker run) e na validação (docker ps, docker inspect).

Dependências:
  docker-engine.

Impacto:
  Pequeno aumento do rootfs.
```

## 8. Montagem do cgroup v2

```
Configuração:
  Target packages
   → System tools
   → cgroupfs-v2-mount (selecionado automaticamente pelo docker-engine)

Símbolo:
  BR2_PACKAGE_CGROUPFS_V2_MOUNT=y

Motivo:
  O Docker usa os cgroups do kernel para isolar e limitar os containers,
  e eles precisam estar montados em /sys/fs/cgroup.

Dependências:
  É incompatível com o pacote cgroupfs-mount (cgroup v1). Ao marcarmos
  o cgroupfs-mount, o docker-engine desapareceu do menu. Mantivemos
  apenas o v2.

Impacto:
  Nenhum passo manual. Como garantia adicional, o S99docker-flask monta
  o cgroup v2 caso ele não esteja montado.
```

## 9. curl

```
Configuração:
  Target packages
   → Libraries
   → Networking
   → libcurl
   → curl binary

Símbolo:
  BR2_PACKAGE_LIBCURL=y
  BR2_PACKAGE_LIBCURL_CURL=y

Motivo:
  A validação exige o teste local curl http://127.0.0.1:5000, e o
  Buildroot não inclui o curl por padrão.

Dependências:
  Nenhuma relevante.

Impacto:
  Aumento pequeno do rootfs.
```

## 10. Tamanho do sistema de arquivos

```
Configuração:
  Filesystem images
   → ext2/3/4 root filesystem
   → exact size

Símbolo:
  BR2_TARGET_ROOTFS_EXT2_SIZE="2G"

Motivo:
  O tamanho padrão não comporta o Docker Engine, o arquivo
  flask-tp8.tar e a imagem descompactada em /var/lib/docker depois do
  docker load.

Dependências:
  Cartão microSD de pelo menos 4 GB.

Impacto:
  O sdcard.img fica maior e a gravação do cartão demora mais.
```

## 11. Kernel

```
Configuração:
  Kernel
   → (mantida a configuração da defconfig oficial)

Motivo:
  A configuração do kernel da Raspberry Pi (bcm2711) já inclui os
  recursos usados por containers. Por isso não criamos um fragmento de
  configuração do kernel.

Validação:
  O funcionamento é comprovado no teste com docker ps e docker inspect
  (docs/07-testes.md). Se faltasse alguma opção, o dockerd não subiria
  ou o container não seria criado.
```
