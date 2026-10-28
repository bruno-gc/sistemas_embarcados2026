# 03 — Sistema Linux: usuário, hostname, boas-vindas e boot

Este documento descreve as customizações do Linux gerado pelo Buildroot e como o sistema inicializa.

## Usuário

O usuário é criado durante o build, a partir do arquivo `br2-external/board/rpi4/users.txt`:

```
nino -1 nino -1 =CCS1234 /home/nino /bin/sh - Usuario TP8
```

Cada campo da linha, na ordem:

| Campo | Valor | Significado |
| --- | --- | --- |
| usuário | `nino` | nome de login |
| uid | `-1` | o Buildroot escolhe o número automaticamente |
| grupo | `nino` | grupo principal, criado junto |
| gid | `-1` | número do grupo escolhido automaticamente |
| senha | `=CCS1234` | o `=` indica senha em texto puro, que o Buildroot criptografa no build |
| home | `/home/nino` | diretório pessoal, criado no build |
| shell | `/bin/sh` | shell de login (BusyBox) |
| grupos extras | `-` | nenhum |
| comentário | `Usuario TP8` | descrição do usuário |

O usuário `root` também está disponível, com a senha definida em `BR2_TARGET_GENERIC_ROOT_PASSWD` (ver `docs/02-buildroot.md`).

## Hostname

Definido no `menuconfig` como `ccs-console` (`BR2_TARGET_GENERIC_HOSTNAME`). No boot, o valor é aplicado a partir de `/etc/hostname`. Para conferir: `hostname`.

## Mensagem de boas-vindas

O arquivo `br2-external/board/rpi4/overlay/etc/issue` vai para o `/etc/issue` do sistema pelo rootfs overlay. O `getty` exibe esse arquivo no console antes do prompt de login. Assim a mensagem aparece sem nenhuma configuração depois do boot.

## Rootfs overlay

O overlay é uma pasta cuja estrutura é copiada por cima do sistema de arquivos no final do build. No projeto, ele leva:

| Arquivo no overlay | Destino no sistema | Função |
| --- | --- | --- |
| `etc/issue` | `/etc/issue` | mensagem de boas-vindas |
| `etc/init.d/S99docker-flask` | `/etc/init.d/S99docker-flask` | sobe o container no boot |
| `opt/docker/flask-tp8.tar` | `/opt/docker/flask-tp8.tar` | imagem Docker da aplicação (gerada pelo `build.sh`) |

## Sequência de boot

O sistema usa o init do BusyBox. Depois que o kernel monta o root filesystem, o `/etc/init.d/rcS` executa, em ordem alfabética, os scripts `/etc/init.d/S??*` com o argumento `start`. O número no nome define a ordem: `S01` roda antes de `S99`.

```
POWER ON
 → firmware da Raspberry Pi (partição boot)
 → kernel Linux
 → init do BusyBox → /etc/init.d/rcS
 → scripts S?? (montagens, rede, cgroup, Docker Engine)
 → S99docker-flask (o último a rodar)
 → getty exibe /etc/issue e o prompt de login
```

## Script S99docker-flask

Fica em `br2-external/board/rpi4/overlay/etc/init.d/S99docker-flask` e é executável (permissão `755` registrada no Git). Ele aceita `start`, `stop` e `restart`. No `start`, ele:

1. **Garante o cgroup v2 montado.** Se `/sys/fs/cgroup/cgroup.controllers` não existir, monta o cgroup2.
2. **Garante o Docker Engine rodando.** Se o `dockerd` não estiver em execução, inicia o processo.
3. **Espera o Docker responder.** Repete `docker info` por até 60 segundos.
4. **Carrega a imagem.** Roda `docker load -i /opt/docker/flask-tp8.tar`, mas só se a imagem `flask-tp8:1.0` ainda não existir.
5. **Recria o container.** Remove o `flask-tp8` antigo, se existir, e executa `docker run -d --name flask-tp8 -p 5000:5000 --restart unless-stopped flask-tp8:1.0`.

Ele fica com o número 99 porque precisa ser o último: depende da rede e do Docker Engine já iniciados.

Para testar manualmente depois do boot:

```bash
/etc/init.d/S99docker-flask restart
```

## Rede

A interface `eth0` recebe o IP por DHCP do roteador da rede local. Para descobrir o IP:

```bash
ip addr show eth0
```

A rede interna do Docker (`docker0` e a bridge) está explicada em `docs/05-rede.md`.
