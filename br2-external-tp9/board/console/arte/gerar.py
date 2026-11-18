"""Renderiza a arte (SVG -> PNG) e as texturas de efeito do menu.

So roda na maquina de desenvolvimento (precisa de Playwright e Pillow); os
PNG gerados ficam no repositorio, entao o build do Buildroot nao precisa
disso. Uso: python3 gerar.py
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageEnhance
from playwright.sync_api import sync_playwright

sys.path.insert(0, os.path.dirname(__file__))
import capas  # noqa: E402

AQUI = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(AQUI, "../../../.."))
DEST = os.path.join(AQUI, "../rootfs-overlay/usr/share/ccs")
WEB = os.path.join(REPO, "console-app/static/arte")
SVG = os.path.join(AQUI, "svg")
for d in (DEST + "/capas", DEST + "/fx", WEB, SVG):
    os.makedirs(d, exist_ok=True)


def render(pagina, svg_path, png_path, w, h):
    pagina.set_viewport_size({"width": w, "height": h})
    pagina.goto("file://" + svg_path)
    pagina.wait_for_timeout(150)
    pagina.screenshot(path=png_path, omit_background=True, clip={"x": 0, "y": 0, "width": w, "height": h})


with sync_playwright() as p:
    nav = p.chromium.launch()
    pg = nav.new_page()
    open(os.path.join(SVG, "emblema.svg"), "w").write(capas.emblema())
    open(os.path.join(SVG, "logo.svg"), "w").write(capas.logo_horizontal())
    render(pg, os.path.join(SVG, "emblema.svg"), os.path.join(DEST, "emblema.png"), 512, 512)
    render(pg, os.path.join(SVG, "logo.svg"), os.path.join(DEST, "logo.png"), 1240, 300)
    for jid, fn in capas.CAPAS.items():
        s = os.path.join(SVG, f"{jid}.svg")
        open(s, "w").write(fn())
        render(pg, s, os.path.join(DEST, "capas", f"{jid}.png"), capas.W, capas.H)
    nav.close()

# capas fornecidas pelo grupo (capas-oficiais/<id>.jpg|png) substituem a arte
# gerada: viram 600x800 com cantos arredondados. Se a proporcao for muito
# diferente de 3:4, a imagem inteira entra centralizada sobre uma copia
# desfocada dela mesma (nada da capa e cortado).
OFICIAIS = os.path.join(AQUI, "capas-oficiais")


def preparar_capa(arq, w=capas.W, h=capas.H, raio=26):
    im = Image.open(arq).convert("RGB")
    alvo = w / h
    prop = im.width / im.height
    if abs(prop - alvo) / alvo <= 0.08:  # quase 3:4: corta as sobras
        if prop > alvo:
            nw = int(im.height * alvo)
            im = im.crop(((im.width - nw) // 2, 0, (im.width + nw) // 2, im.height))
        else:
            nh = int(im.width / alvo)
            im = im.crop((0, (im.height - nh) // 2, im.width, (im.height + nh) // 2))
        capa = im.resize((w, h), Image.LANCZOS)
    else:
        escala = max(w / im.width, h / im.height)
        fundo = im.resize((int(im.width * escala) + 1, int(im.height * escala) + 1), Image.LANCZOS)
        fundo = fundo.crop(((fundo.width - w) // 2, (fundo.height - h) // 2,
                            (fundo.width - w) // 2 + w, (fundo.height - h) // 2 + h))
        fundo = ImageEnhance.Brightness(fundo.filter(ImageFilter.GaussianBlur(24))).enhance(0.6)
        escala = min(w / im.width, h / im.height)
        frente = im.resize((int(im.width * escala), int(im.height * escala)), Image.LANCZOS)
        fundo.paste(frente, ((w - frente.width) // 2, (h - frente.height) // 2))
        capa = fundo
    mascara = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mascara).rounded_rectangle((0, 0, w - 1, h - 1), raio, fill=255)
    saida = capa.convert("RGBA")
    saida.putalpha(mascara)
    contorno = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(contorno).rounded_rectangle((1, 1, w - 2, h - 2), raio - 1, outline=(255, 255, 255, 36), width=2)
    saida.alpha_composite(contorno)
    return saida


for jid in capas.CAPAS:
    for ext in ("png", "jpg", "jpeg", "webp"):
        arq = os.path.join(OFICIAIS, f"{jid}.{ext}")
        if os.path.exists(arq):
            preparar_capa(arq).save(os.path.join(DEST, "capas", f"{jid}.png"))
            print("capa do grupo:", jid, "<-", os.path.basename(arq))
            break

# fundo de cada jogo: a capa ampliada, desfocada e escurecida (atras do menu)
for jid in capas.CAPAS:
    capa = Image.open(os.path.join(DEST, "capas", f"{jid}.png")).convert("RGB")
    f = capa.crop((0, 80, 600, 418)).resize((640, 360), Image.LANCZOS)
    f = f.filter(ImageFilter.GaussianBlur(30))
    f = ImageEnhance.Brightness(f).enhance(0.42)
    f.save(os.path.join(DEST, "capas", f"{jid}-fundo.jpg"), quality=88)

# texturas de efeito (brancas: o menu tinge com a cor do jogo)
def retangulo_suave(w, h, r, borda, blur, nome, preenchido=False):
    m = blur * 3
    im = Image.new("L", (w + 2 * m, h + 2 * m), 0)
    d = ImageDraw.Draw(im)
    if preenchido:
        d.rounded_rectangle((m, m, m + w, m + h), r, fill=255)
    else:
        d.rounded_rectangle((m, m, m + w, m + h), r, outline=255, width=borda)
    im = im.filter(ImageFilter.GaussianBlur(blur))
    rgba = Image.new("RGBA", im.size, (255, 255, 255, 0))
    rgba.putalpha(im)
    rgba.save(os.path.join(DEST, "fx", nome))

retangulo_suave(300, 400, 22, 22, 20, "brilho.png")            # aura do jogo escolhido
retangulo_suave(300, 400, 22, 0, 26, "sombra.png", True)       # sombra dos cards
painel = Image.new("RGBA", (96, 96), (255, 255, 255, 0))        # 9-slice: paineis e botoes
ImageDraw.Draw(painel).rounded_rectangle((0, 0, 95, 95), 28, fill=(255, 255, 255, 255))
painel.save(os.path.join(DEST, "fx", "painel.png"))
borda = Image.new("RGBA", (96, 96), (255, 255, 255, 0))
ImageDraw.Draw(borda).rounded_rectangle((0, 0, 95, 95), 28, outline=(255, 255, 255, 255), width=4)
borda.save(os.path.join(DEST, "fx", "borda.png"))
ponto = Image.new("L", (64, 64), 0)
ImageDraw.Draw(ponto).ellipse((16, 16, 48, 48), fill=255)
ponto = ponto.filter(ImageFilter.GaussianBlur(7))
pt = Image.new("RGBA", (64, 64), (255, 255, 255, 0)); pt.putalpha(ponto)
pt.save(os.path.join(DEST, "fx", "particula.png"))

# copias para o painel web
import shutil  # noqa: E402
for jid in capas.CAPAS:
    Image.open(os.path.join(DEST, "capas", f"{jid}.png")).resize((300, 400), Image.LANCZOS).save(os.path.join(WEB, f"{jid}.png"))
shutil.copy(os.path.join(DEST, "logo.png"), os.path.join(WEB, "logo.png"))
Image.open(os.path.join(DEST, "emblema.png")).resize((64, 64), Image.LANCZOS).save(os.path.join(WEB, "favicon.png"))
print("arte gerada em", DEST)
