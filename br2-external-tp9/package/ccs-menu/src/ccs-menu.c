/*
 * ccs-menu - menu grafico do CCS (Console Contra Sony).
 *
 * Roda dentro do X (iniciado pelo S95ccs-menu com xinit), mostra os jogos
 * de /etc/ccs/jogos.conf e abre o escolhido com o start.sh dele.
 * Controles: setas / D-pad escolhem, Enter / botao A jogam, Esc / botao B
 * pergunta se quer sair para o terminal.
 *
 * Integracao com o painel web (container Docker):
 *   /run/ccs/status  <- o menu escreve o estado atual em JSON
 *   /run/ccs/pedido  -> o painel escreve "jogar <id>" ou "parar"
 *
 * Ao sair pelo menu ("Sair para o terminal?"), cria /run/ccs/sair: o
 * S95ccs-menu ve o arquivo e nao reabre o menu. Se o menu cair por erro,
 * o S95 abre de novo.
 */
#define _GNU_SOURCE
#include <SDL.h>
#include <SDL_ttf.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
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
#define DIR_RUN "/run/ccs"

typedef struct {
	char id[32];
	char nome[64];
	char motor[64];
	char script[256];
	char sigla[8];
	SDL_Color cor;
} Jogo;

static Jogo jogos[MAX_JOGOS];
static int njogos;
static int sel;
static int confirmar_saida;
static char aviso[160];
static Uint32 aviso_ate;

static SDL_Window *win;
static SDL_Renderer *ren;
static TTF_Font *f_titulo, *f_grande, *f_medio, *f_pequeno, *f_sigla;
static int W, H;
static const char *dir_run = DIR_RUN;

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

/* id|Nome|Motor|#cor|SIGLA|/caminho/start.sh */
static void carregar_jogos(const char *arq)
{
	FILE *f = fopen(arq, "r");
	char linha[512];
	if (!f) {
		fprintf(stderr, "ccs-menu: nao abriu %s: %s\n", arq, strerror(errno));
		return;
	}
	while (njogos < MAX_JOGOS && fgets(linha, sizeof linha, f)) {
		char *p = trim(linha), *campo[6] = { 0 };
		int n = 0;
		if (*p == '#' || *p == 0)
			continue;
		for (char *t = strtok(p, "|"); t && n < 6; t = strtok(NULL, "|"))
			campo[n++] = trim(t);
		if (n < 6)
			continue;
		Jogo *j = &jogos[njogos];
		snprintf(j->id, sizeof j->id, "%s", campo[0]);
		snprintf(j->nome, sizeof j->nome, "%s", campo[1]);
		snprintf(j->motor, sizeof j->motor, "%s", campo[2]);
		j->cor = hex(campo[3]);
		snprintf(j->sigla, sizeof j->sigla, "%s", campo[4]);
		snprintf(j->script, sizeof j->script, "%s", campo[5]);
		njogos++;
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

static void enderecos_ip(char *out, size_t n)
{
	struct ifaddrs *ifa, *p;
	out[0] = 0;
	if (getifaddrs(&ifa) != 0)
		return;
	for (p = ifa; p; p = p->ifa_next) {
		if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET)
			continue;
		if (p->ifa_flags & IFF_LOOPBACK)
			continue;
		if (strncmp(p->ifa_name, "docker", 6) == 0 || strncmp(p->ifa_name, "veth", 4) == 0)
			continue;
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr, ip, sizeof ip);
		size_t len = strlen(out);
		snprintf(out + len, n - len, "%shttp://%s", len ? "   " : "", ip);
	}
	freeifaddrs(ifa);
}

/* ---------- desenho ---------- */

static void cor(SDL_Color c, Uint8 a)
{
	SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a);
}

static void retangulo(int x, int y, int w, int h, SDL_Color c, Uint8 a)
{
	SDL_Rect r = { x, y, w, h };
	cor(c, a);
	SDL_RenderFillRect(ren, &r);
}

static void moldura(int x, int y, int w, int h, int esp, SDL_Color c)
{
	for (int i = 0; i < esp; i++) {
		SDL_Rect r = { x - i, y - i, w + 2 * i, h + 2 * i };
		cor(c, 255);
		SDL_RenderDrawRect(ren, &r);
	}
}

/* gradiente vertical de cima (a) para baixo (b) */
static void gradiente(int x, int y, int w, int h, SDL_Color a, SDL_Color b)
{
	SDL_Vertex v[4] = {
		{ { x, y }, a, { 0, 0 } },
		{ { x + w, y }, a, { 0, 0 } },
		{ { x + w, y + h }, b, { 0, 0 } },
		{ { x, y + h }, b, { 0, 0 } },
	};
	int idx[6] = { 0, 1, 2, 0, 2, 3 };
	SDL_RenderGeometry(ren, NULL, v, 4, idx, 6);
}

