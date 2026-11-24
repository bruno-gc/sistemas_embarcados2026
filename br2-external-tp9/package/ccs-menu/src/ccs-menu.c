/*
 * ccs-menu - menu grafico do CCS (Console Contra Sony).
 *
 * Roda dentro do X (iniciado pelo S95ccs-menu com xinit), mostra os jogos
 * de /etc/ccs/jogos.conf num carrossel de capas e abre o escolhido com o
 * start.sh dele. A arte (logo, capas, fundos e efeitos) fica em
 * /usr/share/ccs e as fontes Inter em /usr/share/ccs/fontes.
 *
 * Controles: setas / D-pad escolhem, Enter / A / Start jogam, Esc / Select
 * perguntam se quer sair para o terminal.
 *
 * Integracao com o painel web (container Docker):
 *   /run/ccs/status  <- o menu escreve o estado atual em JSON
 *   /run/ccs/pedido  -> o painel escreve "jogar <id>" ou "parar"
 *   /run/ccs/sair    <- criado quando o usuario escolhe sair para o terminal
 *                       (o S95ccs-menu entao nao reabre o menu)
 */
#define _GNU_SOURCE
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <math.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_JOGOS 12
#define CONF_PADRAO "/etc/ccs/jogos.conf"
#define CONTROLES_PADRAO "/etc/ccs/controles.conf"
#define MAX_TECLAS 10
#define DIR_RUN "/run/ccs"
#define DIR_ARTE "/usr/share/ccs"
#define N_PARTICULAS 46

typedef struct {
	char tecla[32];
	char acao[64];
} Controle;

typedef struct {
	char id[32];
	char nome[64];
	char motor[64];
	char sigla[8];
	char script[256];
	char genero[48];
	char descricao[256];
	char resolucao[16]; /* "640x480": a TV muda para essa resolucao no jogo */
	Controle teclas[MAX_TECLAS];
	int nteclas;
	SDL_Color cor;
	SDL_Texture *capa, *fundo;
} Jogo;

static Jogo jogos[MAX_JOGOS];
static int njogos;
static int sel, sel_ant;
static float pos;          /* posicao animada do carrossel */
static Uint32 troca_ms;    /* hora da ultima troca de jogo (fade do fundo) */
static int confirmar_saida;
static int ver_controles;
static char aviso[160];
static Uint32 aviso_ate;

static SDL_Window *win;
static SDL_Renderer *ren;
static int W, H;
static float S; /* escala: 1.0 em 1080p */
static const char *dir_run = DIR_RUN;
static const char *dir_arte = DIR_ARTE;

static SDL_Texture *tx_logo, *tx_emblema, *tx_brilho, *tx_sombra, *tx_painel, *tx_borda, *tx_particula;

enum { F_TITULO, F_RELOGIO, F_META, F_TEXTO, F_DICA, F_CHIP, F_SIGLA, F_BOTAO, N_FONTES };
static TTF_Font *fonte[N_FONTES];

typedef struct {
	float x, y, v, r, a, fase;
} Particula;
static Particula part[N_PARTICULAS];

static const SDL_Color BRANCO = { 244, 246, 252, 255 };
static const SDL_Color FRACO = { 168, 176, 200, 255 };
static const SDL_Color FUNDO = { 7, 8, 16, 255 };

/* ---------- utilidades ---------- */

static SDL_Color hex(const char *s)
{
	unsigned v = 0x4a7dff;
	if (s && *s == '#')
		s++;
	if (s && strlen(s) >= 6)
		sscanf(s, "%6x", &v);
	SDL_Color c = { (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff, 255 };
	return c;
}

static char *trim(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	char *e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
		*--e = 0;
	return s;
}

static float lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

static int px(float v)
{
	return (int)lroundf(v * S);
}

/* id|Nome|Motor|#cor|SIGLA|/caminho/start.sh|Genero|Descricao */
static void carregar_jogos(const char *arq)
{
	FILE *f = fopen(arq, "r");
	char linha[1024];
	if (!f) {
		fprintf(stderr, "ccs-menu: nao abriu %s: %s\n", arq, strerror(errno));
		return;
	}
	while (njogos < MAX_JOGOS && fgets(linha, sizeof linha, f)) {
		char *p = trim(linha), *campo[10] = { 0 };
		int n = 0;
		if (*p == '#' || *p == 0)
			continue;
		for (char *t = strtok(p, "|"); t && n < 10; t = strtok(NULL, "|"))
			campo[n++] = trim(t);
		if (n < 6)
			continue;
		Jogo *j = &jogos[njogos];
		memset(j, 0, sizeof *j);
		snprintf(j->id, sizeof j->id, "%s", campo[0]);
		snprintf(j->nome, sizeof j->nome, "%s", campo[1]);
		snprintf(j->motor, sizeof j->motor, "%s", campo[2]);
		j->cor = hex(campo[3]);
		snprintf(j->sigla, sizeof j->sigla, "%s", campo[4]);
		snprintf(j->script, sizeof j->script, "%s", campo[5]);
		snprintf(j->genero, sizeof j->genero, "%s", n > 6 ? campo[6] : "");
		snprintf(j->descricao, sizeof j->descricao, "%s", n > 7 ? campo[7] : "");
		snprintf(j->resolucao, sizeof j->resolucao, "%s", n > 8 ? campo[8] : "");
		njogos++;
	}
	fclose(f);
}

static int achar_jogo(const char *id);

/* /etc/ccs/controles.conf: id|tecla|acao, na ordem em que aparecem na tela */
static void carregar_controles(const char *arq)
{
	FILE *f = fopen(arq, "r");
	char linha[256];
	if (!f)
		return;
	while (fgets(linha, sizeof linha, f)) {
		char *p = trim(linha), *campo[3] = { 0 };
		int n = 0;
		if (*p == '#' || *p == 0)
			continue;
		for (char *t = strtok(p, "|"); t && n < 3; t = strtok(NULL, "|"))
			campo[n++] = trim(t);
		int i = n == 3 ? achar_jogo(campo[0]) : -1;
		if (i < 0 || jogos[i].nteclas >= MAX_TECLAS)
			continue;
		Controle *c = &jogos[i].teclas[jogos[i].nteclas++];
		snprintf(c->tecla, sizeof c->tecla, "%s", campo[1]);
		snprintf(c->acao, sizeof c->acao, "%s", campo[2]);
	}
	fclose(f);
}

static void escrever_status(const char *estado, const Jogo *j)
{
	char tmp[256], fin[256];
	snprintf(tmp, sizeof tmp, "%s/status.tmp", dir_run);
	snprintf(fin, sizeof fin, "%s/status", dir_run);
	FILE *f = fopen(tmp, "w");
	if (!f)
		return;
	fprintf(f, "{\"estado\": \"%s\", \"jogo\": %s%s%s, \"nome\": %s%s%s, \"desde\": %ld}\n",
		estado,
		j ? "\"" : "", j ? j->id : "null", j ? "\"" : "",
		j ? "\"" : "", j ? j->nome : "null", j ? "\"" : "",
		(long)time(NULL));
	fclose(f);
	rename(tmp, fin);
}

/* le e apaga o pedido do painel web; devolve 1 se havia algum */
static int ler_pedido(char *buf, size_t n)
{
	char arq[256];
	snprintf(arq, sizeof arq, "%s/pedido", dir_run);
	FILE *f = fopen(arq, "r");
	if (!f)
		return 0;
	if (!fgets(buf, n, f))
		buf[0] = 0;
	fclose(f);
	unlink(arq);
	char *t = trim(buf);
	memmove(buf, t, strlen(t) + 1);
	return buf[0] != 0;
}

static int achar_jogo(const char *id)
{
	for (int i = 0; i < njogos; i++)
		if (strcmp(jogos[i].id, id) == 0)
			return i;
	return -1;
}

/* primeiro IPv4 util (prefere a wlan0 do Access Point) */
static void endereco_ip(char *ip, size_t n)
{
	struct ifaddrs *ifa, *p;
	ip[0] = 0;
	if (getifaddrs(&ifa) != 0)
		return;
	for (int pass = 0; pass < 2 && !ip[0]; pass++)
		for (p = ifa; p; p = p->ifa_next) {
			if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || (p->ifa_flags & IFF_LOOPBACK))
				continue;
			if (!strncmp(p->ifa_name, "docker", 6) || !strncmp(p->ifa_name, "veth", 4))
				continue;
			if (pass == 0 && strcmp(p->ifa_name, "wlan0") != 0)
				continue;
			inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr, ip, n);
			break;
		}
	freeifaddrs(ifa);
}

