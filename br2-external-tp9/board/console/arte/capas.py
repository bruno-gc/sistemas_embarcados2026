"""Gera as capas originais dos jogos e o logo do CCS em SVG.

Arte 100% original do grupo (formas vetoriais), no mesmo estilo para os 6
jogos. Nao usa capas, logos ou personagens oficiais.
"""
import math
import random

W, H = 600, 800


def base(cor, cena, titulo, genero, rodape, linhas_titulo=None):
    linhas = linhas_titulo or [titulo]
    tam = 64 if max(len(l) for l in linhas) <= 9 else (52 if max(len(l) for l in linhas) <= 12 else 44)
    y0 = H - 92 - (len(linhas) - 1) * (tam + 6)
    tit = "".join(
        f'<text x="44" y="{y0 + i * (tam + 6)}" font-family="Inter" font-weight="900" '
        f'font-size="{tam}" fill="#fff" letter-spacing="-1">{l}</text>' for i, l in enumerate(linhas))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
  <linearGradient id="escuro" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#000" stop-opacity="0"/>
    <stop offset="0.55" stop-color="#05060c" stop-opacity="0.35"/>
    <stop offset="1" stop-color="#05060c" stop-opacity="0.96"/>
  </linearGradient>
  <filter id="blur8"><feGaussianBlur stdDeviation="8"/></filter>
  <filter id="blur20"><feGaussianBlur stdDeviation="20"/></filter>
  <filter id="blur3"><feGaussianBlur stdDeviation="3"/></filter>
  <clipPath id="canto"><rect width="{W}" height="{H}" rx="26"/></clipPath>
</defs>
<g clip-path="url(#canto)">
{cena}
<rect y="{H*0.42:.0f}" width="{W}" height="{H*0.58:.0f}" fill="url(#escuro)"/>
<rect x="44" y="{y0 - tam - 22}" width="56" height="6" rx="3" fill="{cor}"/>
{tit}
<text x="46" y="{H - 52}" font-family="Inter" font-weight="600" font-size="22" fill="#ffffff" fill-opacity="0.72" letter-spacing="0.5">{rodape}</text>
<g transform="translate(36,36)">
  <rect width="{len(genero) * 11.5 + 34:.0f}" height="38" rx="19" fill="#05060c" fill-opacity="0.55" stroke="#fff" stroke-opacity="0.25"/>
  <text x="17" y="25" font-family="Inter" font-weight="700" font-size="16" fill="#fff" letter-spacing="1.6">{genero}</text>
</g>
<g transform="translate({W - 96},36)" opacity="0.9">
  <rect width="60" height="38" rx="10" fill="#05060c" fill-opacity="0.55" stroke="#fff" stroke-opacity="0.25"/>
  <text x="30" y="26" text-anchor="middle" font-family="Inter" font-weight="900" font-size="17" fill="#fff" letter-spacing="1">CCS</text>