/* alinhamento: 0 = esquerda, 1 = centro, 2 = direita */
static int texto(TTF_Font *f, const char *s, int x, int y, SDL_Color c, int alinhar)
{
	if (!f || !s || !*s)
		return 0;
	SDL_Surface *sf = TTF_RenderUTF8_Blended(f, s, c);
	if (!sf)
		return 0;
	SDL_Texture *tx = SDL_CreateTextureFromSurface(ren, sf);
	SDL_Rect r = { x, y, sf->w, sf->h };
	if (alinhar == 1)
		r.x -= sf->w / 2;
	else if (alinhar == 2)
		r.x -= sf->w;
	SDL_RenderCopy(ren, tx, NULL, &r);
	int w = sf->w;
	SDL_DestroyTexture(tx);
	SDL_FreeSurface(sf);
	return w;
}

static void desenhar_card(int i, int x, int y, int w, int h)
{
	const Jogo *j = &jogos[i];
	SDL_Color branco = { 240, 243, 250, 255 }, cinza = { 150, 160, 185, 255 };
	SDL_Color fundo = { 24, 28, 44, 255 };
	int ativo = (i == sel);

	if (ativo) {
		x -= w / 40;
		y -= h / 40;
		w += w / 20;
		h += h / 20;
		retangulo(x + 8, y + 10, w, h, (SDL_Color){ 0, 0, 0, 255 }, 120);
	}
	retangulo(x, y, w, h, fundo, 255);

	/* faixa colorida com a sigla do jogo */
	int fh = h * 58 / 100;
	SDL_Color c2 = { j->cor.r / 3, j->cor.g / 3, j->cor.b / 3, 255 };
	gradiente(x, y, w, fh, j->cor, c2);
	texto(f_sigla, j->sigla, x + w / 2, y + fh / 2 - TTF_FontHeight(f_sigla) / 2, branco, 1);

	texto(f_medio, j->nome, x + w / 20, y + fh + h / 14, branco, 0);
	texto(f_pequeno, j->motor, x + w / 20, y + fh + h / 14 + TTF_FontHeight(f_medio) + 4, cinza, 0);

	if (ativo)
		moldura(x, y, w, h, 4, j->cor);
	else
		moldura(x, y, w, h, 1, (SDL_Color){ 52, 58, 84, 255 });
}

static void desenhar(void)
{
	SDL_Color branco = { 240, 243, 250, 255 }, cinza = { 150, 160, 185, 255 };
	SDL_Color azul = { 74, 125, 255, 255 };

	SDL_GetRendererOutputSize(ren, &W, &H);
	gradiente(0, 0, W, H, (SDL_Color){ 16, 19, 34, 255 }, (SDL_Color){ 6, 7, 14, 255 });

	int m = W / 24;
	/* cabecalho */
	int wl = texto(f_titulo, "CCS", m, H / 22, azul, 0);
	texto(f_grande, "Console Contra Sony", m + wl + W / 60,
	      H / 22 + TTF_FontHeight(f_titulo) - TTF_FontHeight(f_grande) - H / 120, branco, 0);
	char hora[16];
	time_t t = time(NULL);
	strftime(hora, sizeof hora, "%H:%M", localtime(&t));
	texto(f_grande, hora, W - m, H / 22 + TTF_FontHeight(f_titulo) - TTF_FontHeight(f_grande) - H / 120,
	      branco, 2);
	retangulo(m, H / 22 + TTF_FontHeight(f_titulo) + H / 60, W - 2 * m, 2, (SDL_Color){ 52, 58, 84, 255 }, 255);

	/* grade de jogos: 3 colunas */
	int cols = 3, rows = (njogos + cols - 1) / cols;
	int top = H / 22 + TTF_FontHeight(f_titulo) + H / 20;
	int bottom = H - H / 7;
	int gap = W / 40;
	int cw = (W - 2 * m - (cols - 1) * gap) / cols;
	int ch = rows ? (bottom - top - (rows - 1) * gap) / rows : 0;
	if (ch > cw * 3 / 4)
		ch = cw * 3 / 4;
	int gy = top + ((bottom - top) - (rows * ch + (rows - 1) * gap)) / 2;

	if (njogos == 0)
		texto(f_grande, "Nenhum jogo em /etc/ccs/jogos.conf", W / 2, H / 2, cinza, 1);
	for (int pass = 0; pass < 2; pass++) /* o selecionado por cima */
		for (int i = 0; i < njogos; i++) {
			if ((i == sel) != pass)
				continue;
			int c = i % cols, r = i / cols;
			desenhar_card(i, m + c * (cw + gap), gy + r * (ch + gap), cw, ch);
		}

	/* rodape: controles e painel web */
	int fy = H - H / 10;
	texto(f_pequeno, "Setas / D-pad: escolher      Enter / A: jogar      Esc / Select: terminal", m, fy, cinza, 0);
	char ips[256], linha[300];
	enderecos_ip(ips, sizeof ips);
	snprintf(linha, sizeof linha, "Painel web: %s", ips[0] ? ips : "sem rede");
	texto(f_pequeno, linha, m, fy + TTF_FontHeight(f_pequeno) + 6, azul, 0);

	if (aviso[0] && SDL_GetTicks() < aviso_ate)
		texto(f_medio, aviso, W - m, fy, (SDL_Color){ 255, 196, 64, 255 }, 2);

	if (confirmar_saida) {
		retangulo(0, 0, W, H, (SDL_Color){ 0, 0, 0, 255 }, 170);
		int bw = W / 2, bh = H / 4;
		retangulo((W - bw) / 2, (H - bh) / 2, bw, bh, (SDL_Color){ 24, 28, 44, 255 }, 255);
		moldura((W - bw) / 2, (H - bh) / 2, bw, bh, 2, azul);
		texto(f_grande, "Sair para o terminal?", W / 2, (H - bh) / 2 + bh / 5, branco, 1);
		texto(f_pequeno, "Enter / A: sim        Esc / B: voltar ao menu", W / 2,
		      (H - bh) / 2 + bh * 3 / 5, cinza, 1);
	}
	SDL_RenderPresent(ren);
}

