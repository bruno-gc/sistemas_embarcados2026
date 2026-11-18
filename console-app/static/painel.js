// Painel do CCS: lista os jogos, mostra o que esta na TV e manda pedidos.
const grade = document.getElementById("grade");
const estadoEl = document.getElementById("estado");
const tvEl = document.getElementById("tv");
const pararBtn = document.getElementById("parar");
const avisoEl = document.getElementById("aviso");
const sistemaEl = document.getElementById("sistema");
let jogos = [];
let status = { estado: "desligado" };

function escurecer(hex) {
  const v = parseInt(hex.replace("#", ""), 16);
  const r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
  return `rgb(${r / 3 | 0}, ${g / 3 | 0}, ${b / 3 | 0})`;
}

function avisar(texto) {
  avisoEl.textContent = texto;
  avisoEl.hidden = !texto;
}

function desenharJogos() {
  grade.innerHTML = "";
  for (const j of jogos) {
    const card = document.createElement("article");
    card.className = "jogo";
    card.dataset.id = j.id;
    const capa = document.createElement("div");
    capa.className = "capa";
    capa.textContent = j.sigla;
    capa.style.background = `linear-gradient(${j.cor}, ${escurecer(j.cor)})`;
    const info = document.createElement("div");
    info.className = "info";
    info.innerHTML = '<div class="nome"></div><div class="motor"></div>';
    info.querySelector(".nome").textContent = j.nome;
    info.querySelector(".motor").textContent = j.motor;
    const btn = document.createElement("button");
    btn.textContent = "Jogar na TV";
    btn.addEventListener("click", () => pedir(`/api/jogar/${encodeURIComponent(j.id)}`));
    info.appendChild(btn);
    card.append(capa, info);
    grade.appendChild(card);
  }
  atualizarTela();
}

function duracao(s) {
  if (s == null) return "";
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
  return h ? `${h} h ${m} min` : `${m} min`;
}

function atualizarTela() {
  const textos = {
    menu: "Menu do console",
    terminal: "Menu fechado (modo terminal)",
    desligado: "Menu da TV nao esta rodando",
  };
  const jogando = status.estado === "jogando";
  estadoEl.textContent = jogando ? `Jogando: ${status.nome}` : textos[status.estado] || "--";
  tvEl.classList.toggle("jogando", jogando);
  pararBtn.hidden = !jogando;
  for (const card of grade.children) {
    card.classList.toggle("ativo", jogando && card.dataset.id === status.jogo);
    card.querySelector("button").disabled = status.estado !== "menu";
  }
  if (status.hostname || status.uptime_s != null) {
    sistemaEl.textContent = `${status.hostname || "Raspberry Pi 4"} - ligado ha ${duracao(status.uptime_s)}`;
  }
  document.getElementById("sensor").hidden = !status.sensor;
}

async function pedir(url) {
  try {
    const r = await fetch(url, { method: "POST" });
    const corpo = await r.json();
    avisar(corpo.ok ? "" : corpo.erro);
  } catch (e) {
    avisar("Sem resposta da Raspberry");
  }
  setTimeout(atualizarStatus, 800);
}

async function atualizarStatus() {
  try {
    const r = await fetch("/api/status");
    status = await r.json();
  } catch (e) {
    status = { estado: "desligado" };
    sistemaEl.textContent = "sem conexao com a Raspberry";
  }
  atualizarTela();
  if (status.sensor) {
    try {
      const d = await (await fetch("/api/distance")).json();
      document.getElementById("distancia").textContent =
        d.distance_cm != null ? `${d.distance_cm.toFixed(1)} cm` : "sem leitura";
    } catch (e) { /* sensor e opcional */ }
  }
}

pararBtn.addEventListener("click", () => pedir("/api/parar"));

fetch("/api/jogos").then(r => r.json()).then(lista => { jogos = lista; desenharJogos(); });
atualizarStatus();
setInterval(atualizarStatus, 1000);
