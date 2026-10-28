# 04 — Docker: imagem, container e inicialização automática

Este documento descreve como a aplicação Flask é empacotada em uma imagem Docker, como essa imagem chega à Raspberry Pi e como o container é iniciado no boot.

## Imagem e container

- **Imagem:** um pacote somente leitura com tudo o que a aplicação precisa: Python, Flask e o código. No projeto, ela se chama `flask-tp8:1.0`.
- **Container:** uma instância em execução da imagem, isolada do resto do sistema. No projeto, ele se chama `flask-tp8`.

Uma imagem pode gerar vários containers. Apagar o container não apaga a imagem.

## Arquivos da aplicação (`flask-app/`)

| Arquivo | Função |
| --- | --- |
| `app.py` | aplicação Flask com as rotas `/` (frase do enunciado) e `/console` (menu) |
| `requirements.txt` | dependências Python (`flask>=3.0,<4`) |
| `Dockerfile` | receita para construir a imagem |
| `templates/console.html` | página do menu do console |
| `static/style.css` | estilo do menu |

## Dockerfile, linha a linha

```dockerfile
FROM python:3.12-slim
```
Parte da imagem oficial do Python 3.12, na versão enxuta. Ela é publicada para várias arquiteturas, incluindo arm64, a da Raspberry Pi 4.

```dockerfile
WORKDIR /app
```
Define `/app` como pasta de trabalho dentro da imagem.

```dockerfile
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
```
Copia primeiro só a lista de dependências e instala o Flask. Separar esse passo faz o Docker reaproveitar essa camada quando só o código muda.

```dockerfile
COPY . .
```
Copia o restante da aplicação (código, templates e arquivos estáticos).

```dockerfile
EXPOSE 5000
```
Documenta que a aplicação usa a porta 5000. **Não publica a porta:** quem publica é o `-p 5000:5000` no `docker run`.

```dockerfile
CMD ["python", "app.py"]
```
Comando executado quando o container inicia. O `app.py` chama `app.run(host="0.0.0.0", port=5000)`. O `0.0.0.0` faz o Flask aceitar conexões vindas de fora do container; com `127.0.0.1`, só o próprio container conseguiria acessar.

## Geração da imagem para arm64

A máquina de build é x86_64 e a Raspberry Pi 4 é arm64. Uma imagem gerada normalmente na VM não rodaria na Pi (erro `exec format error`). Por isso:

1. Registramos na VM o emulador ARM (QEMU) com `docker run --privileged --rm tonistiigi/binfmt --install arm64`.
2. Geramos a imagem informando a plataforma de destino:

```bash
docker build --platform linux/arm64 -t flask-tp8:1.0 flask-app/
docker image inspect flask-tp8:1.0 --format '{{.Architecture}}'   # arm64
```

## Por que a imagem já vai dentro do sistema

O enunciado exige que o funcionamento não dependa de download da Internet durante a apresentação. Por isso a imagem não é baixada na Raspberry: ela é exportada para um arquivo e gravada no cartão junto com o sistema.

```bash
docker save flask-tp8:1.0 -o br2-external/board/rpi4/overlay/opt/docker/flask-tp8.tar
```

O `docker save` grava a imagem completa (todas as camadas) num arquivo `.tar`. Pelo rootfs overlay, esse arquivo vai para `/opt/docker/flask-tp8.tar` na Raspberry. No boot, o `docker load` faz o caminho inverso e registra a imagem no Docker da Pi.

O `.tar` não é versionado no Git, porque é um arquivo grande e reconstruível. O `scripts/build.sh` gera ele a cada build.

## Inicialização automática

O script `/etc/init.d/S99docker-flask` (detalhado em `docs/03-linux.md`) espera o Docker Engine, carrega a imagem se necessário e executa:

```bash
docker run -d --name flask-tp8 -p 5000:5000 --restart unless-stopped flask-tp8:1.0
```

| Opção | Significado |
| --- | --- |
| `-d` | roda em segundo plano |
| `--name flask-tp8` | nome fixo do container, usado em `docker ps` e `docker inspect flask-tp8` |
| `-p 5000:5000` | publica a porta 5000 da Raspberry (host, à esquerda) na porta 5000 do container (à direita) |
| `--restart unless-stopped` | o Docker reinicia o container se ele cair, a não ser que alguém o pare manualmente |
| `flask-tp8:1.0` | imagem usada |

## Comandos de verificação

```bash
docker images                  # flask-tp8 com a tag 1.0
docker ps                      # flask-tp8 Up, 0.0.0.0:5000->5000/tcp
docker logs flask-tp8          # saída do Flask
docker inspect flask-tp8       # configuração completa e IP interno
curl http://127.0.0.1:5000     # resposta da aplicação
```

## Teste antes de gravar o cartão

Como o emulador ARM está registrado, dá para testar a imagem arm64 na própria VM:

```bash
docker run --rm -d --name teste -p 5000:5000 flask-tp8:1.0
sleep 8
./scripts/test.sh 127.0.0.1
docker rm -f teste
```
