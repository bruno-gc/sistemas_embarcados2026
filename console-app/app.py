"""CCS - painel web do Console Contra Sony.

Roda num container Docker na Raspberry e conversa com o menu grafico
(ccs-menu, fora do container) so por arquivos:

  /etc/ccs/jogos.conf  (somente leitura) lista de jogos do menu
  /run/ccs/status      estado atual, escrito pelo menu
  /run/ccs/pedido      pedido do painel: "jogar <id>" ou "parar"

Rotas:
  GET  /              painel (atualiza sozinho)
  GET  /api/jogos     lista de jogos
  GET  /api/status    estado do menu + informacoes do sistema
  POST /api/jogar/ID  abre o jogo na TV
  POST /api/parar     encerra o jogo aberto
  GET  /api/distance  leitura do HC-SR04 do TP9 (se o sensor existir)
"""

import errno
import json
import os
import time

from flask import Flask, jsonify, render_template

JOGOS_CONF = os.environ.get("CCS_JOGOS_CONF", "/etc/ccs/jogos.conf")
RUN_DIR = os.environ.get("CCS_RUN_DIR", "/run/ccs")
DEVICE = os.environ.get("HCSR04_DEVICE", "/dev/hcsr04")
# o menu reescreve o status a cada troca de estado; sem arquivo = menu fechado
ESTADOS = {"menu", "jogando", "terminal"}

app = Flask(__name__)


def ler_jogos():
    jogos = []
    try:
        with open(JOGOS_CONF, encoding="utf-8") as f:
            for linha in f:
                linha = linha.strip()
                if not linha or linha.startswith("#"):
                    continue
                campos = [c.strip() for c in linha.split("|")]
                if len(campos) < 6:
                    continue
                jogos.append({
                    "id": campos[0],
                    "nome": campos[1],
                    "motor": campos[2],
                    "cor": campos[3],
                    "sigla": campos[4],
                })
    except FileNotFoundError:
        pass
    return jogos


def ler_status():
    try:
        with open(os.path.join(RUN_DIR, "status"), encoding="utf-8") as f:
            status = json.load(f)
        if status.get("estado") not in ESTADOS:
            raise ValueError
        return status
    except (FileNotFoundError, ValueError, json.JSONDecodeError):
        return {"estado": "desligado", "jogo": None, "nome": None, "desde": None}


def uptime_s():
    try:
        with open("/proc/uptime") as f:
            return int(float(f.read().split()[0]))
    except OSError:
        return None


def escrever_pedido(texto):
    tmp = os.path.join(RUN_DIR, "pedido.tmp")
    with open(tmp, "w") as f:
        f.write(texto + "\n")
    os.replace(tmp, os.path.join(RUN_DIR, "pedido"))


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/jogos")
def api_jogos():
    return jsonify(ler_jogos())


@app.route("/api/status")
def api_status():
    status = ler_status()
    status["uptime_s"] = uptime_s()
    status["hostname"] = os.environ.get("CCS_HOSTNAME")
    status["sensor"] = os.path.exists(DEVICE)
    status["agora"] = time.time()
    return jsonify(status)


@app.route("/api/jogar/<jogo_id>", methods=["POST"])
def api_jogar(jogo_id):
    jogos = {j["id"]: j for j in ler_jogos()}
    if jogo_id not in jogos:
        return jsonify({"ok": False, "erro": "jogo desconhecido"}), 404
    status = ler_status()
    if status["estado"] != "menu":
        motivo = {
            "jogando": f"ja tem um jogo aberto ({status.get('nome')})",
            "terminal": "o menu foi fechado (modo terminal)",
            "desligado": "o menu da TV nao esta rodando",
        }[status["estado"]]
        return jsonify({"ok": False, "erro": motivo}), 409
    escrever_pedido(f"jogar {jogo_id}")
    return jsonify({"ok": True, "jogo": jogos[jogo_id]["nome"]})


@app.route("/api/parar", methods=["POST"])
def api_parar():
    if ler_status()["estado"] != "jogando":
        return jsonify({"ok": False, "erro": "nenhum jogo aberto"}), 409
    escrever_pedido("parar")
    return jsonify({"ok": True})


@app.route("/api/distance")
def api_distance():
    try:
        with open(DEVICE) as dev:
            mm = int(dev.read().strip())
    except FileNotFoundError:
        return jsonify({"distance_mm": None, "code": "device_missing"}), 503
    except OSError as exc:
        code = "timeout" if exc.errno == errno.ETIMEDOUT else "read_error"
        return jsonify({"distance_mm": None, "code": code}), 503
    except ValueError:
        return jsonify({"distance_mm": None, "code": "invalid_value"}), 502
    return jsonify({"distance_mm": mm, "distance_cm": round(mm / 10, 1)})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, threaded=True)
