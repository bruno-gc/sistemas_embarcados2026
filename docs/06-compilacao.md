# 06 — Procedimento de compilação

Este documento permite que outra pessoa reconstrua a imagem a partir do repositório.

## 1. Máquina de build

| Item | Utilizado no projeto |
| --- | --- |
| Virtualização | VirtualBox no Windows |
| Sistema | Ubuntu Server 26.04.1 LTS (x86_64) |
| Recursos | 8 GB de RAM, 6 CPUs, 80 GB de disco |
| Acesso | SSH pela porta 2222 (NAT do VirtualBox: host 2222 → convidado 22) |

**Observação sobre o Windows:** o WSL2 ativa o hipervisor do Windows, o que impediu o VirtualBox de iniciar VMs (`WHvSetupPartition failed: E_ACCESSDENIED`). A solução foi desativá-lo com `bcdedit /set hypervisorlaunchtype off` e reiniciar.

## 2. Dependências

```bash
sudo apt update
sudo apt install -y build-essential git wget cpio unzip rsync bc file python3 libncurses-dev patch bzip2
```

No Ubuntu 26.04, o comando `install` padrão vem do uutils e é recusado pelo Buildroot (`You have an uutils 'install' version installed`). Use o do GNU coreutils:

```bash
# se /usr/bin/gnuinstall existir:
sudo update-alternatives --install /usr/bin/install install /usr/bin/gnuinstall 100
# caso contrário:
sudo apt install -y coreutils-from-gnu --allow-remove-essential

install --version | head -1    # deve mostrar "GNU coreutils"
```

## 3. Docker e emulação ARM

```bash
sudo apt install -y docker.io
sudo usermod -aG docker $USER
newgrp docker
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

Saída esperada do último comando: `installing: arm64 OK`.
O registro do emulador não sobrevive à reinicialização da máquina de build: depois de reiniciar, rode o comando de novo, senão o container arm64 encerra logo ao iniciar na VM.

## 4. Repositório

```bash
git clone https://github.com/bruno-gc/sistemas_embarcados2026.git ~/tp8-console
cd ~/tp8-console
```

## 5. Build completo

```bash
./scripts/build.sh
```

O script executa quatro etapas:

| Etapa | O que faz |
| --- | --- |
| 1/4 | `docker build --platform linux/arm64` da aplicação e `docker save` para `overlay/opt/docker/flask-tp8.tar` |
| 2/4 | baixa e extrai o Buildroot 2026.02.3 em `~/buildroot-2026.02.3`, se ainda não existir |
| 3/4 | `make BR2_EXTERNAL=<repo>/br2-external tp8_raspberrypi4_defconfig` |
| 4/4 | `make` |

Para usar outra pasta do Buildroot: `BR_DIR=/caminho/buildroot ./scripts/build.sh`.

Para rodar em segundo plano e poder fechar o terminal:

```bash
nohup ./scripts/build.sh > ~/build.log 2>&1 &
tail -f ~/build.log      # Ctrl+C sai do acompanhamento sem parar o build
```

## 6. Tempo de build

- **Primeiro build:** algumas horas. Inclui toolchain, kernel, Go e Docker Engine.
- **Builds seguintes:** poucos minutos, quando só mudam arquivos do overlay ou da aplicação.

## 7. Resultado

Os arquivos ficam em `~/buildroot-2026.02.3/output/images/`:

| Arquivo | Conteúdo |
| --- | --- |
| `sdcard.img` | imagem completa para gravar no microSD |
| `boot.vfat` | partição de boot (firmware, kernel, device trees) |
| `rootfs.ext4` | sistema de arquivos raiz (2 GB) |
| `Image` | kernel Linux |
| `bcm2711-rpi-4-b.dtb` | device tree da Raspberry Pi 4 |

## 8. Alterando a configuração

```bash
cd ~/buildroot-2026.02.3
make menuconfig
make savedefconfig BR2_DEFCONFIG=~/tp8-console/br2-external/configs/tp8_raspberrypi4_defconfig
cp ~/tp8-console/br2-external/configs/tp8_raspberrypi4_defconfig ~/tp8-console/buildroot-config/raspberrypi4_tp8_defconfig
```

Depois, commite a defconfig e rode o `build.sh` de novo.

- **Pacote adicionado:** o `make` compila só o que falta.
- **Opção de um pacote já compilado alterada:** rode `make <pacote>-dirclean` antes, ou `make clean` em último caso.

## 9. Gravação do microSD

```bash
lsblk
sudo ./scripts/flash-sd.sh /dev/sdX
```

No Windows, copie o `sdcard.img` da VM com:

```powershell
scp -P 2222 ninovm@127.0.0.1:/home/ninovm/buildroot-2026.02.3/output/images/sdcard.img .
```

Depois, grave com o Raspberry Pi Imager (*Usar personalizado*), sem aplicar personalização.
