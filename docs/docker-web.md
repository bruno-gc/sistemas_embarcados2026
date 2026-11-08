# Aplicação Web e Docker

A aplicação do TP9 roda num container Docker com Python + Flask. Ela lê a distância do dispositivo `/dev/hcsr04`, criado pelo driver, e mostra o valor num dashboard que atualiza sozinho.

O driver fica no kernel da Raspberry. O container recebe **apenas** o `/dev/hcsr04` e nunca acessa GPIO ou registradores diretamente.

## Arquivos (`flask-app/`)

| Arquivo | Função |
| --- | --- |
| `app.py` | backend Flask: rotas `/` e `/api/distance` |
| `requirements.txt` | dependências Python (`flask>=3.0,<4`) |
| `templates/index.html` | página do dashboard |
| `static/dashboard.js` | consulta a API a cada ~500 ms e atualiza a tela |
| `static/dashboard.css` | visual do dashboard |
| `Dockerfile` | receita da imagem `tp9-distance:1.0` |

## API do backend

| Rota | Método | Resposta |
| --- | --- | --- |
| `/` | GET | o dashboard (HTML) |
| `/api/distance` | GET | JSON com a distância medida |

Resposta de sucesso (HTTP 200):

```json
{
  "distance_mm": 374,
  "distance_cm": 37.4,
  "timestamp": 1793833788.61
}
```

Se a leitura falhar, o servidor **não cai**. Ele responde com erro e os campos de distância em `null`:

| HTTP | `code` | Quando acontece |
| --- | --- | --- |
| 503 | `timeout` | o driver devolveu `ETIMEDOUT`: o sensor não respondeu (desligado ou mal ligado) |
| 503 | `device_missing` | o `/dev/hcsr04` não existe (driver não carregado) |
| 503 | `read_error` | outra falha de leitura no driver |
| 502 | `invalid_value` | o driver devolveu algo que não é um número |

Exemplo de erro:

```json
{
  "distance_mm": null,
  "distance_cm": null,
  "error": "sensor sem resposta (sem eco)",
  "code": "timeout",
  "timestamp": 1793833790.12
}
```

## Como o backend lê o sensor

A cada pedido em `/api/distance`, o Flask abre o `/dev/hcsr04`, lê o conteúdo e converte para inteiro:

```python
with open("/dev/hcsr04", "r") as dev:
    mm = int(dev.read().strip())
```

Cada abertura do arquivo faz o driver disparar **uma medição** e devolver a distância em milímetros, em texto. A conversão para centímetros é feita no backend: `distance_cm = round(mm / 10, 1)`.

Se dois clientes consultarem ao mesmo tempo, o driver serializa as medições com um mutex, e cada pedido recebe a sua leitura.

## Fluxo de atualização do frontend

1. O navegador abre `http://192.168.50.1` e recebe o `index.html`.
2. O `dashboard.js` chama `fetch("/api/distance")`.
3. Com a resposta, ele atualiza só o número (em cm, com uma casa decimal), o valor em mm e a barra de proximidade, **sem recarregar a página**.
4. Se a resposta for um erro, mostra `--`, a mensagem do backend e uma bolinha vermelha.
5. Depois que a resposta chega, a próxima consulta é agendada para daqui a **500 ms** (`setTimeout`).

Como a próxima consulta só começa quando a anterior termina, os pedidos não se acumulam se o sensor demorar. A página não carrega nada de fora (fontes, bibliotecas ou CDN), porque a rede da Raspberry não tem Internet, e funciona no navegador do celular e do notebook.

## Imagem Docker

```dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
EXPOSE 5000
CMD ["python", "app.py"]
```

- **`python:3.12-slim`**: imagem oficial e enxuta, publicada para arm64.
- **`requirements.txt` antes do código**: o Docker reaproveita a camada do `pip install` quando só o código muda.
- **`EXPOSE 5000`**: só documenta a porta. Quem publica é o `-p` do `docker run`.
- **`app.run(host="0.0.0.0", port=5000)`**: o Flask aceita conexões vindas de fora do container. Com `127.0.0.1`, só o próprio container acessaria.

