# 07 — Testes e evidências

Este documento descreve os testes feitos para validar o sistema, com os resultados esperados e obtidos. As capturas ficam em `evidence/`.

## Ambiente de teste

| Item | Valor |
| --- | --- |
| Placa | Raspberry Pi 4 Model B |
| Imagem | `sdcard.img` gerado pelo `build.sh` (Buildroot 2026.02.3) |
| Rede | Raspberry e notebook conectados ao mesmo roteador |
| Data do teste | PREENCHER |

## 1. Teste da imagem Docker na VM (antes de gravar o cartão)

Com a emulação ARM, a imagem arm64 foi executada na própria máquina de build:

```bash
docker image inspect flask-tp8:1.0 --format '{{.Architecture}}'
docker run --rm -d --name teste -p 5000:5000 flask-tp8:1.0
sleep 8
./scripts/test.sh 127.0.0.1
docker rm -f teste
```

| Verificação | Esperado | Obtido |
| --- | --- | --- |
| Arquitetura da imagem | `arm64` | PREENCHER |
| Rota `/` | frase exata | PREENCHER |
| Rota `/console` | HTTP 200 | PREENCHER |

## 2. Testes na Raspberry Pi 4

O teste parte da placa desligada.

| # | Teste | Comando ou ação | Esperado | Obtido | Evidência |
| --- | --- | --- | --- | --- | --- |
| 1 | Boot e boas-vindas | ligar a placa | mensagem de `/etc/issue` antes do login | PREENCHER | `evidence/login/01-login.png` |
| 2 | Login | usuário `nino`, senha `CCS1234` | prompt do usuário | PREENCHER | `evidence/login/01-login.png` |
| 3 | Hostname | `hostname` | `ccs-console` | PREENCHER | — |
| 4 | IP da Raspberry | `ip addr show eth0` | IP da rede local em `inet` | PREENCHER | `evidence/network/02-ip-raspberry.png` |
| 5 | Docker Engine | `docker version` | cliente e servidor respondendo | PREENCHER | `evidence/docker/03-docker-version.png` |
| 6 | Container | `docker ps` | `flask-tp8` Up, `0.0.0.0:5000->5000/tcp` | PREENCHER | `evidence/docker/04-container-running.png` |
| 7 | Redes Docker | `docker network ls` | `bridge`, `host` e `none` | PREENCHER | `evidence/network/05-docker-network.png` |
| 8 | Inspeção | `docker inspect flask-tp8` | IP interno do container na rede bridge | PREENCHER | `evidence/docker/06-docker-inspect.png` |
| 9 | Acesso local | `curl http://127.0.0.1:5000` | `Helooowww world eng. Computacional!!!!!!!` | PREENCHER | `evidence/web/07-curl-local.png` |
| 10 | Acesso pela rede | navegador do notebook em `http://IP_DA_RASPBERRY:5000` | a mesma frase | PREENCHER | `evidence/web/08-acesso-notebook.png` |
| 11 | Menu do console | navegador em `http://IP_DA_RASPBERRY:5000/console` | menu exibido | PREENCHER | `evidence/web/09-console.png` |
| 12 | Teste automatizado | `./scripts/test.sh IP_DA_RASPBERRY` no notebook ou na VM | duas linhas `OK` | PREENCHER | — |

## 3. Teste de reinicialização

Confirma que nada depende de comando manual:

1. Executar `reboot` na Raspberry.
2. Depois do boot, sem nenhum comando, acessar `http://IP_DA_RASPBERRY:5000` pelo notebook.

| Esperado | Obtido |
| --- | --- |
| a página responde sem `docker load` ou `docker run` manual | PREENCHER |

## 4. Problemas encontrados nos testes

PREENCHER — registrar aqui qualquer falha observada no hardware e a correção aplicada (com o commit `fix:` correspondente). Se não houver falhas, escrever "Nenhum problema encontrado nos testes de hardware".
