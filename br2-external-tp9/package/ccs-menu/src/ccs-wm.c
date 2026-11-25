/*
 * ccs-wm - gerenciador de janelas minimo do CCS.
 *
 * Sem gerenciador de janelas, um jogo que pede tela cheia (SDL2 usa
 * _NET_WM_STATE_FULLSCREEN) fica numa janela do tamanho antigo e desenha
 * como se tivesse a tela toda: aparece so um pedaco da imagem. Este WM
 * resolve isso do jeito de um console: toda janela de aplicativo ocupa a
 * tela inteira, sem borda, e recebe o foco do teclado. Quando a resolucao
 * muda (xrandr, ver ccs-resolucao), as janelas acompanham.
 *
 * Atalho de teclado: segurar Home por 1 s pede ao menu para encerrar o jogo
 * (o mesmo pedido "parar" que o painel web manda), para quem joga sem controle.
 *
 * Excecoes, centralizadas no tamanho que pedirem:
 *  - janelas "transient" (dialogos);
 *  - janelas de tamanho fixo (min == max) que nao pediram tela cheia, como
 *    emuladores em 640x480. Para essas o menu troca a resolucao da TV antes
 *    de abrir o jogo (campo "resolucao" do jogos.conf), e ai a janela
 *    centralizada ja ocupa a tela toda, ampliada pelo proprio monitor.
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIENTES 64
#define PEDIDO "/run/ccs/pedido"

static Display *d;
static Window raiz, marca;
static int largura_tela, altura_tela, evento_randr;
static Window clientes[MAX_CLIENTES];
static char tela_cheia[MAX_CLIENTES]; /* a janela pediu _NET_WM_STATE_FULLSCREEN */
static int nclientes;

enum { A_SUPORTADO, A_ESTADO, A_TELA_CHEIA, A_CHECK, A_NOME, A_UTF8, A_ATIVA, A_LISTA, N_ATOMOS };
static Atom at[N_ATOMOS];
static const char *nomes_atomos[N_ATOMOS] = {
	"_NET_SUPPORTED", "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN",
	"_NET_SUPPORTING_WM_CHECK", "_NET_WM_NAME", "UTF8_STRING",
	"_NET_ACTIVE_WINDOW", "_NET_CLIENT_LIST",
};

static int ignorar_erro(Display *dpy, XErrorEvent *e)
{
	(void)dpy;
	(void)e; /* janela que sumiu no meio do caminho: normal */
	return 0;
}

static int outro_wm;
static int erro_outro_wm(Display *dpy, XErrorEvent *e)
{
	(void)dpy;
	if (e->error_code == BadAccess)
		outro_wm = 1;
	return 0;
}

static void ler_tamanho_tela(void)
{
	largura_tela = DisplayWidth(d, DefaultScreen(d));
	altura_tela = DisplayHeight(d, DefaultScreen(d));
}

static int achar(Window w)
{
	for (int i = 0; i < nclientes; i++)
		if (clientes[i] == w)
			return i;
	return -1;
}

static void publicar_lista(void)
{
	XChangeProperty(d, raiz, at[A_LISTA], XA_WINDOW, 32, PropModeReplace,
			(unsigned char *)clientes, nclientes);
}

/* WM_TRANSIENT_FOR lido direto: XGetTransientForHint devolve "sucesso"
 * mesmo quando a propriedade nao existe */
static int eh_dialogo(Window w)
{
	Atom tipo;
	int fmt;
	unsigned long n, resto;
	unsigned char *v = NULL;
	int r = 0;
	if (XGetWindowProperty(d, w, XA_WM_TRANSIENT_FOR, 0, 1, False, XA_WINDOW, &tipo, &fmt,
			       &n, &resto, &v) == Success && tipo == XA_WINDOW && n == 1 && v)
		r = *(Window *)v != None && *(Window *)v != raiz;
	if (v)
		XFree(v);
	return r;
}

static int tamanho_fixo(Window w)
{
	XSizeHints h;
	long sup;
	if (!XGetWMNormalHints(d, w, &h, &sup))
		return 0;
	return (h.flags & PMinSize) && (h.flags & PMaxSize) && h.min_width == h.max_width &&
	       h.min_height == h.max_height && h.min_width > 0;
}

