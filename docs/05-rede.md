# Rede Docker e Publicação de Porta

## IP da Raspberry Pi

O IP da Raspberry é o endereço IP de uma interface de rede local na Raspberry. Ele não possui um IP fixo justamente pela programação DHCP do roteador.
Logo, para um computador encontrar o IP da Raspberry, ele necessita estar conectado na mesma rede local da Raspberry.

Comando utilizado para encontrar o endereço IP local:

```sh
ip addr
```

Geralmente, a interface de rede da Raspberry é padronizada como `eth0`.

## Interface `docker0`

Quando o Docker é inicializado na Raspberry, uma interface de rede **virtual** chamada `docker0` é criada lá dentro. Ela funciona como um switch virtual onde os containers se conectam; não existe um switch físico.
A `docker0` também tem um IP, que normalmente é `172.17.0.1`. Esse endereço serve de gateway para os containers, ou seja, é por ele que o tráfego dos containers sai para fora. A interface também aparece no `ip addr`.

## Bridge network

**Bridge** é a rede padrão do Docker e está ligada à `docker0`. Todo container criado sem indicar outra rede entra nela automaticamente.

Cada container conectado à bridge recebe um IP próprio, em uma faixa privada (normalmente `172.17.0.0/16`) que **só existe dentro da Raspberry**. O roteador da rede local não sabe que essa faixa existe. As redes do Docker podem ser listadas com:

```sh
docker network ls
```

onde aparece a rede `bridge` (além de `host` e `none`).

## IP interno do container

É o endereço que o container `flask-tp8` recebeu dentro da rede bridge (normalmente algo como `172.17.0.2`). Ele pode ser visto com:

```sh
docker inspect flask-tp8
```

no campo `IPAddress`, dentro de `NetworkSettings`.

Esse IP só é alcançável **de dentro da Raspberry**. O notebook não consegue acessá-lo, porque ele não faz parte da rede local que está conectada com a Raspberry.

## Porta do host e porta do container

Existem duas portas envolvidas:

- **Porta do host:** é a porta aberta na própria Raspberry Pi, que quem está na rede local consegue acessar.
- **Porta do container:** é a porta em que o Flask escuta lá dentro do container. No nosso `app.py`, o Flask roda com `app.run(host="0.0.0.0", port=5000)`.

O `host="0.0.0.0"` é importante: ele faz o Flask aceitar conexões vindas de qualquer interface do container, inclusive as que chegam pela bridge. Se fosse `127.0.0.1`, só o próprio container conseguiria acessar a aplicação, e o repasse de porta não funcionaria.

No nosso projeto, as duas portas são a 5000, mas elas poderiam ser diferentes.

## O que significa `-p 5000:5000`

O container é iniciado diretamente no boot pelo script `/etc/init.d/S99docker-flask` com o comando:

```sh
docker run -d --name flask-tp8 -p 5000:5000 --restart unless-stopped flask-tp8:1.0
```

A opção `-p` publica uma porta do container no host, no formato `PORTA_DO_HOST:PORTA_DO_CONTAINER`:

- o **5000 da esquerda** é a porta da Raspberry Pi;
- o **5000 da direita** é a porta do container, onde o Flask escuta.

Com isso, o Docker cria uma regra de repasse (NAT) na Raspberry: tudo o que chega na porta 5000 da Pi é encaminhado para a porta 5000 do container. Sem o `-p`, o Flask continuaria rodando, mas ninguém de fora da Raspberry conseguiria acessá-lo.

## Por que acessar `http://IP_DA_RASPBERRY:5000` e não o IP do container

O notebook e o roteador só conhecem a rede local, onde a Raspberry tem o IP da `eth0`. A rede bridge do Docker (`172.17.x.x`) é interna da Raspberry, então o notebook não tem como chegar até o IP do container porque ele nem sabe que essa rede existe.

Por isso o acesso é feito pelo IP da Raspberry:

1. o navegador do notebook acessa `http://IP_DA_RASPBERRY:5000`;
2. o pedido chega na `eth0` da Raspberry, na porta 5000;
3. a regra criada pelo `-p 5000:5000` repassa o pedido para o container `flask-tp8`, pela `docker0`;
4. o Flask responde e a resposta faz o caminho de volta até o navegador.

Já dentro da própria Raspberry, dá para testar com `curl http://127.0.0.1:5000`, porque a porta 5000 do host também está publicada localmente.
