# Wi-Fi em modo Access Point

A Raspberry Pi 4 cria a própria rede Wi-Fi durante o boot. Um celular ou notebook conecta direto nela e abre o dashboard, sem depender de roteador externo.

## Parâmetros

| Parâmetro | Valor |
| --- | --- |
| SSID | `TP9-GRUPO01` |
| Segurança | WPA2-PSK (AES/CCMP) |
| Senha | `embarcados2026` |
| Interface | `wlan0` (Wi-Fi onboard) |
| IP da Raspberry | `192.168.50.1/24` |
| Faixa DHCP | `192.168.50.10` a `192.168.50.100`, lease de 12 h |
| Gateway e DNS entregues | `192.168.50.1` |
| URL da aplicação | `http://192.168.50.1` |

## Componentes

| Componente | Pacote do Buildroot | Função |
| --- | --- | --- |
| hostapd | `BR2_PACKAGE_HOSTAPD` | coloca a `wlan0` em modo Access Point e faz a autenticação WPA2 |
| dnsmasq | `BR2_PACKAGE_DNSMASQ` | servidor DHCP (e DNS leve) da rede |
| iw | `BR2_PACKAGE_IW` | diagnóstico da interface Wi-Fi |
| Firmware Broadcom | `BR2_PACKAGE_BRCMFMAC_SDIO_FIRMWARE_RPI_WIFI` | firmware do chip Wi-Fi onboard da Raspberry Pi 4 |
| Driver brcmfmac | kernel (módulo da configuração oficial) | driver Linux do chip Wi-Fi |

## `hostapd.conf`

Arquivo `br2-external-tp9/board/rpi4/hostapd.conf`, instalado em `/etc/hostapd.conf` pelo `post-build.sh`.

```
interface=wlan0
driver=nl80211
ssid=TP9-GRUPO01
country_code=BR
hw_mode=g
channel=6
wpa=2
wpa_passphrase=embarcados2026
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
```

| Linha | Significado |
| --- | --- |
| `interface=wlan0` | interface Wi-Fi onboard que vira o Access Point |
| `driver=nl80211` | interface padrão do Linux para configurar Wi-Fi, suportada pelo driver `brcmfmac` |
| `ssid=TP9-GRUPO01` | nome da rede, no padrão `TP9-GRUPOXX` |
| `country_code=BR` | região regulatória do Brasil: define canais e potências permitidos |
| `hw_mode=g` | 802.11g, na faixa de 2,4 GHz, compatível com praticamente todo celular e notebook |
| `channel=6` | canal de 2,4 GHz usado pela rede |
| `wpa=2` | usa WPA2 (RSN) |
| `wpa_passphrase=embarcados2026` | senha da rede |
| `wpa_key_mgmt=WPA-PSK` | autenticação por senha compartilhada (Pre-Shared Key) |
| `rsn_pairwise=CCMP` | criptografia AES (CCMP) para o tráfego |

## `dnsmasq.conf`

Arquivo `br2-external-tp9/board/rpi4/dnsmasq.conf`, instalado em `/etc/dnsmasq.conf` pelo `post-build.sh`.

```
interface=wlan0
bind-interfaces
listen-address=192.168.50.1
no-resolv
dhcp-range=192.168.50.10,192.168.50.100,255.255.255.0,12h
dhcp-option=3,192.168.50.1
dhcp-option=6,192.168.50.1
dhcp-leasefile=/var/run/dnsmasq.leases
```

| Linha | Significado |
| --- | --- |
| `interface=wlan0` | atende somente a rede Wi-Fi |
| `bind-interfaces` | escuta só nas interfaces indicadas, sem ocupar as portas nas outras (como a `docker0`) |
| `listen-address=192.168.50.1` | escuta no IP da Raspberry |
| `no-resolv` | não repassa consultas DNS para servidores externos: a rede não tem Internet |
| `dhcp-range=…,12h` | entrega IPs de 192.168.50.10 a 192.168.50.100, máscara /24, por 12 horas |
| `dhcp-option=3,192.168.50.1` | opção 3 do DHCP: o gateway dos clientes é a Raspberry |
| `dhcp-option=6,192.168.50.1` | opção 6 do DHCP: o DNS dos clientes é a Raspberry |
| `dhcp-leasefile=/var/run/dnsmasq.leases` | guarda os leases em `/var/run`, um sistema de arquivos em memória que sempre existe no boot |

## IP fixo da Raspberry

A Raspberry é o servidor da rede: é ela quem distribui os endereços, e o dashboard precisa estar sempre no mesmo endereço, `http://192.168.50.1`. Por isso a `wlan0` recebe o IP fixo `192.168.50.1/24` no boot, e não pede IP por DHCP.

## Inicialização (`S40wifi-ap`)

Script `br2-external-tp9/board/rpi4/rootfs-overlay/etc/init.d/S40wifi-ap`. Ele roda depois do driver do sensor (`S30hcsr04`) e antes do Docker (`S60dockerd`) e do container (`S99tp9-distance`).

1. Carrega o driver do Wi-Fi: `modprobe brcmfmac`.
2. Espera a interface `wlan0` existir (`/sys/class/net/wlan0`), por até 30 s. É uma espera por uma condição real, e não um tempo fixo.
3. Desbloqueia o rádio, se o `rfkill` estiver disponível.
4. Ativa a interface e aplica o IP fixo:
   ```
   ip link set wlan0 up
   ip addr flush dev wlan0
   ip addr add 192.168.50.1/24 dev wlan0
   ```
5. Sobe o Access Point em segundo plano: `hostapd -B -P /var/run/hostapd.pid /etc/hostapd.conf`. O log vai para `/var/log/hostapd.log`.
6. Por último, sobe o DHCP: `dnsmasq -C /etc/dnsmasq.conf -x /var/run/dnsmasq.pid`. O `dnsmasq` vem depois do IP fixo porque precisa escutar em `192.168.50.1`.

Os pacotes `dnsmasq` e `hostapd` instalam scripts de init próprios (`S80dnsmasq`). O `post-build.sh` remove esses scripts, para os serviços não subirem duas vezes nem antes de a `wlan0` ter IP. Assim, o `S40wifi-ap` é o único responsável pela ordem.

## Diagnóstico

| Comando | Para quê |
| --- | --- |
| `ip addr show wlan0` | confirmar `inet 192.168.50.1/24` |
| `iw dev wlan0 info` | confirmar `type AP` e o SSID |
| `cat /var/log/hostapd.log` | erros do Access Point |
| `ps \| grep -E "hostapd\|dnsmasq"` | confirmar que os dois processos estão rodando |
| `cat /var/run/dnsmasq.leases` | ver os clientes que receberam IP |
| `dmesg \| grep -i brcmfmac` | erros do driver ou do firmware do Wi-Fi |

## Observação sobre celulares

Como a rede `TP9-GRUPO01` não tem Internet, alguns celulares Android voltam para os dados móveis. Nesse caso, basta escolher "Manter conexão" no aviso do Wi-Fi ou desligar os dados móveis para abrir `http://192.168.50.1`.