/* ---------- cache de textos (renderizar texto toda hora pesa na CPU) ---------- */

typedef struct {
	TTF_Font *f;
	Uint32 cor, quebra, uso;
	char s[256];
	SDL_Texture *t;
	int w, h;
} TextoCache;
static TextoCache cache[128];
static Uint32 relogio_cache;

static TextoCache *texto_tx(TTF_Font *f, const char *s, SDL_Color c, int quebra)
{
	Uint32 cor = (c.r << 16) | (c.g << 8) | c.b;
	int alvo = -1;
	for (int i = 0; i < (int)(sizeof cache / sizeof cache[0]); i++) {
		TextoCache *e = &cache[i];
		if (e->t && e->f == f && e->cor == cor && e->quebra == (Uint32)quebra && !strcmp(e->s, s)) {
			e->uso = ++relogio_cache;
			return e;
		}
		if (alvo < 0 || !e->t || (cache[alvo].t && e->uso < cache[alvo].uso))
			alvo = i; /* entrada livre ou a usada ha mais tempo */
	}
	TextoCache *e = &cache[alvo];
	if (e->t)
		SDL_DestroyTexture(e->t);
	memset(e, 0, sizeof *e);
	SDL_Color opaca = { c.r, c.g, c.b, 255 };
	SDL_Surface *sf = quebra > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, s, opaca, quebra)
				     : TTF_RenderUTF8_Blended(f, s, opaca);
	if (!sf)
		return NULL;
	e->t = SDL_CreateTextureFromSurface(ren, sf);
	e->w = sf->w;
	e->h = sf->h;
	SDL_FreeSurface(sf);
	e->f = f;
	e->cor = cor;
	e->quebra = quebra;
	e->uso = ++relogio_cache;
	snprintf(e->s, sizeof e->s, "%s", s);
	return e;
}

static void limpar_cache(void)
{
	for (int i = 0; i < (int)(sizeof cache / sizeof cache[0]); i++) {
		if (cache[i].t)
			SDL_DestroyTexture(cache[i].t);
		memset(&cache[i], 0, sizeof cache[i]);
	}
}

/* alinhar: 0 esquerda, 1 centro, 2 direita; devolve a largura */
static int texto(int f, const char *s, int x, int y, SDL_Color c, Uint8 alfa, int alinhar)
{
	if (!fonte[f] || !s || !*s)
		return 0;
	TextoCache *e = texto_tx(fonte[f], s, c, 0);
	if (!e)
		return 0;
	SDL_Rect r = { x, y, e->w, e->h };
	if (alinhar == 1)
		r.x -= e->w / 2;
	else if (alinhar == 2)
		r.x -= e->w;
	SDL_SetTextureAlphaMod(e->t, alfa);
	SDL_RenderCopy(ren, e->t, NULL, &r);
	return e->w;
}

static int largura(int f, const char *s)
{
	int w = 0, h;
	if (fonte[f] && s)
		TTF_SizeUTF8(fonte[f], s, &w, &h);
	return w;
}

static int altura(int f)
{
	return fonte[f] ? TTF_FontHeight(fonte[f]) : 0;
}

/* ---------- desenho ---------- */

static void retangulo(int x, int y, int w, int h, SDL_Color c, Uint8 a)
{
	SDL_Rect r = { x, y, w, h };
	SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a);
	SDL_RenderFillRect(ren, &r);
}

/* gradiente vertical: cor a (alfa aa) em cima, cor b (alfa ab) embaixo */
static void gradiente(int x, int y, int w, int h, SDL_Color a, Uint8 aa, SDL_Color b, Uint8 ab)
{
	SDL_Color ca = { a.r, a.g, a.b, aa }, cb = { b.r, b.g, b.b, ab };
	SDL_Vertex v[4] = {
		{ { x, y }, ca, { 0, 0 } },
		{ { x + w, y }, ca, { 0, 0 } },
		{ { x + w, y + h }, cb, { 0, 0 } },
		{ { x, y + h }, cb, { 0, 0 } },
	};
	int idx[6] = { 0, 1, 2, 0, 2, 3 };
	SDL_RenderGeometry(ren, NULL, v, 4, idx, 6);
}

/* gradiente horizontal (esquerda -> direita) */
static void gradiente_h(int x, int y, int w, int h, SDL_Color c, Uint8 aa, Uint8 ab)
{
	SDL_Color ca = { c.r, c.g, c.b, aa }, cb = { c.r, c.g, c.b, ab };
	SDL_Vertex v[4] = {
		{ { x, y }, ca, { 0, 0 } },
		{ { x + w, y }, cb, { 0, 0 } },
		{ { x + w, y + h }, cb, { 0, 0 } },
		{ { x, y + h }, ca, { 0, 0 } },
	};
	int idx[6] = { 0, 1, 2, 0, 2, 3 };
	SDL_RenderGeometry(ren, NULL, v, 4, idx, 6);
}

