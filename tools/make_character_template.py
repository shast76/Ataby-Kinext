"""Kinext karakter sablonu ve ornek karakter (Kiki) uretir.
Cikti: assets/character_template/ (kilavuzlu bos parcalar + aciklama) ve assets/characters/Kiki/ (ornek)."""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(__file__), "..", "assets")
S = 4  # yuksek cozunurlukte ciz, sonra kucult (yumusak kenar)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

PARTS = {  # ad: (genislik, yukseklik, ust eklem, alt eklem)
    "head":      (256, 256, "başın tepesi / top of head", "çene / chin"),
    "torso":     (256, 320, "boyun / neck", "kalça / hips"),
    "upper_arm": (96, 200, "omuz / shoulder", "dirsek / elbow"),
    "lower_arm": (88, 200, "dirsek / elbow", "bilek / wrist"),
    "hand":      (96, 128, "bilek / wrist", "parmak ucu / fingertips"),
    "upper_leg": (112, 220, "kalça / hip", "diz / knee"),
    "lower_leg": (104, 220, "diz / knee", "bilek / ankle"),
    "foot":      (120, 128, "ayak bileği / ankle", "ayak ucu / toes"),
}

def canvas(w, h):
    return Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))

def down(im, w, h):
    return im.resize((w, h), Image.LANCZOS)