static void tela_carregando(const Jogo *j)
{
	SDL_Color branco = { 240, 243, 250, 255 };
	gradiente(0, 0, W, H, j->cor, (SDL_Color){ 6, 7, 14, 255 });
	texto(f_titulo, j->nome, W / 2, H / 2 - TTF_FontHeight(f_titulo), branco, 1);
	texto(f_medio, "Carregando...", W / 2, H / 2 + H / 30, branco, 1);
	SDL_RenderPresent(ren);
}

/* ---------- janela ---------- */

static TTF_Font *fonte(const char *arq, int px)
{
	TTF_Font *f = TTF_OpenFont(arq, px);
	if (!f)
		fprintf(stderr, "ccs-menu: fonte %s: %s\n", arq, TTF_GetError());
	return f;
}

static void fechar_fontes(void)
{
	TTF_Font **v[] = { &f_titulo, &f_grande, &f_medio, &f_pequeno, &f_sigla };
	for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++)
		if (*v[i]) {
			TTF_CloseFont(*v[i]);
			*v[i] = NULL;
		}
}

static int abrir_janela(void)
{
	const char *dir = getenv("CCS_FONT_DIR");
	char regular[256], negrito[256];
	if (!dir)
		dir = "/usr/share/fonts/dejavu";
	snprintf(regular, sizeof regular, "%s/DejaVuSans.ttf", dir);
	snprintf(negrito, sizeof negrito, "%s/DejaVuSans-Bold.ttf", dir);

	int w = 1280, h = 720;
	const char *geo = getenv("CCS_GEOMETRY"); /* para testes: 1280x720 */
	Uint32 flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
	if (geo && sscanf(geo, "%dx%d", &w, &h) == 2)
		flags = 0;
	win = SDL_CreateWindow("CCS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
	if (!win)
		return -1;
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!ren)
		ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
	if (!ren)
		return -1;
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_GetRendererOutputSize(ren, &W, &H);

	fechar_fontes();
	f_titulo = fonte(negrito, H / 11);
	f_grande = fonte(negrito, H / 24);
	f_medio = fonte(negrito, H / 34);
	f_pequeno = fonte(regular, H / 44);
	f_sigla = fonte(negrito, H / 7);
	return (f_titulo && f_grande && f_medio && f_pequeno && f_sigla) ? 0 : -1;
}

static void fechar_janela(void)
{
	fechar_fontes();
	if (ren)
		SDL_DestroyRenderer(ren);
	if (win)
		SDL_DestroyWindow(win);
	ren = NULL;
	win = NULL;
}

/* ---------- jogar ---------- */

static void jogar(int i)
{
	Jogo *j = &jogos[i];
	char buf[64];

	if (access(j->script, X_OK) != 0) {
		snprintf(aviso, sizeof aviso, "%s nao instalado", j->nome);
		aviso_ate = SDL_GetTicks() + 4000;
		return;
	}
	tela_carregando(j);
	SDL_Delay(400);

	/* libera a tela e o controle para o jogo */
	fechar_janela();
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

	escrever_status("jogando", j);
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		execl("/bin/sh", "sh", j->script, (char *)NULL);
		_exit(127);
	}
	if (pid > 0) {
		/* espera o jogo acabar; o painel web pode pedir para encerrar */
		for (;;) {
			int st;
			pid_t r = waitpid(pid, &st, WNOHANG);
			if (r == pid || (r < 0 && errno != EINTR))
				break;
			if (ler_pedido(buf, sizeof buf) && strcmp(buf, "parar") == 0) {
				kill(-pid, SIGTERM);
				for (int k = 0; k < 30 && waitpid(pid, &st, WNOHANG) == 0; k++)
					usleep(100000);
				kill(-pid, SIGKILL);
			}
			usleep(250000);
		}
	}
	escrever_status("menu", NULL);

	SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
	if (abrir_janela() != 0) {
		fprintf(stderr, "ccs-menu: nao reabriu a janela: %s\n", SDL_GetError());
		exit(1);
	}
}