/* textura "9-slice": cantos arredondados que nao esticam */
static void nove(SDL_Texture *t, int x, int y, int w, int h, int canto, SDL_Color c, Uint8 a)
{
	if (!t || w <= 0 || h <= 0)
		return;
	int tw, th, k = 30; /* tamanho do canto dentro da textura 96x96 */
	SDL_QueryTexture(t, NULL, NULL, &tw, &th);
	if (canto * 2 > h)
		canto = h / 2;
	if (canto * 2 > w)
		canto = w / 2;
	SDL_SetTextureColorMod(t, c.r, c.g, c.b);
	SDL_SetTextureAlphaMod(t, a);
	int sx[4] = { 0, k, tw - k, tw }, sy[4] = { 0, k, th - k, th };
	int dx[4] = { x, x + canto, x + w - canto, x + w }, dy[4] = { y, y + canto, y + h - canto, y + h };
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++) {
			SDL_Rect s = { sx[i], sy[j], sx[i + 1] - sx[i], sy[j + 1] - sy[j] };
			SDL_Rect d = { dx[i], dy[j], dx[i + 1] - dx[i], dy[j + 1] - dy[j] };
			if (d.w > 0 && d.h > 0)
				SDL_RenderCopy(ren, t, &s, &d);
		}
}

static void triangulo_play(int cx, int cy, int r, SDL_Color c)
{
	SDL_Vertex v[3] = {
		{ { cx - r * 0.55f, cy - r * 0.7f }, c, { 0, 0 } },
		{ { cx + r * 0.75f, cy }, c, { 0, 0 } },
		{ { cx - r * 0.55f, cy + r * 0.7f }, c, { 0, 0 } },
	};
	SDL_RenderGeometry(ren, NULL, v, 3, NULL, 0);
}

/* tecla desenhada: [Enter] */
static int tecla(int x, int y, const char *rotulo)
{
	int w = largura(F_CHIP, rotulo) + px(22), h = altura(F_CHIP) + px(8);
	if (w < h)
		w = h;
	nove(tx_painel, x, y, w, h, px(9), BRANCO, 34);
	nove(tx_borda, x, y, w, h, px(9), BRANCO, 70);
	texto(F_CHIP, rotulo, x + w / 2, y + px(4), BRANCO, 235, 1);
	return w;
}

/* dica do rodape: teclas + descricao; devolve a largura total */
static int dica(int x, int y, const char *t1, const char *t2, const char *desc)
{
	int w = tecla(x, y, t1);
	if (t2)
		w += px(6) + tecla(x + w + px(6), y, t2);
	int ty = y + (altura(F_CHIP) + px(8) - altura(F_DICA)) / 2;
	w += px(12) + texto(F_DICA, desc, x + w + px(12), ty, FRACO, 255, 0);
	return w;
}

/* uma linha da lista de controles: teclas (separadas por "/") + acao */
static int linha_controle(int x, int y, int col_acao, const Controle *c)
{
	char buf[32];
	snprintf(buf, sizeof buf, "%s", c->tecla);
	int cx = x;
	for (char *t = strtok(buf, "/"); t; t = strtok(NULL, "/")) {
		char *k = trim(t);
		if (cx > x)
			cx += px(8);
		cx += tecla(cx, y, k);
	}
	int ty = y + (altura(F_CHIP) + px(8) - altura(F_TEXTO)) / 2;
	texto(F_TEXTO, c->acao, (cx + px(18) > x + col_acao) ? cx + px(18) : x + col_acao, ty, BRANCO, 235, 0);
	return altura(F_CHIP) + px(8);
}

/* lista de controles em uma ou duas colunas; devolve a altura usada */
static int lista_controles(const Jogo *j, int x, int y, int w)
{
	if (!j->nteclas) {
		texto(F_TEXTO, "Controles padrão do jogo", x, y, FRACO, 255, 0);
		return altura(F_TEXTO);
	}
	int colunas = (j->nteclas > 5 && w > px(900)) ? 2 : 1;
	int por_coluna = (j->nteclas + colunas - 1) / colunas;
	int lw = w / colunas, passo = altura(F_CHIP) + px(22);
	for (int i = 0; i < j->nteclas; i++) {
		int c = i / por_coluna, l = i % por_coluna;
		linha_controle(x + c * lw, y + l * passo, px(240), &j->teclas[i]);
	}
	return por_coluna * passo;
}

static void desenhar_fundo(void)
{
	retangulo(0, 0, W, H, FUNDO, 255);
	Uint32 t = SDL_GetTicks() - troca_ms;
	float a = t > 450 ? 1.0f : t / 450.0f;
	SDL_Rect tela = { 0, 0, W, H };
	if (a < 1.0f && jogos[sel_ant].fundo) {
		SDL_SetTextureAlphaMod(jogos[sel_ant].fundo, 255);
		SDL_RenderCopy(ren, jogos[sel_ant].fundo, NULL, &tela);
	}
	if (jogos[sel].fundo) {
		SDL_SetTextureAlphaMod(jogos[sel].fundo, (Uint8)(a * 255));
		SDL_RenderCopy(ren, jogos[sel].fundo, NULL, &tela);
	} else {
		gradiente(0, 0, W, H, jogos[sel].cor, (Uint8)(90 * a), FUNDO, 255);
	}
	/* escurece em cima e embaixo, deixa o meio com a cor do jogo */
	int meio = (int)(H * 0.42f);
	gradiente(0, 0, W, meio, FUNDO, 225, FUNDO, 70);
	gradiente(0, meio, W, H - meio, FUNDO, 70, FUNDO, 248);
	gradiente_h(0, 0, W / 3, H, FUNDO, 110, 0);
}

static void desenhar_particulas(float dt)
{
	if (!tx_particula)
		return;
	SDL_Color c = jogos[sel].cor;
	SDL_SetTextureBlendMode(tx_particula, SDL_BLENDMODE_ADD);
	SDL_SetTextureColorMod(tx_particula, (c.r + 255) / 2, (c.g + 255) / 2, (c.b + 255) / 2);
	for (int i = 0; i < N_PARTICULAS; i++) {
		Particula *p = &part[i];
		p->y -= p->v * dt;
		p->fase += dt * 0.6f;
		if (p->y < -0.05f) {
			p->y = 1.05f;
			p->x = (float)rand() / RAND_MAX;
		}
		float x = (p->x + 0.012f * sinf(p->fase)) * W, y = p->y * H;
		int r = (int)(p->r * S);
		SDL_Rect d = { (int)x - r, (int)y - r, 2 * r, 2 * r };
		SDL_SetTextureAlphaMod(tx_particula, (Uint8)(p->a * (0.6f + 0.4f * sinf(p->fase * 2))));
		SDL_RenderCopy(ren, tx_particula, NULL, &d);
	}
	SDL_SetTextureBlendMode(tx_particula, SDL_BLENDMODE_BLEND);
}