def template():
    out = os.path.join(ROOT, "character_template")
    os.makedirs(out, exist_ok=True)
    for name, (w, h, top, bot) in PARTS.items():
        im = canvas(w, h); d = ImageDraw.Draw(im)
        W, H = w * S, h * S
        y0, y1 = int(H * 0.1), int(H * 0.9)
        # ornek siluet (acik gri): cizimin buraya gelecek
        pad = int(W * 0.18)
        if name == "head":
            d.ellipse([pad, y0, W - pad, y1], fill=(200, 200, 210, 120), outline=(120, 120, 135, 200), width=3 * S)
        elif name == "torso":
            d.rounded_rectangle([pad, y0, W - pad, y1], radius=W // 6, fill=(200, 200, 210, 120), outline=(120, 120, 135, 200), width=3 * S)
        else:
            d.rounded_rectangle([pad, y0 - W // 4, W - pad, y1 + W // 4], radius=(W - 2 * pad) // 2, fill=(200, 200, 210, 120), outline=(120, 120, 135, 200), width=3 * S)
        # eklem cizgileri (kirmizi, kesikli)
        for y in (y0, y1):
            for x in range(0, W, 8 * S):
                d.line([x, y, x + 4 * S, y], fill=(230, 40, 60, 255), width=2 * S)
            d.ellipse([W // 2 - 5 * S, y - 5 * S, W // 2 + 5 * S, y + 5 * S], fill=(230, 40, 60, 255))
        f = ImageFont.truetype(FONT, int(min(12, w / 9) * S))
        d.text((W // 2, y0 - 3 * S), top, font=f, fill=(230, 40, 60, 255), anchor="md")
        d.text((W // 2, y1 + 3 * S), bot, font=f, fill=(230, 40, 60, 255), anchor="ma")
        d.text((W // 2, H // 2), name, font=ImageFont.truetype(FONT, int(min(16, w / 6) * S)), fill=(90, 90, 105, 255), anchor="mm")
        down(im, w, h).save(os.path.join(out, name + ".png"))
    with open(os.path.join(out, "character.txt"), "w", encoding="utf-8") as fh:
        fh.write("name=Karakterim\ndescription=Benim karakterim\n")
    with open(os.path.join(out, "OKU_BENI.txt"), "w", encoding="utf-8") as fh:
        fh.write(README)

README = """KINEXT KARAKTER SABLONU  /  CHARACTER TEMPLATE
================================================

TURKCE
------
1) Bu klasoru kopyala ve adini degistir (ornek: "Ejderham").
2) Her PNG'nin ustune kendi cizimini yap (Photoshop, Krita, Paint.NET, GIMP...).
   - Arka plan SEFFAF olmali.
   - Kirmizi kesikli cizgiler eklem noktalaridir: parcanin o cizgilere kadar uzanmasi gerekir.
     Ornek: upper_arm.png'de ust cizgi omuz, alt cizgi dirsek.
   - Kirmizi cizgileri ve yazilari cizimini bitirince sil.
   - Resim boyutunu degistirebilirsin; cizgiler her zaman ustten %10 ve alttan %10'da kabul edilir.
3) Kol ve bacak resimleri SAG taraf icindir; sol taraf otomatik aynalanir.
   Farkli olsun istersen upper_arm_left.png gibi "_left" ekli bir dosya ekle.
4) Istege bagli: head_back.png ve torso_back.png (3D oyunlarda arkadan gorunus).
5) character.txt icinde adini ve kisa aciklamasini yaz.
6) Klasoru Kinext penceresine SURUKLE-BIRAK. Karakterin Maskot listesine eklenir.
   (Ya da klasoru %APPDATA%\\Kinext\\characters icine kopyala.)

ENGLISH
-------
1) Copy this folder and rename it (e.g. "MyDragon").
2) Draw over each PNG (Photoshop, Krita, Paint.NET, GIMP...). Keep the background TRANSPARENT.
   The red dashed lines are the joints: the part must reach those lines
   (e.g. upper_arm.png: top line = shoulder, bottom line = elbow). Remove the guides when done.
   You may resize images; the joints are always at 10% from the top and 10% from the bottom.
3) Arm and leg images are for the RIGHT side; the left side is mirrored automatically.
   Add "_left" files (e.g. upper_arm_left.png) for a different left side.
4) Optional: head_back.png and torso_back.png (back view in 3D games).
5) Put the name and a short description in character.txt.
6) DRAG AND DROP the folder onto the Kinext window. Your character appears in the Mascot list.
   (Or copy the folder into %APPDATA%\\Kinext\\characters.)
"""

def kiki():
    """Ornek karakter: yesil uzayli cocuk Kiki"""
    out = os.path.join(ROOT, "characters", "Kiki")
    os.makedirs(out, exist_ok=True)
    green, dgreen, purple, blue, white, dark = (120, 215, 110, 255), (70, 160, 70, 255), (150, 90, 220, 255), (60, 110, 200, 255), (255, 255, 255, 255), (35, 30, 45, 255)
    def part(name, fn):
        w, h = PARTS[name][0], PARTS[name][1]
        im = canvas(w, h); d = ImageDraw.Draw(im); W, H = w * S, h * S
        fn(d, W, H, int(H * 0.1), int(H * 0.9))
        down(im, w, h).save(os.path.join(out, name + ".png"))
    def head(d, W, H, y0, y1):
        d.line([W * 0.5, y0 + H * 0.15, W * 0.62, y0 - H * 0.02], fill=dgreen, width=4 * S)   # anten
        d.ellipse([W * 0.57, y0 - H * 0.08, W * 0.69, y0 + H * 0.04], fill=(255, 220, 60, 255))
        d.ellipse([W * 0.12, y0 + H * 0.08, W * 0.88, y1], fill=green, outline=dgreen, width=4 * S)
        for cx in (0.36, 0.64):
            d.ellipse([W * (cx - 0.12), H * 0.38, W * (cx + 0.12), H * 0.62], fill=white, outline=dark, width=3 * S)
            d.ellipse([W * (cx - 0.05), H * 0.45, W * (cx + 0.05), H * 0.58], fill=dark)
            d.ellipse([W * (cx - 0.03), H * 0.46, W * (cx), H * 0.5], fill=white)
        d.arc([W * 0.38, H * 0.58, W * 0.62, H * 0.76], 20, 160, fill=dark, width=4 * S)
        d.ellipse([W * 0.2, H * 0.62, W * 0.3, H * 0.68], fill=(255, 150, 160, 200)); d.ellipse([W * 0.7, H * 0.62, W * 0.8, H * 0.68], fill=(255, 150, 160, 200))
    def head_back(d, W, H, y0, y1):
        d.line([W * 0.5, y0 + H * 0.15, W * 0.38, y0 - H * 0.02], fill=dgreen, width=4 * S)
        d.ellipse([W * 0.31, y0 - H * 0.08, W * 0.43, y0 + H * 0.04], fill=(255, 220, 60, 255))
        d.ellipse([W * 0.12, y0 + H * 0.08, W * 0.88, y1], fill=green, outline=dgreen, width=4 * S)
    def torso(d, W, H, y0, y1, back=False):
        d.rounded_rectangle([W * 0.14, y0 - H * 0.02, W * 0.86, y1], radius=W // 6, fill=purple, outline=(100, 50, 170, 255), width=4 * S)
        if not back:
            d.ellipse([W * 0.38, H * 0.38, W * 0.62, H * 0.56], fill=(255, 220, 60, 255))   # yildiz rozet yerine daire
            d.text((W * 0.5, H * 0.47), "K", font=ImageFont.truetype(FONT, int(H * 0.12)), fill=purple, anchor="mm")
        d.rectangle([W * 0.14, H * 0.8, W * 0.86, y1], fill=blue)
    def limb(color, outline):
        def f(d, W, H, y0, y1):
            d.rounded_rectangle([W * 0.2, y0 - W * 0.25, W * 0.8, y1 + W * 0.25], radius=int(W * 0.3), fill=color, outline=outline, width=3 * S)
        return f
    def hand(d, W, H, y0, y1):
        d.ellipse([W * 0.15, y0, W * 0.85, y1 - H * 0.1], fill=green, outline=dgreen, width=3 * S)
        d.ellipse([W * 0.05, H * 0.3, W * 0.35, H * 0.6], fill=green, outline=dgreen, width=3 * S)
    def foot(d, W, H, y0, y1):
        d.rounded_rectangle([W * 0.12, y0, W * 0.88, y1], radius=int(W * 0.3), fill=(240, 80, 80, 255), outline=(170, 40, 40, 255), width=3 * S)
        d.rectangle([W * 0.12, y1 - H * 0.15, W * 0.88, y1], fill=white)
    part("head", head); part("torso", torso)
    w, h = PARTS["head"][:2]; im = canvas(w, h); head_back(ImageDraw.Draw(im), w * S, h * S, int(h * S * 0.1), int(h * S * 0.9)); down(im, w, h).save(os.path.join(out, "head_back.png"))
    w, h = PARTS["torso"][:2]; im = canvas(w, h); torso(ImageDraw.Draw(im), w * S, h * S, int(h * S * 0.1), int(h * S * 0.9), True); down(im, w, h).save(os.path.join(out, "torso_back.png"))
    part("upper_arm", limb(purple, (100, 50, 170, 255))); part("lower_arm", limb(green, dgreen)); part("hand", hand)
    part("upper_leg", limb(blue, (40, 70, 150, 255))); part("lower_leg", limb(blue, (40, 70, 150, 255))); part("foot", foot)
    with open(os.path.join(out, "character.txt"), "w", encoding="utf-8") as fh:
        fh.write("name=Kiki\ndescription=Ornek karakter / Example\n")

template(); kiki()
print("ok")