A Raspberry Pi 4 é ARM de 64 bits, e a máquina de build é x86_64. Por isso a imagem é gerada com `docker build --platform linux/arm64`, usando o emulador QEMU registrado pelo `tonistiigi/binfmt`. Uma imagem x86 daria `exec format error` na Raspberry.

### Por que a imagem já vai no cartão

A validação não pode depender de Internet. O `scripts/build.sh` exporta a imagem com `docker save` para `rootfs-overlay/opt/docker/tp9-distance.tar`, e o Buildroot grava esse arquivo no sistema. No boot, o container é criado a partir desse arquivo, sem baixar nada.

## Execução do container

O script `S99tp9-distance` sobe o container no boot. Ele espera o `/dev/hcsr04` existir e o `docker info` responder, faz `docker load` só se a imagem ainda não estiver carregada e executa:

```bash
docker run -d \
  --name tp9-distance \
  --device=/dev/hcsr04:/dev/hcsr04 \
  -p 80:5000 \
  --restart unless-stopped \
  tp9-distance:1.0
```

| Opção | Significado |
| --- | --- |
| `-d` | roda em segundo plano |
| `--name tp9-distance` | nome fixo, usado em `docker ps` e `docker inspect` |
| `--device=/dev/hcsr04:/dev/hcsr04` | dá ao container acesso **somente** ao dispositivo do sensor |
| `-p 80:5000` | publica a porta 80 da Raspberry na porta 5000 do container |
| `--restart unless-stopped` | o Docker reinicia o container se ele cair, a menos que alguém o pare |

### `--device` em vez de `--privileged`

O `--privileged` dá ao container acesso a todos os dispositivos da Raspberry e desliga várias proteções do Docker. O projeto **não usa `--privileged`**: o `--device` libera só o `/dev/hcsr04`, o mínimo de que a aplicação precisa. No cgroup v2, o Docker aplica essa regra com um filtro de dispositivos em eBPF, por isso o kernel precisa de `CONFIG_CGROUP_BPF`, que a configuração oficial da Raspberry já traz.

## Rede Docker

- O container fica na rede **bridge** padrão do Docker, ligada à interface virtual `docker0`, e recebe um IP interno (faixa `172.17.0.x`).
- Esse IP só existe dentro da Raspberry. O celular, conectado na rede `TP9-GRUPO01`, só enxerga a Raspberry em `192.168.50.1`.
- O pedido do celular chega em `192.168.50.1:80`, e o Docker, por regras de NAT, repassa para a porta 5000 do container, onde o Flask escuta.
- A porta 80 é a padrão do HTTP, então o endereço é só `http://192.168.50.1`, sem número de porta.

## Teste local sem o sensor

A variável de ambiente `HCSR04_DEVICE` permite testar a API num PC, apontando para um arquivo de texto no lugar do `/dev/hcsr04`:

```powershell
pip install -r requirements.txt
$env:HCSR04_DEVICE="teste.txt"; python app.py
```

Ela existe **só para desenvolvimento**. O `Dockerfile` não define essa variável, e na Raspberry a aplicação sempre lê o `/dev/hcsr04` real.

## Verificação na Raspberry

| Comando | Esperado |
| --- | --- |
| `docker ps` | `tp9-distance` com STATUS Up e `0.0.0.0:80->5000/tcp` |
| `docker inspect tp9-distance \| grep -A4 Devices` | o `/dev/hcsr04` como único dispositivo |
| `docker inspect tp9-distance \| grep Privileged` | `"Privileged": false` |
| `docker logs tp9-distance` | as requisições do Flask |
| `curl http://127.0.0.1/api/distance` | o JSON com a distância |