static void desenhar_cabecalho(void)
{
	int m = px(96), y = px(54);
	if (tx_logo) {
		int lh = px(82), lw = lh * 1240 / 300;
		SDL_Rect r = { m, y, lw, lh };
		SDL_RenderCopy(ren, tx_logo, NULL, &r);
	} else {
		texto(F_RELOGIO, "CCS  Console Contra Sony", m, y, BRANCO, 255, 0);
	}
	char hora[16], data[64];
	static const char *dias[] = { "dom", "seg", "ter", "qua", "qui", "sex", "sab" };
	static const char *meses[] = { "jan", "fev", "mar", "abr", "mai", "jun",
				       "jul", "ago", "set", "out", "nov", "dez" };
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	strftime(hora, sizeof hora, "%H:%M", tm);
	snprintf(data, sizeof data, "%s, %d %s", dias[tm->tm_wday], tm->tm_mday, meses[tm->tm_mon]);
	texto(F_RELOGIO, hora, W - m, y - px(2), BRANCO, 255, 2);
	texto(F_DICA, data, W - m, y + altura(F_RELOGIO) - px(4), FRACO, 255, 2);
}

/* um card do carrossel; d = distancia (em cards) ate o selecionado */
static void desenhar_card(int i, float cx, float cy, float d, Uint32 agora)
{
	Jogo *j = &jogos[i];
	float ad = fabsf(d);
	float k = 1.0f + 0.24f * fmaxf(0.0f, 1.0f - ad);
	int w = (int)(px(264) * k), h = w * 4 / 3;
	int x = (int)cx - w / 2, y = (int)cy - h / 2 - (int)(px(10) * fmaxf(0.0f, 1.0f - ad));
	Uint8 alfa = (Uint8)lerp(255, 170, fminf(1.0f, ad));

	if (tx_sombra) { /* sombra */
		SDL_Rect s = { x - w / 6, y + h / 10, w + w / 3, h + h / 4 };
		SDL_SetTextureColorMod(tx_sombra, 0, 0, 0);
		SDL_SetTextureAlphaMod(tx_sombra, (Uint8)(alfa * 0.75f));
		SDL_RenderCopy(ren, tx_sombra, NULL, &s);
	}
	if (ad < 0.5f && tx_brilho) { /* aura pulsando na cor do jogo */
		float pulso = 0.75f + 0.25f * sinf(agora / 420.0f);
		SDL_Rect g = { x - w * 26 / 100, y - h * 20 / 100, w + w * 52 / 100, h + h * 40 / 100 };
		SDL_SetTextureBlendMode(tx_brilho, SDL_BLENDMODE_ADD);
		SDL_SetTextureColorMod(tx_brilho, j->cor.r, j->cor.g, j->cor.b);
		SDL_SetTextureAlphaMod(tx_brilho, (Uint8)(230 * pulso * (1.0f - ad * 2)));
		SDL_RenderCopy(ren, tx_brilho, NULL, &g);
	}
	SDL_Rect r = { x, y, w, h };
	if (j->capa) {
		SDL_SetTextureAlphaMod(j->capa, alfa);
		SDL_RenderCopy(ren, j->capa, NULL, &r);
	} else { /* sem capa: card com a sigla */
		SDL_Color c2 = { j->cor.r / 3, j->cor.g / 3, j->cor.b / 3, 255 };
		nove(tx_painel, x, y, w, h, px(20), (SDL_Color){ 24, 28, 44, 255 }, alfa);
		gradiente(x + px(6), y + px(6), w - px(12), h * 6 / 10, j->cor, alfa, c2, alfa);
		texto(F_SIGLA, j->sigla, x + w / 2, y + h * 3 / 10 - altura(F_SIGLA) / 2, BRANCO, alfa, 1);
		texto(F_CHIP, j->nome, x + px(18), y + h * 7 / 10, BRANCO, alfa, 0);
	}
	if (ad >= 0.5f) /* escurece os que nao estao escolhidos */
		nove(tx_painel, x, y, w, h, px(16), FUNDO, (Uint8)(90 * fminf(1.0f, ad)));
	else
		nove(tx_borda, x - px(3), y - px(3), w + px(6), h + px(6), px(22), BRANCO,
		     (Uint8)(220 * (1.0f - ad * 2)));
}

static void desenhar_carrossel(Uint32 agora)
{
	float cy = H * 0.415f, passo = px(264) + px(52);
	/* do mais longe para o mais perto, para o escolhido ficar por cima */
	for (int dist = njogos; dist >= 0; dist--)
		for (int i = 0; i < njogos; i++) {
			float d = i - pos;
			if ((int)lroundf(fabsf(d)) != dist)
				continue;
			float cx = W / 2.0f + d * passo + (d > 0 ? 1 : -1) * px(34) * fminf(1.0f, fabsf(d));
			if (cx < -passo || cx > W + passo)
				continue;
			desenhar_card(i, cx, cy, d, agora);
		}
	/* indicador de pagina */
	int dy = (int)(cy + px(264) * 1.24f * 4 / 6) + px(40), tot = 0;
	for (int i = 0; i < njogos; i++)
		tot += (i == sel ? px(34) : px(10)) + px(10);
	int x = W / 2 - tot / 2;
	for (int i = 0; i < njogos; i++) {
		int w = i == sel ? px(34) : px(10);
		nove(tx_painel, x, dy, w, px(10), px(5), i == sel ? jogos[i].cor : BRANCO, i == sel ? 255 : 90);
		x += w + px(10);
	}
}

static void desenhar_info(void)
{
	Jogo *j = &jogos[sel];
	int m = px(96), y = (int)(H * 0.695f);
	texto(F_TITULO, j->nome, m, y, BRANCO, 255, 0);
	y += altura(F_TITULO) + px(4);

	int x = m;
	if (j->genero[0]) { /* chip do genero na cor do jogo */
		int cw = largura(F_CHIP, j->genero) + px(26), chh = altura(F_CHIP) + px(10);
		nove(tx_painel, x, y, cw, chh, chh / 2, j->cor, 235);
		texto(F_CHIP, j->genero, x + cw / 2, y + px(5), BRANCO, 255, 1);
		x += cw + px(16);
	}
	texto(F_META, j->motor, x, y + (altura(F_CHIP) + px(10) - altura(F_META)) / 2, FRACO, 255, 0);
	y += altura(F_CHIP) + px(24);
	if (j->descricao[0]) {
		TextoCache *e = texto_tx(fonte[F_TEXTO], j->descricao, BRANCO, (int)(W * 0.56f));
		if (e) {
			SDL_Rect r = { m, y, e->w, e->h };
			SDL_SetTextureAlphaMod(e->t, 215);
			SDL_RenderCopy(ren, e->t, NULL, &r);
		}
	}

	/* botao JOGAR */
	int bw = px(300), bh = px(84), bx = W - m - bw, by = (int)(H * 0.745f);
	nove(tx_painel, bx, by + px(6), bw, bh, bh / 2, FUNDO, 120);
	nove(tx_painel, bx, by, bw, bh, bh / 2, BRANCO, 250);
	triangulo_play(bx + px(62), by + bh / 2, px(18), j->cor);
	texto(F_BOTAO, "JOGAR", bx + px(96), by + (bh - altura(F_BOTAO)) / 2, (SDL_Color){ 12, 14, 26, 255 }, 255, 0);
	texto(F_DICA, "Enter  ·  X no controle", bx + bw / 2, by + bh + px(14), FRACO, 255, 1);
}