</g>
</g>
<rect x="1" y="1" width="{W-2}" height="{H-2}" rx="25" fill="none" stroke="#fff" stroke-opacity="0.14" stroke-width="2"/>
</svg>'''


def estrelas(n, y_max, seed, cor="#fff", r=(0.6, 1.8), op=(0.3, 0.9)):
    rnd = random.Random(seed)
    return "".join(
        f'<circle cx="{rnd.uniform(0, W):.1f}" cy="{rnd.uniform(0, y_max):.1f}" r="{rnd.uniform(*r):.2f}" '
        f'fill="{cor}" fill-opacity="{rnd.uniform(*op):.2f}"/>' for _ in range(n))


def cs():
    rnd = random.Random(16)
    predios = []
    x = -10
    while x < W:
        w = rnd.randint(50, 110)
        h = rnd.randint(150, 300)
        predios.append(f'<rect x="{x}" y="{560 - h}" width="{w}" height="{h + 240}" fill="#1a110b"/>')
        if rnd.random() < 0.45:  # arco
            ax = x + w / 2
            predios.append(f'<path d="M{ax-16} {560} v-38 a16 16 0 0 1 32 0 v38 z" fill="#3a2414"/>')
        for _ in range(rnd.randint(0, 3)):
            jx, jy = x + rnd.randint(8, max(9, w - 22)), 560 - h + rnd.randint(18, max(19, h - 60))
            predios.append(f'<rect x="{jx}" y="{jy}" width="12" height="18" fill="#ffb066" fill-opacity="{rnd.uniform(.15,.55):.2f}"/>')
        x += w + rnd.randint(-6, 14)
    poeira = "".join(f'<circle cx="{rnd.uniform(0,W):.0f}" cy="{rnd.uniform(250,640):.0f}" r="{rnd.uniform(1,3):.1f}" fill="#ffd9a8" fill-opacity="{rnd.uniform(.15,.5):.2f}"/>' for _ in range(70))
    mira = '''<g transform="translate(300,300)" stroke="#fff" stroke-opacity="0.85" fill="none">
      <circle r="118" stroke-width="2.5"/><circle r="78" stroke-width="1.2" stroke-opacity="0.5"/>
      <path d="M0 -150 v58 M0 150 v-58 M-150 0 h58 M150 0 h-58" stroke-width="3"/>
      <path d="M0 -20 v12 M0 20 v-12 M-20 0 h12 M20 0 h-12" stroke-width="3"/>
      <circle r="3" fill="#ff5a36" stroke="none"/></g>'''
    cena = f'''<defs><linearGradient id="ceu" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#160d0a"/><stop offset="0.5" stop-color="#7a3a17"/><stop offset="0.8" stop-color="#e0892b"/><stop offset="1" stop-color="#f6c27a"/></linearGradient></defs>
    <rect width="{W}" height="{H}" fill="url(#ceu)"/>
    <circle cx="430" cy="470" r="120" fill="#ffd38a" fill-opacity="0.55" filter="url(#blur20)"/>
    <circle cx="430" cy="470" r="62" fill="#fff0c8"/>
    {"".join(predios)}<rect y="560" width="{W}" height="240" fill="#120b07"/>{poeira}{mira}'''
    return base("#e0892b", cena, "COUNTER-STRIKE", "FPS TÁTICO", "Versão 1.6  ·  Xash3D FWGS", ["COUNTER-", "STRIKE 1.6"])


def ilha(cx, cy, w, arvores=0, seed=0):
    rnd = random.Random(seed)
    s = (f'<path d="M{cx-w/2} {cy} Q{cx} {cy + w*0.9} {cx+w/2} {cy} Z" fill="#8a5a2b"/>'
         f'<path d="M{cx-w/2} {cy} Q{cx} {cy + w*0.55} {cx+w/2} {cy} Z" fill="#a8703a"/>'
         f'<rect x="{cx-w/2-6}" y="{cy-16}" width="{w+12}" height="26" rx="13" fill="#3cc25a"/>'
         f'<rect x="{cx-w/2-6}" y="{cy-16}" width="{w+12}" height="10" rx="5" fill="#79e07f"/>')
    for i in range(arvores):
        tx = cx - w / 3 + i * (w / 3) + rnd.uniform(-8, 8)
        s += (f'<rect x="{tx-4}" y="{cy-60}" width="8" height="46" fill="#7a4a22"/>'
              f'<circle cx="{tx}" cy="{cy-70}" r="26" fill="#2fae4e"/><circle cx="{tx-8}" cy="{cy-78}" r="12" fill="#5fd36d"/>')
    return s


def nuvem(x, y, k=1.0):
    return (f'<g transform="translate({x},{y}) scale({k})" fill="#fff">'
            '<ellipse cx="0" cy="0" rx="70" ry="30"/><circle cx="-30" cy="-14" r="30"/>'
            '<circle cx="18" cy="-26" r="38"/><circle cx="52" cy="-6" r="26"/></g>')


def mario():
    cena = f'''<defs><linearGradient id="ceu2" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2f8fff"/><stop offset="0.7" stop-color="#8fdcff"/><stop offset="1" stop-color="#d9f6ff"/></linearGradient></defs>
    <rect width="{W}" height="{H}" fill="url(#ceu2)"/>
    <circle cx="470" cy="150" r="70" fill="#fff6c2" fill-opacity="0.9"/><circle cx="470" cy="150" r="130" fill="#fff6c2" fill-opacity="0.35" filter="url(#blur20)"/>
    {nuvem(120, 150, 1.1)}{nuvem(420, 300, 0.8)}{nuvem(250, 420, 0.6)}
    <path d="M-20 640 Q120 520 260 610 T620 590 V800 H-20 Z" fill="#43c15e"/>
    <path d="M-20 690 Q160 600 330 680 T620 660 V800 H-20 Z" fill="#2fa64a"/>
    {ilha(160, 330, 170, 2, 1)}{ilha(450, 470, 150, 1, 2)}{ilha(300, 560, 110, 0, 3)}
    '''
    return base("#e23b3b", cena, "SUPER MARIO 64", "PLATAFORMA 3D", "sm64ex  ·  OpenGL", ["SUPER", "MARIO 64"])


def zelda():
    rnd = random.Random(7)
    pinheiros = ""
    for camada, (y, cor, k) in enumerate([(560, "#0d3a29", 0.8), (620, "#082a1e", 1.0), (690, "#041a12", 1.25)]):
        x = -20
        while x < W + 20:
            h = rnd.uniform(90, 160) * k
            w = h * 0.42
            pinheiros += f'<path d="M{x} {y} L{x + w/2} {y - h} L{x + w} {y} Z" fill="{cor}"/>'
            x += w * rnd.uniform(0.45, 0.8)
        pinheiros += f'<rect x="-10" y="{y-2}" width="{W+20}" height="{H}" fill="{cor}"/>'
    vagalumes = "".join(f'<circle cx="{rnd.uniform(20,W-20):.0f}" cy="{rnd.uniform(430,700):.0f}" r="{rnd.uniform(2,4):.1f}" fill="#d8ff8a" filter="url(#blur3)"/>' for _ in range(28))
    cena = f'''<defs><linearGradient id="ceu3" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#03110d"/><stop offset="0.6" stop-color="#11543b"/><stop offset="1" stop-color="#2f9e5b"/></linearGradient></defs>
    <rect width="{W}" height="{H}" fill="url(#ceu3)"/>{estrelas(90, 380, 3)}
    <circle cx="380" cy="230" r="150" fill="#e9fff2" fill-opacity="0.18" filter="url(#blur20)"/>
    <circle cx="380" cy="230" r="86" fill="#f1fff6"/><circle cx="352" cy="210" r="16" fill="#d6efe1"/><circle cx="405" cy="258" r="10" fill="#d6efe1"/>
    <path d="M-20 520 L110 360 L190 450 L300 300 L420 470 L520 380 L620 500 V800 H-20 Z" fill="#0f4a34" fill-opacity="0.9"/>
    <g fill="#0a3626"><rect x="268" y="330" width="16" height="70"/><rect x="292" y="300" width="20" height="100"/><path d="M288 300 l12 -26 l12 26 z"/><rect x="318" y="340" width="16" height="60"/></g>
    {pinheiros}{vagalumes}'''
    return base("#2f9e5b", cena, "OCARINA OF TIME", "AVENTURA", "Zelda  ·  Ship of Harkinian", ["OCARINA", "OF TIME"])


def palmeira(x, y, k, flip=1):
    folhas = ""
    for ang in (-150, -115, -80, -45, -15, 20):
        a = math.radians(ang)
        ex, ey = x + flip * math.cos(a) * 120 * k, y + math.sin(a) * 60 * k + 40 * k
        cx, cy = x + flip * math.cos(a) * 60 * k, y + math.sin(a) * 90 * k
        folhas += f'<path d="M{x} {y} Q{cx:.0f} {cy:.0f} {ex:.0f} {ey:.0f} Q{(cx+ex)/2:.0f} {(cy+ey)/2+14*k:.0f} {x} {y+6*k:.0f} Z" fill="#160a1c"/>'
    tronco = f'<path d="M{x} {y} Q{x - flip*30*k} {y + 200*k} {x + flip*10*k} {y + 420*k}" stroke="#160a1c" stroke-width="{16*k:.0f}" fill="none" stroke-linecap="round"/>'
    return tronco + folhas


def crash():
    rnd = random.Random(9)
    reflexo = "".join(f'<rect x="{300 - w/2:.0f}" y="{y}" width="{w:.0f}" height="3" rx="1.5" fill="#ffd27a" fill-opacity="{rnd.uniform(.35,.8):.2f}"/>'
                      for y, w in [(560 + i * 14, 180 - i * 14 + rnd.uniform(-20, 20)) for i in range(10)])
    cena = f'''<defs><linearGradient id="ceu4" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2a0f45"/><stop offset="0.45" stop-color="#c2366b"/><stop offset="0.68" stop-color="#ff8a3a"/><stop offset="0.7" stop-color="#ffcf7a"/></linearGradient>
      <linearGradient id="mar" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5b2a6e"/><stop offset="1" stop-color="#12081e"/></linearGradient></defs>
    <rect width="{W}" height="{H}" fill="url(#ceu4)"/>
    <circle cx="300" cy="500" r="200" fill="#ffd27a" fill-opacity="0.35" filter="url(#blur20)"/>
    <circle cx="300" cy="520" r="86" fill="#ffe3a3"/>
    <path d="M150 548 L260 400 Q280 380 300 382 Q320 380 340 400 L470 548 Z" fill="#3b1430"/>
    <path d="M285 390 q-20 -60 10 -110 q30 -50 -5 -100" stroke="#e9b3c9" stroke-opacity="0.35" stroke-width="26" fill="none" filter="url(#blur8)"/>
    <rect y="548" width="{W}" height="252" fill="url(#mar)"/>{reflexo}
    <path d="M-20 700 Q150 650 320 720 T620 700 V800 H-20 Z" fill="#160a1c"/>
    {palmeira(70, 300, 1.15)}{palmeira(560, 360, 0.95, -1)}'''
    return base("#f08a24", cena, "CRASH BANDICOOT", "PLATAFORMA", "PlayStation 1  ·  PCSX-ReARMed", ["CRASH", "BANDICOOT"])


def doom():
    rnd = random.Random(93)
    brasas = "".join(f'<circle cx="{rnd.uniform(0,W):.0f}" cy="{rnd.uniform(120,700):.0f}" r="{rnd.uniform(1,3.5):.1f}" fill="#ffb14a" fill-opacity="{rnd.uniform(.3,.9):.2f}"/>' for _ in range(90))
    rachas = "".join(f'<path d="M{x} 800 L{x + rnd.randint(-40,40)} {rnd.randint(640,700)} L{x + rnd.randint(-60,60)} {rnd.randint(580,630)}" stroke="#ff5a1a" stroke-width="{rnd.uniform(2,5):.1f}" fill="none" filter="url(#blur3)"/>' for x in range(30, 600, 70))
    cena = f'''<defs><linearGradient id="ceu5" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#0c0101"/><stop offset="0.55" stop-color="#5a0a06"/><stop offset="0.8" stop-color="#b3261e"/><stop offset="1" stop-color="#ff6a1f"/></linearGradient></defs>
    <rect width="{W}" height="{H}" fill="url(#ceu5)"/>{estrelas(40, 260, 5, "#ffc2a0")}
    <circle cx="150" cy="200" r="96" fill="#2a0604"/><circle cx="150" cy="200" r="96" fill="none" stroke="#ff7a3a" stroke-opacity="0.45" stroke-width="3"/>
    <path d="M-20 560 L60 430 L120 500 L200 360 L260 470 L330 390 L400 500 L470 410 L540 480 L620 420 V800 H-20 Z" fill="#240504"/>
    <g fill="#160302"><rect x="360" y="330" width="34" height="140"/><path d="M356 330 l21 -40 l21 40 z"/><rect x="404" y="370" width="26" height="100"/><rect x="338" y="400" width="20" height="70"/></g>
    <path d="M-20 620 Q200 560 300 610 T620 590 V800 H-20 Z" fill="#0e0201"/>{rachas}{brasas}'''
    return base("#b3261e", cena, "DOOM", "FPS CLÁSSICO", "1993  ·  Chocolate Doom")


def quake():
    arcos = ""
    for i in range(7):
        k = 0.78 ** i
        w, h = 520 * k, 640 * k
        x, y = 300 - w / 2, 690 - h * 0.98
        op = 0.95 - i * 0.1
        arcos += (f'<path d="M{x:.0f} 800 V{y + w/2:.0f} A{w/2:.0f} {w/2:.0f} 0 0 1 {x + w:.0f} {y + w/2:.0f} V800" '
                  f'fill="none" stroke="#2a2016" stroke-opacity="{op:.2f}" stroke-width="{46*k:.0f}"/>')
    cena = f'''<defs><radialGradient id="fundo6" cx="0.5" cy="0.62" r="0.6">
      <stop offset="0" stop-color="#c79a52"/><stop offset="0.3" stop-color="#5e4526"/><stop offset="1" stop-color="#0b0906"/></radialGradient></defs>
    <rect width="{W}" height="{H}" fill="url(#fundo6)"/>{arcos}
    <path d="M0 800 L240 560 H360 L600 800 Z" fill="#17110b"/>
    <g fill="#ffb347" filter="url(#blur8)"><circle cx="70" cy="420" r="26"/><circle cx="530" cy="420" r="26"/></g>
    <g fill="#ffe0a8"><ellipse cx="70" cy="420" rx="7" ry="12"/><ellipse cx="530" cy="420" rx="7" ry="12"/></g>
    <rect width="{W}" height="{H}" fill="#7d6a4a" fill-opacity="0.08"/>'''
    return base("#7d6a4a", cena, "QUAKE", "FPS GÓTICO", "1996  ·  QuakeSpasm")


def emblema(tam=512):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{tam}" height="{tam}" viewBox="0 0 512 512">
<defs>
  <linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#5b8cff"/><stop offset="0.55" stop-color="#7a5cff"/><stop offset="1" stop-color="#c04cff"/></linearGradient>
  <linearGradient id="brilho" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.35"/><stop offset="0.5" stop-color="#fff" stop-opacity="0"/></linearGradient>
</defs>
<rect x="16" y="16" width="480" height="480" rx="120" fill="url(#g)"/>
<rect x="16" y="16" width="480" height="480" rx="120" fill="url(#brilho)"/>
<g fill="none" stroke="#fff" stroke-linecap="round">
  <path d="M332 150 A150 150 0 1 0 332 362" stroke-width="44"/>
  <path d="M300 206 A82 82 0 1 0 300 306" stroke-width="30" stroke-opacity="0.75"/>
</g>
<path d="M238 216 L298 256 L238 296 Z" fill="#fff" stroke="#fff" stroke-width="14" stroke-linejoin="round"/>
<circle cx="372" cy="256" r="22" fill="#fff"/>
</svg>'''


def logo_horizontal():
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1240" height="300" viewBox="0 0 1240 300">
<defs><linearGradient id="t" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#c9d6ff"/></linearGradient></defs>
<image href="emblema.svg" x="0" y="10" width="280" height="280"/>
<text x="320" y="190" font-family="Inter" font-weight="900" font-size="190" fill="url(#t)" letter-spacing="-4">CCS</text>
<text x="326" y="262" font-family="Inter" font-weight="600" font-size="44" fill="#ffffff" fill-opacity="0.72" letter-spacing="13">CONSOLE CONTRA SONY</text>
</svg>'''


CAPAS = {"cs16": cs, "sm64": mario, "zelda": zelda, "crash": crash, "doom": doom, "quake": quake}
