#!/bin/bash
# test.sh - testa a aplicacao a partir de um equipamento ligado na rede TP9-GRUPO01
# Uso: ./scripts/test.sh [IP]   (padrao: 192.168.50.1)
IP="${1:-192.168.50.1}"

CODE="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://$IP/")"
[ "$CODE" = "200" ] && echo "OK    /              -> dashboard (HTTP 200)" \
                    || echo "FALHA /              -> HTTP $CODE"

RESP="$(curl -s --max-time 5 "http://$IP/api/distance")"
if echo "$RESP" | grep -q '"distance_mm": *[0-9]'; then
    echo "OK    /api/distance  -> $RESP"
else
    echo "FALHA /api/distance  -> $RESP"
fi