static void desenhar_rodape(void)
{
	int m = px(96), y = H - px(74);
	gradiente(0, y - px(30), W, H - y + px(30), FUNDO, 0, FUNDO, 200);
	retangulo(m, y - px(20), W - 2 * m, 1, BRANCO, 28);
	int x = m;
	x += dica(x, y, "←", "→", "Escolher") + px(40);
	x += dica(x, y, "Enter", NULL, "Jogar") + px(40);
	x += dica(x, y, "C", NULL, "Controles") + px(40);
	x += dica(x, y, "Esc", NULL, "Terminal") + px(40);
	texto(F_DICA, "No jogo: Create + Options (2 s) volta ao menu", x, y + (altura(F_CHIP) + px(8) - altura(F_DICA)) / 2, FRACO, 160, 0);

	char ip[64], linha[160];
	endereco_ip(ip, sizeof ip);
	if (ip[0])
		snprintf(linha, sizeof linha, "Painel no celular:  http://%s", ip);
	else
		snprintf(linha, sizeof linha, "Sem rede");
	int lw = largura(F_DICA, linha) + px(50), lh = altura(F_CHIP) + px(8);
	nove(tx_painel, W - m - lw, y, lw, lh, lh / 2, BRANCO, 26);
	SDL_Color verde = { 80, 220, 140, 255 };
	nove(tx_painel, W - m - lw + px(16), y + lh / 2 - px(5), px(10), px(10), px(5), ip[0] ? verde : FRACO, 255);
	texto(F_DICA, linha, W - m - lw + px(34), y + (lh - altura(F_DICA)) / 2, BRANCO, 230, 0);
}

static void desenhar_aviso(void)
{
	if (!aviso[0] || SDL_GetTicks() > aviso_ate)
		return;
	int w = largura(F_META, aviso) + px(56), h = altura(F_META) + px(24);
	int x = (W - w) / 2, y = px(170);
	nove(tx_painel, x, y, w, h, h / 2, (SDL_Color){ 60, 40, 8, 255 }, 240);
	nove(tx_borda, x, y, w, h, h / 2, (SDL_Color){ 255, 196, 64, 255 }, 200);
	texto(F_META, aviso, W / 2, y + px(12), (SDL_Color){ 255, 214, 120, 255 }, 255, 1);
}

static void desenhar_confirmacao(void)
{
	retangulo(0, 0, W, H, FUNDO, 200);
	int bw = px(760), bh = px(380), bx = (W - bw) / 2, by = (H - bh) / 2;
	nove(tx_painel, bx, by, bw, bh, px(28), (SDL_Color){ 20, 23, 38, 255 }, 250);
	nove(tx_borda, bx, by, bw, bh, px(28), BRANCO, 40);
	if (tx_emblema) {
		SDL_Rect e = { W / 2 - px(40), by + px(36), px(80), px(80) };
		SDL_RenderCopy(ren, tx_emblema, NULL, &e);
	}
	texto(F_RELOGIO, "Sair para o terminal?", W / 2, by + px(136), BRANCO, 255, 1);
	texto(F_DICA, "Para voltar ao menu: Ctrl+Alt+F2 ou reinicie o console.", W / 2, by + px(200), FRACO, 255, 1);
	int w2 = px(250), h2 = px(70), y2 = by + bh - h2 - px(40);
	int xv = W / 2 - w2 - px(14), xs = W / 2 + px(14);
	nove(tx_borda, xv, y2, w2, h2, h2 / 2, BRANCO, 140);
	texto(F_META, "Voltar  (Esc)", xv + w2 / 2, y2 + (h2 - altura(F_META)) / 2, BRANCO, 255, 1);
	nove(tx_painel, xs, y2, w2, h2, h2 / 2, (SDL_Color){ 224, 72, 62, 255 }, 255);
	texto(F_META, "Sair  (Enter)", xs + w2 / 2, y2 + (h2 - altura(F_META)) / 2, BRANCO, 255, 1);
}

static void desenhar_controles(void)
{
	Jogo *j = &jogos[sel];
	retangulo(0, 0, W, H, FUNDO, 215);
	int colunas = (j->nteclas > 5) ? 2 : 1;
	int linhas = j->nteclas ? (j->nteclas + colunas - 1) / colunas : 1;
	int bw = colunas == 2 ? px(1320) : px(820);
	int bh = px(200) + linhas * (altura(F_CHIP) + px(22)) + px(60);
	int bx = (W - bw) / 2, by = (H - bh) / 2;
	nove(tx_painel, bx, by, bw, bh, px(28), (SDL_Color){ 20, 23, 38, 255 }, 250);
	nove(tx_borda, bx, by, bw, bh, px(28), j->cor, 120);
	char t[96];
	snprintf(t, sizeof t, "Controles · %s", j->nome);
	texto(F_RELOGIO, t, bx + px(56), by + px(44), BRANCO, 255, 0);
	texto(F_DICA, "Teclado  ·  no controle de PS5 os botões seguem o padrão do jogo", bx + px(56),
	      by + px(44) + altura(F_RELOGIO) + px(8), FRACO, 255, 0);
	lista_controles(j, bx + px(56), by + px(170), bw - px(112));
	texto(F_DICA, "C / Triângulo / Esc: fechar      Enter: jogar", W / 2, by + bh - px(50), FRACO, 255, 1);
}

static void desenhar(float dt)
{
	Uint32 agora = SDL_GetTicks();
	SDL_GetRendererOutputSize(ren, &W, &H);
	pos = lerp(pos, (float)sel, fminf(1.0f, dt * 12.0f));
	if (fabsf(pos - sel) < 0.002f)
		pos = (float)sel;

	if (njogos == 0) {
		retangulo(0, 0, W, H, FUNDO, 255);
		desenhar_cabecalho();
		texto(F_RELOGIO, "Nenhum jogo em /etc/ccs/jogos.conf", W / 2, H / 2, FRACO, 255, 1);
		SDL_RenderPresent(ren);
		return;
	}
	desenhar_fundo();
	desenhar_particulas(dt);
	desenhar_cabecalho();
	desenhar_carrossel(agora);
	desenhar_info();
	desenhar_rodape();
	desenhar_aviso();
	if (ver_controles && !confirmar_saida)
		desenhar_controles();
	if (confirmar_saida)
		desenhar_confirmacao();
	SDL_RenderPresent(ren);
}

