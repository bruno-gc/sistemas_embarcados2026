#!/bin/bash
# medir-boot.sh - mede o tempo de boot do CCS a partir de OUTRO equipamento
# na mesma rede (notebook ligado na rede Wi-Fi TP9-GRUPO01 ou no mesmo cabo).
#
# Uso:
#   ./scripts/medir-boot.sh [IP] [arquivo-de-log]
#   (padrao: 192.168.50.1 e boot-AAAAMMDD-HHMMSS.log)
#
# Procedimento (edital, secao 7):
#   1. A Raspberry esta desligada da tomada, com o provisionamento do
#      primeiro boot ja concluido.
#   2. Rode este script e, no mesmo instante, ligue a Raspberry na tomada
#      (filme a tomada e a tela do notebook no mesmo video).
#   3. O script consulta GET /api/status a cada 0,2 s e para na primeira
#      resposta HTTP 200: esse e o fim da medicao.
#   4. Repita 3 vezes (desligar, esperar, rodar de novo).
#
# Observacao: com a aplicacao servida pelo Wi-Fi da propria Raspberry, o
# notebook so volta a se conectar depois que a rede TP9-GRUPO01 reaparece;
# o tempo de associacao entra na medicao, como o edital exige.

IP="${1:-192.168.50.1}"
LOG="${2:-boot-$(date +%Y%m%d-%H%M%S).log}"
URL="http://$IP/api/status"

inicio=$(date +%s.%N)
echo "inicio: $(date '+%F %T.%N') - ligue a Raspberry agora" | tee "$LOG"

while :; do
	resp=$(curl -s --max-time 0.5 -w '\n%{http_code}' "$URL" 2>/dev/null)
	code=${resp##*$'\n'}
	if [ "$code" = "200" ]; then
		fim=$(date +%s.%N)
		corpo=${resp%$'\n'*}
		uptime=$(printf '%s' "$corpo" | sed -n 's/.*"uptime_s": *\([0-9]*\).*/\1/p')
		total=$(echo "$fim - $inicio" | bc)
		echo "fim:    $(date '+%F %T.%N') - primeira resposta 200 de $URL" | tee -a "$LOG"
		printf 'tempo total (tomada -> aplicacao): %.2f s\n' "$total" | tee -a "$LOG"
		echo "uptime informado pela Raspberry (desde o kernel): ${uptime:-?} s" | tee -a "$LOG"
		echo "resposta: $corpo" >>"$LOG"
		break
	fi
	sleep 0.2
done
