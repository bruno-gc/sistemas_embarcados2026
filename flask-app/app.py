"""TP9 - Backend Flask do medidor ultrassonico HC-SR04.

Le a distancia do dispositivo /dev/hcsr04, criado pelo driver hcsr04.ko,
e entrega:
  GET /              -> dashboard (atualiza sozinho a cada ~500 ms)
  GET /api/distance  -> {"distance_mm": 374, "distance_cm": 37.4, ...}

O container recebe somente o /dev/hcsr04 (docker run --device). A aplicacao
nunca acessa GPIO ou registradores: quem fala com o hardware e o driver.
"""

import errno
import os
import time

from flask import Flask, jsonify, render_template

# Caminho do dispositivo. Na Raspberry e sempre /dev/hcsr04; a variavel de
# ambiente existe so para testar a API no PC, apontando para um arquivo.
DEVICE = os.environ.get("HCSR04_DEVICE", "/dev/hcsr04")

app = Flask(__name__)


def read_distance_mm():
    """Abre o dispositivo, dispara uma medicao no driver e devolve os mm."""
    with open(DEVICE, "r") as dev:
        text = dev.read().strip()
    return int(text)


def error_response(message, code, status):
    """Resposta de erro: o servidor continua no ar e o dashboard mostra o aviso."""
    body = {
        "distance_mm": None,
        "distance_cm": None,
        "error": message,
        "code": code,
        "timestamp": time.time(),
    }
    return jsonify(body), status


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/distance")
def api_distance():
    try:
        mm = read_distance_mm()
    except FileNotFoundError:
        return error_response(f"{DEVICE} nao existe (driver nao carregado?)",
                              "device_missing", 503)
    except OSError as exc:
        if exc.errno == errno.ETIMEDOUT:
            return error_response("sensor sem resposta (sem eco)",
                                  "timeout", 503)
        return error_response(f"falha na leitura do sensor: {exc.strerror}",
                              "read_error", 503)
    except ValueError:
        return error_response("resposta invalida do driver",
                              "invalid_value", 502)

    return jsonify({
        "distance_mm": mm,
        "distance_cm": round(mm / 10, 1),
        "timestamp": time.time(),
    })


if __name__ == "__main__":
    # 0.0.0.0: aceita conexoes de fora do container (o Docker publica 80 -> 5000)
    app.run(host="0.0.0.0", port=5000, threaded=True)