/* tela "Abrindo X": capa + controles do jogo, por uns 4 s (Enter/A pula) */
static void tela_carregando(Jogo *j)
{
	const int quadros = 160; /* 160 x 25 ms = 4 s */
	for (int quadro = 0; quadro <= quadros; quadro++) {
		float t = (float)quadro / quadros;
		SDL_Rect tela = { 0, 0, W, H };
		retangulo(0, 0, W, H, FUNDO, 255);
		if (j->fundo) {
			SDL_SetTextureAlphaMod(j->fundo, 255);
			SDL_RenderCopy(ren, j->fundo, NULL, &tela);
		}
		retangulo(0, 0, W, H, FUNDO, 150);
		int w = px(360), h = w * 4 / 3, x = px(170), y = (H - h) / 2 - px(30);
		if (tx_brilho) {
			SDL_Rect g = { x - w / 4, y - h / 5, w + w / 2, h + h * 2 / 5 };
			SDL_SetTextureBlendMode(tx_brilho, SDL_BLENDMODE_ADD);
			SDL_SetTextureColorMod(tx_brilho, j->cor.r, j->cor.g, j->cor.b);
			SDL_SetTextureAlphaMod(tx_brilho, 255);
			SDL_RenderCopy(ren, tx_brilho, NULL, &g);
		}
		if (j->capa) {
			SDL_Rect r = { x, y, w, h };
			SDL_SetTextureAlphaMod(j->capa, 255);
			SDL_RenderCopy(ren, j->capa, NULL, &r);
		}
		int px0 = x + w + px(110), pw = W - px0 - px(130), py = y + px(10);
		char msg[96];
		snprintf(msg, sizeof msg, "Abrindo %s", j->nome);
		texto(F_TITULO, msg, px0, py, BRANCO, 255, 0);
		py += altura(F_TITULO) + px(30);
		texto(F_META, "CONTROLES", px0, py, j->cor, 255, 0);
		py += altura(F_META) + px(22);
		py += lista_controles(j, px0, py, pw) + px(30);
		texto(F_DICA, "No jogo: Create + Options (2 s) no controle volta ao menu", px0, py, FRACO, 255, 0);

		int bw = px(520), bx = px0, by = y + h - px(8);
		nove(tx_painel, bx, by, bw, px(8), px(4), BRANCO, 40);
		nove(tx_painel, bx, by, (int)(bw * (0.05f + 0.95f * t)), px(8), px(4), j->cor, 255);
		texto(F_DICA, "Enter pula", bx + bw + px(24), by - px(10), FRACO, 200, 0);
		SDL_RenderPresent(ren);

		SDL_Event e;
		int pular = 0;
		while (SDL_PollEvent(&e)) {
			if (quadro < 20)
				continue; /* ignora o Enter que abriu o jogo */
			if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER ||
						      e.key.keysym.sym == SDLK_SPACE))
				pular = 1;
			if (e.type == SDL_CONTROLLERBUTTONDOWN && (e.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
								  e.cbutton.button == SDL_CONTROLLER_BUTTON_START))
				pular = 1;
			if (e.type == SDL_JOYBUTTONDOWN)
				pular = 1;
		}
		if (pular)
			break;
		SDL_Delay(25);
	}
}

/* ---------- janela e arte ---------- */

static SDL_Texture *carregar(const char *rel)
{
	char arq[512];
	snprintf(arq, sizeof arq, "%s/%s", dir_arte, rel);
	SDL_Texture *t = IMG_LoadTexture(ren, arq);
	if (t)
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
	else
		fprintf(stderr, "ccs-menu: sem arte %s (%s)\n", arq, IMG_GetError());
	return t;
}

static TTF_Font *abrir_fonte(const char *peso, int tam)
{
	const char *dir = getenv("CCS_FONT_DIR");
	char arq[512];
	snprintf(arq, sizeof arq, "%s/Inter-%s.otf", dir ? dir : DIR_ARTE "/fontes", peso);
	TTF_Font *f = TTF_OpenFont(arq, tam);
	if (!f) { /* reserva: DejaVu, que vem no Buildroot */
		snprintf(arq, sizeof arq, "/usr/share/fonts/dejavu/DejaVuSans%s.ttf",
			 strcmp(peso, "Regular") ? "-Bold" : "");
		f = TTF_OpenFont(arq, tam);
	}
	if (!f)
		fprintf(stderr, "ccs-menu: fonte %s: %s\n", peso, TTF_GetError());
	return f;
}

static void fechar_recursos(void)
{
	limpar_cache();
	for (int i = 0; i < N_FONTES; i++)
		if (fonte[i]) {
			TTF_CloseFont(fonte[i]);
			fonte[i] = NULL;
		}
	SDL_Texture **v[] = { &tx_logo, &tx_emblema, &tx_brilho, &tx_sombra, &tx_painel, &tx_borda, &tx_particula };
	for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++)
		if (*v[i]) {
			SDL_DestroyTexture(*v[i]);
			*v[i] = NULL;
		}
	for (int i = 0; i < njogos; i++) {
		if (jogos[i].capa)
			SDL_DestroyTexture(jogos[i].capa);
		if (jogos[i].fundo)
			SDL_DestroyTexture(jogos[i].fundo);
		jogos[i].capa = jogos[i].fundo = NULL;
	}
}

static int abrir_janela(void)
{
	int w = 1280, h = 720;
	const char *geo = getenv("CCS_GEOMETRY"); /* para testes: 1920x1080 */
	Uint32 flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
	if (geo && sscanf(geo, "%dx%d", &w, &h) == 2)
		flags = 0;
	win = SDL_CreateWindow("CCS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
	if (!win)
		return -1;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!ren)
		ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
	if (!ren)
		return -1;
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_GetRendererOutputSize(ren, &W, &H);
	S = H / 1080.0f;

	fechar_recursos();
	fonte[F_TITULO] = abrir_fonte("Black", px(68));
	fonte[F_RELOGIO] = abrir_fonte("Bold", px(44));
	fonte[F_META] = abrir_fonte("SemiBold", px(26));
	fonte[F_TEXTO] = abrir_fonte("Regular", px(27));
	fonte[F_DICA] = abrir_fonte("SemiBold", px(22));
	fonte[F_CHIP] = abrir_fonte("Bold", px(20));
	fonte[F_SIGLA] = abrir_fonte("Black", px(120));
	fonte[F_BOTAO] = abrir_fonte("Black", px(34));

	tx_logo = carregar("logo.png");
	tx_emblema = carregar("emblema.png");
	tx_brilho = carregar("fx/brilho.png");
	tx_sombra = carregar("fx/sombra.png");
	tx_painel = carregar("fx/painel.png");
	tx_borda = carregar("fx/borda.png");
	tx_particula = carregar("fx/particula.png");
	for (int i = 0; i < njogos; i++) {
		char rel[128];
		snprintf(rel, sizeof rel, "capas/%s.png", jogos[i].id);
		jogos[i].capa = carregar(rel);
		snprintf(rel, sizeof rel, "capas/%s-fundo.jpg", jogos[i].id);
		jogos[i].fundo = carregar(rel);
	}
	for (int i = 0; i < N_FONTES; i++)
		if (!fonte[i])
			return -1;
	return 0;
}

