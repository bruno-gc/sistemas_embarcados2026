#!/bin/bash
# Testa a aplicacao pela rede a partir de outra maquina
# Uso: ./scripts/test.sh IP_DA_RASPBERRY
IP="$1"
[ -z "$IP" ] && { echo "Uso: $0 IP_DA_RASPBERRY"; exit 1; }
ESPERADO="Helooowww world eng. Computacional!!!!!!!"
RESP="$(curl -s --max-time 5 "http://$IP:5000/")"
[ "$RESP" = "$ESPERADO" ] && echo "OK    /        -> frase exata" || echo "FALHA /        -> recebido: $RESP"
CODE="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://$IP:5000/console")"
[ "$CODE" = "200" ] && echo "OK    /console -> HTTP 200" || echo "FALHA /console -> HTTP $CODE"