static int pediu_tela_cheia(Window w)
{
	Atom tipo;
	int fmt;
	unsigned long n, resto;
	unsigned char *v = NULL;
	int r = 0;
	if (XGetWindowProperty(d, w, at[A_ESTADO], 0, 16, False, XA_ATOM, &tipo, &fmt, &n, &resto,
			       &v) == Success && v) {
		for (unsigned long i = 0; i < n; i++)
			if (((Atom *)v)[i] == at[A_TELA_CHEIA])
				r = 1;
	}
	if (v)
		XFree(v);
	return r;
}

static void centralizar(Window w)
{
	XWindowAttributes a;
	if (XGetWindowAttributes(d, w, &a)) {
		int x = (largura_tela - a.width) / 2, y = (altura_tela - a.height) / 2;
		XSetWindowBorderWidth(d, w, 0);
		XMoveWindow(d, w, x < 0 ? 0 : x, y < 0 ? 0 : y);
	}
}

static void ajustar(Window w)
{
	int i = achar(w);
	if (eh_dialogo(w) || (tamanho_fixo(w) && !(i >= 0 && tela_cheia[i]))) {
		XDeleteProperty(d, w, at[A_ESTADO]);
		centralizar(w);
		return;
	}
	XSetWindowBorderWidth(d, w, 0);
	XMoveResizeWindow(d, w, 0, 0, largura_tela, altura_tela);
	XChangeProperty(d, w, at[A_ESTADO], XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&at[A_TELA_CHEIA], 1);
}