static void fechar_janela(void)
{
	fechar_recursos();
	if (ren)
		SDL_DestroyRenderer(ren);
	if (win)
		SDL_DestroyWindow(win);
	ren = NULL;
	win = NULL;
}

/* ---------- controles ---------- */

/* Controles reconhecidos pelo SDL (Xbox, PlayStation, 8BitDo...) viram
 * SDL_GameController. Controles genericos sem mapeamento ficam como
 * SDL_Joystick "cru": direcional pelo hat/eixo 0, botoes 0-3 = confirmar,
 * 8 = Select, 9 = Start (o padrao dos controles USB baratos). */
#define MAX_CONTROLES 8
static SDL_GameController *ctl[MAX_CONTROLES];
static SDL_Joystick *joy[MAX_CONTROLES];

static void abrir_controle(int idx)
{
	for (int i = 0; i < MAX_CONTROLES; i++) {
		if (ctl[i] || joy[i])
			continue;
		if (SDL_IsGameController(idx))
			ctl[i] = SDL_GameControllerOpen(idx);
		else
			joy[i] = SDL_JoystickOpen(idx);
		return;
	}
}

static int eh_controle_mapeado(SDL_JoystickID id)
{
	return SDL_GameControllerFromInstanceID(id) != NULL;
}

/* Select + Start segurados juntos (em qualquer controle) */
static int atalho_sair_apertado(void)
{
	SDL_GameControllerUpdate();
	SDL_JoystickUpdate();
	for (int i = 0; i < MAX_CONTROLES; i++) {
		if (ctl[i] && SDL_GameControllerGetButton(ctl[i], SDL_CONTROLLER_BUTTON_BACK) &&
		    SDL_GameControllerGetButton(ctl[i], SDL_CONTROLLER_BUTTON_START))
			return 1;
		if (joy[i] && SDL_JoystickNumButtons(joy[i]) > 9 && SDL_JoystickGetButton(joy[i], 8) &&
		    SDL_JoystickGetButton(joy[i], 9))
			return 1;
	}
	return 0;
}

/* ---------- jogar ---------- */

static void encerrar_jogo(pid_t pid)
{
	int st;
	kill(-pid, SIGTERM);
	for (int k = 0; k < 30 && waitpid(pid, &st, WNOHANG) == 0; k++)
		usleep(100000);
	kill(-pid, SIGKILL);
}

static void mostrar_aviso(const char *msg)
{
	snprintf(aviso, sizeof aviso, "%s", msg);
	aviso_ate = SDL_GetTicks() + 4000;
}

static void jogar(int i)
{
	Jogo *j = &jogos[i];
	char buf[64];

	if (access(j->script, R_OK) != 0) {
		char msg[128];
		snprintf(msg, sizeof msg, "%s não está instalado neste cartão", j->nome);
		mostrar_aviso(msg);
		return;
	}
	tela_carregando(j);

	/* libera a tela para o jogo; o controle continua aberto aqui so para o
	 * atalho Select + Start (o jogo le o mesmo controle normalmente) */
	fechar_janela();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if (j->resolucao[0]) {
		char cmd[96];
		snprintf(cmd, sizeof cmd, "/usr/bin/ccs-resolucao %s", j->resolucao);
		if (system(cmd) != 0)
			fprintf(stderr, "ccs-menu: nao mudou a resolucao para %s\n", j->resolucao);
	}

	escrever_status("jogando", j);
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		execl("/bin/sh", "sh", j->script, (char *)NULL);
		_exit(127);
	}
	if (pid > 0) {
		/* espera o jogo acabar. Para voltar ao menu sem teclado: segurar
		 * Select + Start por 2 s, ou "Encerrar jogo" no painel web. */
		Uint32 atalho_desde = 0;
		for (int volta = 0;; volta++) {
			int st;
			pid_t r = waitpid(pid, &st, WNOHANG);
			if (r == pid || (r < 0 && errno != EINTR))
				break;
			SDL_Event e;
			while (SDL_PollEvent(&e))
				if (e.type == SDL_JOYDEVICEADDED)
					abrir_controle(e.jdevice.which);
			if (atalho_sair_apertado()) {
				if (!atalho_desde)
					atalho_desde = SDL_GetTicks();
				else if (SDL_GetTicks() - atalho_desde > 2000) {
					encerrar_jogo(pid);
					atalho_desde = 0;
				}
			} else {
				atalho_desde = 0;
			}
			if (volta % 5 == 0 && ler_pedido(buf, sizeof buf) && strcmp(buf, "parar") == 0)
				encerrar_jogo(pid);
			usleep(50000);
		}
	}
	escrever_status("menu", NULL);
	if (j->resolucao[0] && system("/usr/bin/ccs-resolucao") != 0)
		fprintf(stderr, "ccs-menu: nao voltou a resolucao\n");

	SDL_InitSubSystem(SDL_INIT_VIDEO);
	if (abrir_janela() != 0) {
		fprintf(stderr, "ccs-menu: nao reabriu a janela: %s\n", SDL_GetError());
		exit(1);
	}
}

static void escolher(int n)
{
	if (n < 0 || n >= njogos || n == sel)
		return;
	sel_ant = sel;
	sel = n;
	troca_ms = SDL_GetTicks();
}

/* devolve 1 para sair do menu */
static int acao(int k)
{
	switch (k) {
	case SDLK_LEFT: if (!confirmar_saida) escolher(sel - 1); break;
	case SDLK_RIGHT: if (!confirmar_saida) escolher(sel + 1); break;
	case SDLK_HOME: if (!confirmar_saida) escolher(0); break;
	case SDLK_END: if (!confirmar_saida) escolher(njogos - 1); break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (confirmar_saida)
			return 1;
		ver_controles = 0;
		if (njogos)
			jogar(sel);
		break;
	case SDLK_c:
		if (!confirmar_saida && njogos)
			ver_controles = !ver_controles;
		break;
	case SDLK_ESCAPE:
		if (ver_controles)
			ver_controles = 0;
		else
			confirmar_saida = !confirmar_saida;
		break;
	}
	return 0;
}

static void iniciar_particulas(void)
{
	for (int i = 0; i < N_PARTICULAS; i++) {
		part[i].x = (float)rand() / RAND_MAX;
		part[i].y = (float)rand() / RAND_MAX;
		part[i].v = 0.012f + 0.03f * rand() / RAND_MAX;
		part[i].r = 6 + 18.0f * rand() / RAND_MAX;
		part[i].a = 30 + 70.0f * rand() / RAND_MAX;
		part[i].fase = 6.28f * rand() / RAND_MAX;
	}
}

