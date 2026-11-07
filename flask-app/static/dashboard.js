// Atualiza a distancia a cada ~500 ms sem recarregar a pagina.
// A proxima consulta so comeca quando a anterior termina, para nao
// empilhar pedidos se o sensor demorar a responder.

const INTERVALO_MS = 500;
const ESCALA_MAX_CM = 200;

const painel = document.querySelector(".painel");
const valor = document.getElementById("valor");
const mm = document.getElementById("mm");
const preenchimento = document.getElementById("preenchimento");
const ponto = document.getElementById("ponto");
const statusTexto = document.getElementById("status");

function mostrarLeitura(dados) {
  valor.textContent = dados.distance_cm.toFixed(1);
  mm.textContent = dados.distance_mm;
  const pct = Math.min(dados.distance_cm / ESCALA_MAX_CM, 1) * 100;
  preenchimento.style.width = pct + "%";
  painel.classList.remove("sem-leitura");
  ponto.className = "ponto ok";
  statusTexto.textContent = "Atualizado às " + new Date().toLocaleTimeString("pt-BR");
}

function mostrarErro(mensagem) {
  valor.textContent = "--";
  mm.textContent = "--";
  preenchimento.style.width = "0";
  painel.classList.add("sem-leitura");
  ponto.className = "ponto erro";
  statusTexto.textContent = mensagem;
}

async function atualizar() {
  try {
    const resposta = await fetch("/api/distance", { cache: "no-store" });
    const dados = await resposta.json();
    if (resposta.ok && dados.distance_cm !== null) {
      mostrarLeitura(dados);
    } else {
      mostrarErro(dados.error || "Sem leitura do sensor");
    }
  } catch (e) {
    mostrarErro("Sem conexão com a Raspberry");
  } finally {
    setTimeout(atualizar, INTERVALO_MS);
  }
}

atualizar();