static void mover(int dx, int dy)
{
	int cols = 3;
	int n = sel + dx + dy * cols;
	if (n >= 0 && n < njogos)
		sel = n;
}

/* devolve 1 para sair do menu */
static int acao(int tecla)
{
	switch (tecla) {
	case SDLK_LEFT: if (!confirmar_saida) mover(-1, 0); break;
	case SDLK_RIGHT: if (!confirmar_saida) mover(1, 0); break;
	case SDLK_UP: if (!confirmar_saida) mover(0, -1); break;
	case SDLK_DOWN: if (!confirmar_saida) mover(0, 1); break;
	case SDLK_RETURN:
		if (confirmar_saida)
			return 1;
		if (njogos)
			jogar(sel);
		break;
	case SDLK_ESCAPE:
		confirmar_saida = !confirmar_saida;
		break;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *conf = getenv("CCS_JOGOS_CONF");
	const char *shot = getenv("CCS_SCREENSHOT"); /* testes: salva um BMP e sai */
	char buf[96];

	if (getenv("CCS_RUN_DIR"))
		dir_run = getenv("CCS_RUN_DIR");
	mkdir(dir_run, 0755);
	carregar_jogos(conf ? conf : CONF_PADRAO);
	if (argc > 1)
		sel = atoi(argv[1]) % (njogos ? njogos : 1);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0 || TTF_Init() != 0) {
		fprintf(stderr, "ccs-menu: SDL: %s\n", SDL_GetError());
		return 1;
	}
	if (abrir_janela() != 0) {
		fprintf(stderr, "ccs-menu: janela: %s\n", SDL_GetError());
		return 1;
	}
	escrever_status("menu", NULL);

	if (shot) {
		if (getenv("CCS_CONFIRMAR"))
			confirmar_saida = 1;
		desenhar();
		SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
		SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
		SDL_SaveBMP(s, shot);
		return 0;
	}

	Uint32 ultimo_pedido = 0;
	for (;;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			int tecla = 0;
			if (e.type == SDL_QUIT)
				goto fim;
			if (e.type == SDL_KEYDOWN)
				tecla = e.key.keysym.sym;
			if (e.type == SDL_CONTROLLERDEVICEADDED)
				SDL_GameControllerOpen(e.cdevice.which);
			if (e.type == SDL_CONTROLLERBUTTONDOWN) {
				switch (e.cbutton.button) {
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT: tecla = SDLK_LEFT; break;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: tecla = SDLK_RIGHT; break;
				case SDL_CONTROLLER_BUTTON_DPAD_UP: tecla = SDLK_UP; break;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN: tecla = SDLK_DOWN; break;
				case SDL_CONTROLLER_BUTTON_A:
				case SDL_CONTROLLER_BUTTON_START: tecla = SDLK_RETURN; break;
				case SDL_CONTROLLER_BUTTON_B:
				case SDL_CONTROLLER_BUTTON_BACK: tecla = SDLK_ESCAPE; break;
				}
				if (tecla == SDLK_ESCAPE && !confirmar_saida && e.cbutton.button == SDL_CONTROLLER_BUTTON_B)
					tecla = 0; /* B so cancela; sair pelo controle e com Back */
			}
			if (tecla && acao(tecla)) {
				char arq[256];
				snprintf(arq, sizeof arq, "%s/sair", dir_run);
				FILE *f = fopen(arq, "w");
				if (f)
					fclose(f);
				goto fim;
			}
		}

		/* pedidos do painel web, 4x por segundo */
		if (SDL_GetTicks() - ultimo_pedido > 250) {
			ultimo_pedido = SDL_GetTicks();
			if (ler_pedido(buf, sizeof buf) && strncmp(buf, "jogar ", 6) == 0) {
				int i = achar_jogo(buf + 6);
				if (i >= 0) {
					sel = i;
					confirmar_saida = 0;
					jogar(i);
				}
			}
		}
		desenhar();
		SDL_Delay(30);
	}
fim:
	escrever_status("terminal", NULL);
	fechar_janela();
	TTF_Quit();
	SDL_Quit();
	return 0;
}