static void marcar_saida(void)
{
	char arq[256];
	snprintf(arq, sizeof arq, "%s/sair", dir_run);
	FILE *f = fopen(arq, "w");
	if (f)
		fclose(f);
}

int main(int argc, char **argv)
{
	const char *conf = getenv("CCS_JOGOS_CONF");
	const char *shot = getenv("CCS_SCREENSHOT"); /* testes: salva um BMP e sai */
	char buf[96];

	if (getenv("CCS_RUN_DIR"))
		dir_run = getenv("CCS_RUN_DIR");
	if (getenv("CCS_ARTE_DIR"))
		dir_arte = getenv("CCS_ARTE_DIR");
	mkdir(dir_run, 0755);
	srand((unsigned)time(NULL));
	carregar_jogos(conf ? conf : CONF_PADRAO);
	carregar_controles(getenv("CCS_CONTROLES_CONF") ? getenv("CCS_CONTROLES_CONF") : CONTROLES_PADRAO);
	if (argc > 1 && njogos)
		sel = sel_ant = atoi(argv[1]) % njogos;
	pos = (float)sel;
	iniciar_particulas();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0 || TTF_Init() != 0) {
		fprintf(stderr, "ccs-menu: SDL: %s\n", SDL_GetError());
		return 1;
	}
	IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
	if (abrir_janela() != 0) {
		fprintf(stderr, "ccs-menu: janela: %s\n", SDL_GetError());
		return 1;
	}
	escrever_status("menu", NULL);

	if (shot) {
		troca_ms = SDL_GetTicks() - 1000;
		if (getenv("CCS_CONFIRMAR"))
			confirmar_saida = 1;
		if (getenv("CCS_VER_CONTROLES"))
			ver_controles = 1;
		if (getenv("CCS_CARREGANDO")) { /* testes: tela "Abrindo X" */
			tela_carregando(&jogos[sel]);
			SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
			SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
			SDL_SaveBMP(s, shot);
			return 0;
		}
		if (getenv("CCS_AVISO"))
			mostrar_aviso(getenv("CCS_AVISO"));
		for (int q = 0; q < 3; q++)
			desenhar(0.016f);
		SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
		SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
		SDL_SaveBMP(s, shot);
		return 0;
	}

	Uint32 ultimo_pedido = 0, antes = SDL_GetTicks();
	Uint32 repete_ms = 0;
	int repete_dir = 0;
	for (;;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			int k = 0;
			if (e.type == SDL_QUIT)
				goto fim;
			if (e.type == SDL_KEYDOWN)
				k = e.key.keysym.sym;
			if (e.type == SDL_JOYDEVICEADDED)
				abrir_controle(e.jdevice.which);
			if (e.type == SDL_JOYDEVICEREMOVED)
				for (int c = 0; c < MAX_CONTROLES; c++) {
					if (ctl[c] && !SDL_GameControllerGetAttached(ctl[c])) {
						SDL_GameControllerClose(ctl[c]);
						ctl[c] = NULL;
					}
					if (joy[c] && !SDL_JoystickGetAttached(joy[c])) {
						SDL_JoystickClose(joy[c]);
						joy[c] = NULL;
					}
				}
			/* controle generico (sem mapeamento no SDL) */
			if (e.type == SDL_JOYHATMOTION && !eh_controle_mapeado(e.jhat.which)) {
				if (e.jhat.value & SDL_HAT_LEFT)
					k = SDLK_LEFT;
				else if (e.jhat.value & SDL_HAT_RIGHT)
					k = SDLK_RIGHT;
			}
			if (e.type == SDL_JOYBUTTONDOWN && !eh_controle_mapeado(e.jbutton.which)) {
				if (e.jbutton.button <= 3 || e.jbutton.button == 9)
					k = SDLK_RETURN;
				else if (e.jbutton.button == 8)
					k = SDLK_ESCAPE;
			}
			if (e.type == SDL_JOYAXISMOTION && e.jaxis.axis == 0 && !eh_controle_mapeado(e.jaxis.which)) {
				int dir = e.jaxis.value < -16000 ? -1 : (e.jaxis.value > 16000 ? 1 : 0);
				if (dir != repete_dir) {
					repete_dir = dir;
					repete_ms = SDL_GetTicks() + 380;
					if (dir)
						k = dir < 0 ? SDLK_LEFT : SDLK_RIGHT;
				}
			}
			if (e.type == SDL_CONTROLLERBUTTONDOWN) {
				switch (e.cbutton.button) {
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT: k = SDLK_LEFT; break;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: k = SDLK_RIGHT; break;
				case SDL_CONTROLLER_BUTTON_A:
				case SDL_CONTROLLER_BUTTON_START: k = SDLK_RETURN; break;
				case SDL_CONTROLLER_BUTTON_BACK: k = SDLK_ESCAPE; break;
				case SDL_CONTROLLER_BUTTON_B: if (confirmar_saida || ver_controles) k = SDLK_ESCAPE; break;
				case SDL_CONTROLLER_BUTTON_Y: k = SDLK_c; break;
				}
			}
			if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
				int dir = e.caxis.value < -16000 ? -1 : (e.caxis.value > 16000 ? 1 : 0);
				if (dir != repete_dir) { /* analogico: move uma vez e repete se segurar */
					repete_dir = dir;
					repete_ms = SDL_GetTicks() + 380;
					if (dir)
						k = dir < 0 ? SDLK_LEFT : SDLK_RIGHT;
				}
			}
			if (k && acao(k)) {
				marcar_saida();
				goto fim;
			}
		}
		if (repete_dir && SDL_GetTicks() > repete_ms) {
			acao(repete_dir < 0 ? SDLK_LEFT : SDLK_RIGHT);
			repete_ms = SDL_GetTicks() + 160;
		}

		/* pedidos do painel web, 4x por segundo */
		if (SDL_GetTicks() - ultimo_pedido > 250) {
			ultimo_pedido = SDL_GetTicks();
			if (ler_pedido(buf, sizeof buf) && strncmp(buf, "jogar ", 6) == 0) {
				int i = achar_jogo(buf + 6);
				if (i >= 0) {
					escolher(i);
					pos = (float)sel;
					confirmar_saida = 0;
					jogar(i);
					antes = SDL_GetTicks();
				}
			}
		}
		Uint32 agora = SDL_GetTicks();
		float dt = (agora - antes) / 1000.0f;
		antes = agora;
		if (dt > 0.1f)
			dt = 0.1f;
		desenhar(dt);
		SDL_Delay(4); /* o vsync limita a ~60 quadros; isto so alivia a CPU */
	}
fim:
	escrever_status("terminal", NULL);
	fechar_janela();
	IMG_Quit();
	TTF_Quit();
	SDL_Quit();
	return 0;
}