static void focar(Window w)
{
	if (w == None) {
		XSetInputFocus(d, PointerRoot, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(d, raiz, at[A_ATIVA]);
		return;
	}
	XRaiseWindow(d, w);
	XSetInputFocus(d, w, RevertToPointerRoot, CurrentTime);
	XChangeProperty(d, raiz, at[A_ATIVA], XA_WINDOW, 32, PropModeReplace,
			(unsigned char *)&w, 1);
}

static void esquecer(Window w)
{
	int i = achar(w);
	if (i < 0)
		return;
	memmove(&clientes[i], &clientes[i + 1], (nclientes - i - 1) * sizeof(Window));
	memmove(&tela_cheia[i], &tela_cheia[i + 1], nclientes - i - 1);
	nclientes--;
	publicar_lista();
	focar(nclientes ? clientes[nclientes - 1] : None);
}

static void ao_mapear(XMapRequestEvent *e)
{
	if (achar(e->window) < 0 && nclientes < MAX_CLIENTES) {
		tela_cheia[nclientes] = pediu_tela_cheia(e->window);
		clientes[nclientes++] = e->window;
		XSelectInput(d, e->window, StructureNotifyMask);
		publicar_lista();
	}
	ajustar(e->window);
	XMapRaised(d, e->window);
	focar(e->window);
}

static void ao_configurar(XConfigureRequestEvent *e)
{
	if (achar(e->window) < 0) {
		/* ainda nao mapeada: deixa a aplicacao arrumar como quiser */
		XWindowChanges c = {
			.x = e->x, .y = e->y, .width = e->width, .height = e->height,
			.border_width = e->border_width, .sibling = e->above, .stack_mode = e->detail,
		};
		XConfigureWindow(d, e->window, e->value_mask, &c);
		return;
	}
	int i = achar(e->window);
	if (eh_dialogo(e->window) || (tamanho_fixo(e->window) && !tela_cheia[i])) {
		XWindowChanges c = { .width = e->width, .height = e->height };
		XConfigureWindow(d, e->window, e->value_mask & (CWWidth | CWHeight), &c);
		ajustar(e->window);
		return;
	}
	/* janela de jogo: continua com a tela inteira, e avisa o tamanho real
	 * (ICCCM: evento sintetico quando o pedido nao e atendido) */
	ajustar(e->window);
	XConfigureEvent ce = {
		.type = ConfigureNotify, .display = d, .event = e->window, .window = e->window,
		.x = 0, .y = 0, .width = largura_tela, .height = altura_tela,
		.border_width = 0, .above = None, .override_redirect = False,
	};
	XSendEvent(d, e->window, False, StructureNotifyMask, (XEvent *)&ce);
}

static void ao_mensagem(XClientMessageEvent *e)
{
	int i = achar(e->window);
	if (e->message_type == at[A_ESTADO] && i >= 0) {
		/* l[0]: 0 tira, 1 poe, 2 alterna; l[1] e l[2]: estados */
		if ((Atom)e->data.l[1] == at[A_TELA_CHEIA] || (Atom)e->data.l[2] == at[A_TELA_CHEIA]) {
			long acao = e->data.l[0];
			tela_cheia[i] = acao == 1 ? 1 : acao == 0 ? 0 : !tela_cheia[i];
		}
		ajustar(e->window);
	}
	else if (e->message_type == at[A_ATIVA] && i >= 0)
		focar(e->window);
}

static void ao_mudar_resolucao(XEvent *e)
{
	XRRUpdateConfiguration(e);
	ler_tamanho_tela();
	for (int i = 0; i < nclientes; i++)
		ajustar(clientes[i]);
}

static void anunciar(void)
{
	/* _NET_SUPPORTING_WM_CHECK: e assim que o SDL descobre que ha um WM */
	marca = XCreateSimpleWindow(d, raiz, -10, -10, 1, 1, 0, 0, 0);
	XChangeProperty(d, raiz, at[A_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&marca, 1);
	XChangeProperty(d, marca, at[A_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&marca, 1);
	XChangeProperty(d, marca, at[A_NOME], at[A_UTF8], 8, PropModeReplace, (unsigned char *)"ccs-wm", 6);
	Atom sup[] = { at[A_ESTADO], at[A_TELA_CHEIA], at[A_CHECK], at[A_ATIVA], at[A_LISTA] };
	XChangeProperty(d, raiz, at[A_SUPORTADO], XA_ATOM, 32, PropModeReplace,
			(unsigned char *)sup, sizeof sup / sizeof sup[0]);
}

static KeyCode tecla_sair;
static Time sair_desde;

static void prender_tecla_sair(void)
{
	tecla_sair = XKeysymToKeycode(d, XK_Home);
	if (!tecla_sair)
		return;
	/* qualquer combinacao de NumLock/CapsLock */
	unsigned int mods[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
	for (unsigned i = 0; i < sizeof mods / sizeof mods[0]; i++)
		XGrabKey(d, tecla_sair, mods[i], raiz, True, GrabModeAsync, GrabModeAsync);
	Bool ok;
	XkbSetDetectableAutoRepeat(d, True, &ok); /* segurar nao gera soltar/apertar */
}

static void pedir_parar(void)
{
	FILE *f = fopen(PEDIDO, "w");
	if (f) {
		fputs("parar\n", f);
		fclose(f);
	}
}

int main(void)
{
	signal(SIGCHLD, SIG_IGN);
	d = XOpenDisplay(NULL);
	if (!d) {
		fprintf(stderr, "ccs-wm: sem DISPLAY\n");
		return 1;
	}
	raiz = DefaultRootWindow(d);
	XInternAtoms(d, (char **)nomes_atomos, N_ATOMOS, False, at);

	XSetErrorHandler(erro_outro_wm);
	XSelectInput(d, raiz, SubstructureRedirectMask | SubstructureNotifyMask);
	XSync(d, False);
	if (outro_wm) {
		fprintf(stderr, "ccs-wm: ja existe outro gerenciador de janelas\n");
		return 1;
	}
	XSetErrorHandler(ignorar_erro);

	int erro_base;
	if (XRRQueryExtension(d, &evento_randr, &erro_base))
		XRRSelectInput(d, raiz, RRScreenChangeNotifyMask);
	else
		evento_randr = -1;
	ler_tamanho_tela();
	anunciar();
	prender_tecla_sair();
	XSync(d, False);

	for (;;) {
		XEvent e;
		XNextEvent(d, &e);
		switch (e.type) {
		case MapRequest: ao_mapear(&e.xmaprequest); break;
		case ConfigureRequest: ao_configurar(&e.xconfigurerequest); break;
		case ClientMessage: ao_mensagem(&e.xclient); break;
		case UnmapNotify:
			if (e.xunmap.event != raiz)
				esquecer(e.xunmap.window);
			break;
		case DestroyNotify: esquecer(e.xdestroywindow.window); break;
		case KeyPress:
			if (e.xkey.keycode == tecla_sair && !sair_desde)
				sair_desde = e.xkey.time ? e.xkey.time : 1;
			break;
		case KeyRelease:
			if (e.xkey.keycode == tecla_sair && sair_desde) {
				if (e.xkey.time - sair_desde >= 1000)
					pedir_parar();
				sair_desde = 0;
			}
			break;
		default:
			if (evento_randr >= 0 && e.type == evento_randr + RRScreenChangeNotify)
				ao_mudar_resolucao(&e);
		}
	}
}
