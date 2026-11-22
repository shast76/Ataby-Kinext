# Kinext tanitim gorselleri: tamamen koddan cizilir, gercek kamera goruntusu yok
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math, random

W, H, S = 1280, 720, 2          # 2x cizip kucultuyoruz (yumusak kenarlar)
FB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FR = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
def font(sz, bold=True): return ImageFont.truetype(FB if bold else FR, sz * S)
def P(x, y): return (x * S, y * S)

def background(top=(18, 22, 34), bottom=(6, 8, 14)):
    img = Image.new("RGB", (W * S, H * S))
    d = ImageDraw.Draw(img)
    for y in range(H * S):
        t = y / (H * S)
        d.line([(0, y), (W * S, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))
    return img

def figure(cx, base_y, scale=1.0, arms="up_right", lean=0.0):
    """Kinect formatinda 20 eklem (piksel). arms: poz secimi"""
    s = scale
    hip = (cx + lean * 0.3, base_y - 250 * s)
    sc = (cx + lean, base_y - 430 * s)
    j = {}
    j["HipCenter"] = hip
    j["Spine"] = (hip[0] + (sc[0] - hip[0]) * 0.35, hip[1] + (sc[1] - hip[1]) * 0.35)
    j["ShoulderCenter"] = sc
    j["Head"] = (sc[0] + lean * 0.2, sc[1] - 75 * s)
    j["ShoulderLeft"] = (sc[0] - 75 * s, sc[1] + 8 * s)
    j["ShoulderRight"] = (sc[0] + 75 * s, sc[1] + 8 * s)
    if arms == "up_right":
        j["ElbowLeft"] = (sc[0] - 120 * s, sc[1] + 95 * s); j["WristLeft"] = (sc[0] - 135 * s, sc[1] + 190 * s)
        j["ElbowRight"] = (sc[0] + 150 * s, sc[1] - 40 * s); j["WristRight"] = (sc[0] + 200 * s, sc[1] - 140 * s)
    elif arms == "wide":
        j["ElbowLeft"] = (sc[0] - 165 * s, sc[1] + 20 * s); j["WristLeft"] = (sc[0] - 255 * s, sc[1] - 45 * s)
        j["ElbowRight"] = (sc[0] + 165 * s, sc[1] + 30 * s); j["WristRight"] = (sc[0] + 260 * s, sc[1] - 20 * s)
    else:  # down
        j["ElbowLeft"] = (sc[0] - 100 * s, sc[1] + 110 * s); j["WristLeft"] = (sc[0] - 110 * s, sc[1] + 215 * s)
        j["ElbowRight"] = (sc[0] + 100 * s, sc[1] + 110 * s); j["WristRight"] = (sc[0] + 110 * s, sc[1] + 215 * s)
    for side, sg in (("Left", -1), ("Right", 1)):
        e, w = j["Elbow" + side], j["Wrist" + side]
        j["Hand" + side] = (w[0] + (w[0] - e[0]) * 0.25, w[1] + (w[1] - e[1]) * 0.25)
        j["Hip" + side] = (hip[0] + sg * 50 * s, hip[1] + 10 * s)
        j["Knee" + side] = (hip[0] + sg * 62 * s, hip[1] + 130 * s)
        j["Ankle" + side] = (hip[0] + sg * 70 * s, hip[1] + 240 * s)
        j["Foot" + side] = (j["Ankle" + side][0] + sg * 6 * s, j["Ankle" + side][1] + 18 * s)
    return j

BONES = [("HipCenter","Spine"),("Spine","ShoulderCenter"),("ShoulderCenter","Head"),
 ("ShoulderCenter","ShoulderLeft"),("ShoulderLeft","ElbowLeft"),("ElbowLeft","WristLeft"),("WristLeft","HandLeft"),
 ("ShoulderCenter","ShoulderRight"),("ShoulderRight","ElbowRight"),("ElbowRight","WristRight"),("WristRight","HandRight"),
 ("HipCenter","HipLeft"),("HipLeft","KneeLeft"),("KneeLeft","AnkleLeft"),("AnkleLeft","FootLeft"),
 ("HipCenter","HipRight"),("HipRight","KneeRight"),("KneeRight","AnkleRight"),("AnkleRight","FootRight")]

def draw_silhouette(d, j):
    col = (120, 170, 200)
    for a, b in BONES:
        d.line([P(*j[a]), P(*j[b])], fill=col, width=10 * S)
    hx, hy = j["Head"]; d.ellipse([P(hx - 30, hy - 30), P(hx + 30, hy + 30)], fill=col)

def draw_hands(d, j):
    for h in ("HandLeft", "HandRight"):
        x, y = j[h]
        d.ellipse([P(x - 22, y - 22), P(x + 22, y + 22)], outline=(255, 255, 255), width=3 * S)
        d.ellipse([P(x - 8, y - 8), P(x + 8, y + 8)], fill=(255, 255, 255))

def hud(d, left, mid, right, left_col=(255, 220, 80)):
    d.rectangle([P(0, 0), P(W, 56)], fill=(20, 20, 20))
    d.text(P(20, 12), left, font=font(28), fill=left_col)
    tw = d.textlength(mid, font=font(30)); d.text(((W * S - tw) / 2, 10 * S), mid, font=font(30), fill=(255, 255, 255))
    tw = d.textlength(right, font=font(24)); d.text((W * S - tw - 20 * S, 15 * S), right, font=font(24), fill=(200, 200, 200))

def save(img, name):
    img.resize((W, H), Image.LANCZOS).save(f"/home/claude/project-kinext/docs/images/{name}", optimize=True)

def ctext(d, y, t, sz, col, bold=True):
    f = font(sz, bold); tw = d.textlength(t, font=f); d.text(((W * S - tw) / 2, y * S), t, font=f, fill=col)

# 1) Iskelet: 20 eklem ve adlari
img = background(); d = ImageDraw.Draw(img)
j = figure(560, 680, 1.0, "up_right")
for a, b in BONES:
    d.line([P(*j[a]), P(*j[b])], fill=(60, 220, 60), width=5 * S)
labels_right = {"Head","ShoulderRight","ElbowRight","WristRight","HandRight","HipRight","KneeRight","AnkleRight","FootRight","Spine","ShoulderCenter","HipCenter"}
for name, (x, y) in j.items():
    d.ellipse([P(x - 8, y - 8), P(x + 8, y + 8)], fill=(255, 140, 30))
    f = font(15, False)
    if name in ("ShoulderCenter", "HipCenter"):          # ortadakiler: ustte / altta ortali
        tw = d.textlength(name, font=f)
        d.text((x * S - tw / 2, (y - 34 if name == "ShoulderCenter" else y + 12) * S), name, font=f, fill=(220, 220, 220))
    elif name in labels_right: d.text(P(x + 14, y - 10), name, font=f, fill=(220, 220, 220))
    else:
        tw = d.textlength(name, font=f); d.text((x * S - tw - 14 * S, (y - 10) * S), name, font=f, fill=(220, 220, 220))
d.text(P(860, 200), "KINEXT 1.0", font=font(44), fill=(255, 255, 255))
for i, line in enumerate(["Webcam ile Kinect tarzi iskelet", "20 eklem (Kinect SDK 1.8 formati)", "MoveNet + ONNX Runtime + DirectML", "~30 FPS, Kinect gerekmez"]):
    d.text(P(862, 270 + i * 40), line, font=font(20, False), fill=(190, 200, 215))
save(img, "skeleton.png")

# 2) Balon Patlat
random.seed(7)
img = background((30, 24, 44), (10, 8, 18)); d = ImageDraw.Draw(img)
j = figure(640, 760, 1.05, "wide")
draw_silhouette(d, j)
cols = [(230, 60, 60), (40, 140, 230), (80, 200, 60), (240, 200, 40), (200, 80, 200), (80, 160, 255)]
for (x, y, r) in [(140, 520, 46), (330, 300, 40), (1050, 250, 50), (1150, 560, 42), (880, 140, 38), (470, 610, 44), (760, 430, 36)]:
    c = random.choice(cols)
    d.line([P(x, y + r * 1.15), P(x + 6, y + r * 2.1)], fill=(220, 220, 220), width=2 * S)
    d.ellipse([P(x - r, y - r * 1.15), P(x + r, y + r * 1.15)], fill=c)
    d.ellipse([P(x - r / 3 - r / 5, y - r / 2 - r / 5), P(x - r / 3 + r / 5, y - r / 2 + r / 5)], fill=(255, 255, 255))
hx, hy = j["HandRight"]
for i in range(16):                                    # patlama parcaciklari
    a = i / 16 * 2 * math.pi; rr = random.uniform(30, 70)
    px, py = hx + math.cos(a) * rr, hy + math.sin(a) * rr
    d.ellipse([P(px - 5, py - 5), P(px + 5, py + 5)], fill=(240, 200, 40))
draw_hands(d, j)
hud(d, "PUAN 42", "37", "KACAN 3")
save(img, "balloon.png")

# 3) Engelden Kac
img = background((16, 26, 30), (6, 10, 12)); d = ImageDraw.Draw(img)
lw = W / 3
d.rectangle([P(lw * 2, 56), P(W, H)], fill=(24, 52, 30))
for i in (1, 2): d.line([P(lw * i, 60), P(lw * i, H)], fill=(200, 200, 200), width=2 * S)
for lane, y in [(0, 470), (1, 300), (0, 120), (2, 90)]:
    x0 = lw * lane + lw * 0.1
    d.rectangle([P(x0, y), P(x0 + lw * 0.8, y + 86)], fill=(210, 50, 50), outline=(255, 255, 255), width=2 * S)
j = figure(lw * 2.5, 760, 0.95, "down", lean=25)
draw_silhouette(d, j)
x0 = lw * 2 + lw * 0.25
d.rectangle([P(x0, H * 0.74), P(x0 + lw * 0.5, H * 0.92)], outline=(150, 255, 150), width=3 * S)
draw_hands(d, j)
hud(d, "PUAN 186", "", "CAN  O O -", left_col=(255, 220, 80))
save(img, "dodge.png")

# 4) Menu
img = background((28, 30, 40), (8, 9, 14)); d = ImageDraw.Draw(img)
j = figure(640, 820, 1.0, "up_right")
draw_silhouette(d, j)
ctext(d, 80, "KINEXT 1.0", 64, (255, 255, 255))
ctext(d, 165, "Elini bir oyunun ustunde tut", 22, (200, 200, 200), False)
bw, bh, gap = 384, 144, 64
x1 = (W - (2 * bw + gap)) / 2
for i, (label, col) in enumerate([("BALON PATLAT", (200, 90, 180)), ("ENGELDEN KAC", (200, 90, 60))]):
    x = x1 + i * (bw + gap)
    d.rectangle([P(x, 288), P(x + bw, 288 + bh)], fill=col, outline=(255, 255, 255), width=(3 if i == 0 else 1) * S)
    tw = d.textlength(label, font=font(34)); d.text((x * S + (bw * S - tw) / 2, (288 + 50) * S), label, font=font(34), fill=(255, 255, 255))
hx, hy = x1 + bw * 0.62, 360                          # el butonun ustunde, halka dolmak uzere
d.arc([P(hx - 34, hy - 34), P(hx + 34, hy + 34)], start=-90, end=-90 + 260, fill=(255, 255, 255), width=6 * S)
d.ellipse([P(hx - 8, hy - 8), P(hx + 8, hy + 8)], fill=(255, 255, 255))
d.rectangle([P(W / 2 - 96, 540), P(W / 2 + 96, 612)], fill=(80, 80, 80), outline=(255, 255, 255), width=S)
ctext(d, 560, "CIKIS", 26, (255, 255, 255))
save(img, "menu.png")
print("ok")
