# Arquitetura do Sistema

## Componentes
O sistema é composto pelos seguintes elementos principais:
- **Raspberry Pi 4:** Placa de hardware (arquitetura ARM 64 bits) onde todo o projeto é executado.
- **Linux (Buildroot):** Sistema operativo Linux enxuto e compilado sob medida para a placa através do Buildroot, contendo o kernel, sistema de ficheiros e pacotes necessários.
- **Docker Engine:** Motor de contentorização instalado na imagem Linux para gerir e isolar a execução da aplicação.
- **Container `flask-tp8`:** Contentor criado a partir da imagem `python:3.12-slim`, responsável por rodar o ambiente da aplicação e publicar a porta 5000 (`-p 5000:5000`).
- **Aplicação Flask:** Servidor web em Python que processa os pedidos HTTP na porta 5000 (`host="0.0.0.0"`) e entrega as páginas ao navegador.

## Fluxo de Boot
O sistema foi configurado para funcionar de forma autónoma e sem necessidade de ligação à Internet:
1. Ao ligar a **Raspberry Pi 4**, o **Linux gerado pelo Buildroot** é iniciado a partir do cartão microSD.
2. Durante a inicialização, o serviço do **Docker Engine** é ativado.
3. Um script de arranque localizado em `/etc/init.d/` (incluído via *rootfs overlay*) carrega a imagem Docker que já está embarcada no sistema e inicia o contentor **`flask-tp8`**.
4. Dentro do contentor, o **Flask** é executado automaticamente e passa a escutar na **porta 5000**, deixando o sistema pronto para receber acessos pela rede local.

## Rotas da Aplicação
- **`/`:** Rota principal de teste que retorna a mensagem de texto exigida no enunciado (`Helooowww world eng. Computacional!!!!!!!`).
- **`/console`:** Rota que renderiza o template `console.html` juntamente com as folhas de estilo locais em `/static/style.css`, exibindo o menu interativo de jogos do CCS (Console Contra Sony).

## Diagrama da Arquitetura
![Arquitetura do sistema](imagens/arquitetura.png)