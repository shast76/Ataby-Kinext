// Kinext 1.0 Dashboard - tema, yazi tipleri, el imleci ve secim (hover) sistemi
#pragma once
#include "../skeleton.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>

constexpr int VW = 1280, VH = 720;   // sanal cozunurluk: pencere ne olursa olsun buna gore cizilir

// --- Vurgu renkleri (Xbox One'daki gibi kullanici secer) ---
struct AccentColor { const char* name; Color color; };
inline const AccentColor ACCENTS[] = {
    { "Mavi",       {   0, 120, 215, 255 } },
    { "Yeşil",      {  16, 124,  16, 255 } },
    { "Kırmızı",    { 200,  30,  45, 255 } },
    { "Turuncu",    { 225, 105,  20, 255 } },
    { "Pembe",      { 225,   0, 135, 255 } },
    { "Mor",        { 120,  50, 190, 255 } },
    { "Turkuaz",    {   0, 145, 155, 255 } },
    { "Sarı",       { 205, 160,   0, 255 } },
    { "Kahverengi", { 135,  85,  45, 255 } },
    { "Gri",        {  95,  98, 110, 255 } },
};
constexpr int ACCENT_COUNT = sizeof(ACCENTS) / sizeof(ACCENTS[0]);

// --- Yazi tipleri: Windows'ta Segoe UI (Xbox One'in yazi tipi), yoksa DejaVu, o da yoksa varsayilan ---
struct Fonts {
    Font light{}, regular{}, bold{};

    void load() {
        const char* charset =
            " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
            "ÇçĞğİıÖöŞşÜüâî•…·×→←●○";
        int count = 0;
        int* cps = LoadCodepoints(charset, &count);
        light   = loadFirst({ "C:/Windows/Fonts/segoeuil.ttf", "C:/Windows/Fonts/segoeui.ttf",
                              "/usr/share/fonts/truetype/dejavu/DejaVuSans-ExtraLight.ttf",
                              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" }, cps, count);
        regular = loadFirst({ "C:/Windows/Fonts/segoeui.ttf",
                              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" }, cps, count);
        bold    = loadFirst({ "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/segoeui.ttf",
                              "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" }, cps, count);
        UnloadCodepoints(cps);
    }

private:
    static Font loadFirst(std::initializer_list<const char*> paths, int* cps, int count) {
        for (const char* p : paths) {
            if (!FileExists(p)) continue;
            Font f = LoadFontEx(p, 72, cps, count);
            if (f.texture.id != 0) { SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR); return f; }
        }
        return GetFontDefault();
    }
};

inline void text(const Font& f, const std::string& s, float x, float y, float size, Color c) {
    DrawTextEx(f, s.c_str(), { x, y }, size, 0, c);
}
inline float textWidth(const Font& f, const std::string& s, float size) {
    return MeasureTextEx(f, s.c_str(), size, 0).x;
}
inline void textCenter(const Font& f, const std::string& s, float cx, float y, float size, Color c) {
    text(f, s, cx - textWidth(f, s, size) / 2, y, size, c);
}
// Golgeli yazi (oyun ekranlari icin)
inline void textCenterShadow(const Font& f, const std::string& s, float cx, float y, float size, Color c) {
    textCenter(f, s, cx + 3, y + 3, size, { 0, 0, 0, 160 });
    textCenter(f, s, cx, y, size, c);
}

inline Color withAlpha(Color c, float a) { return ColorAlpha(c, std::clamp(a, 0.f, 1.f)); }
inline Color lighten(Color c, float f) { return ColorBrightness(c, f); }
inline bool inside(Vector2 p, Rectangle r) { return CheckCollisionPointRec(p, r); }

// --- El imleci + secim sistemi ---
// Kinect'teki gibi: elini kaldirinca ekranda imlec belirir, bir seyin ustunde tutunca halka dolar ve secilir.
// Fare de calisir: tiklamak aninda secer.
class Ui {
public:
    static constexpr float HOLD = 1.0f;

    // Imleci yoneten el: 0 = otomatik (hangisi yukaridaysa), 1 = sol el, 2 = sag el
    enum HandMode { HAND_AUTO = 0, HAND_LEFT = 1, HAND_RIGHT = 2 };
    static const char* handModeName(int m) {
        static const char* n[] = { "Otomatik", "Sol el", "Sağ el" };
        return n[std::clamp(m, 0, 2)];
    }
    void setHandMode(int m) { handMode_ = std::clamp(m, 0, 2); }

    void begin(float dt, const Skeleton& norm) {
        dt_ = dt;
        blocked_ = false;
        claimed_ = false;
        clicked_ = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // Fare gercekten hareket ettiyse bir sure fare imlecini kullan
        Vector2 m = GetMousePosition();
        if (std::fabs(m.x - lastMouse_.x) + std::fabs(m.y - lastMouse_.y) > 6.f) mouseTime_ = 2.5f;
        lastMouse_ = m;
        mouseTime_ = (std::max)(0.f, mouseTime_ - dt);

        Vector2 target{};
        bool found = norm.valid && pickHand(norm, target);
        if (found) {
            lostTime_ = 0;
            if (!handSeen_) { handPos_ = target; filtered_ = target; }
            // Uyarlanir yumusatma (One Euro mantigi): el yavasken cok yumusak (titreme yok),
            // hizliyken az yumusak (gecikme yok)
            float dx = target.x - filtered_.x, dy = target.y - filtered_.y;
            float raw = std::sqrt(dx * dx + dy * dy) / (std::max)(dt, 0.001f);   // piksel/sn
            speed_ += (raw - speed_) * 0.25f;          // hiz tahminini de yumusat: tek karelik titreme hizli sayilmasin
            float alpha = std::clamp(0.05f + (speed_ - 400.f) / 5000.f, 0.05f, 0.65f);
            filtered_.x += dx * alpha;
            filtered_.y += dy * alpha;
            handPos_ = filtered_;
            handSeen_ = true;
        } else if (handSeen_) {
            // Takip bir anlik koptuysa imleci hemen kaybetme
            lostTime_ += dt;
            if (lostTime_ > 0.5f) { handSeen_ = false; activeHand_ = -1; }
        }
        handActive_ = handSeen_ && mouseTime_ <= 0.f;
        cursor_ = handActive_ ? handPos_ : m;
    }

    // Bir alan secildi mi? (el ile tutma ya da fare tiklamasi). id her oge icin benzersiz olmali.
    // Onay penceresi acikken arkadaki ogeler tepki vermesin
    void setBlocked(bool b) { blocked_ = b; }

    bool select(const std::string& id, Rectangle r, float hold = HOLD) {
        if (blocked_) return false;
        // Ustunde durulan oge biraz daha genis sayilir: kenarda titreyen el halkayi sifirlamasin
        bool current = hoverId_ == id;
        Rectangle hit = current ? Rectangle{ r.x - 24, r.y - 24, r.width + 48, r.height + 48 } : r;
        if (!inside(cursor_, hit)) return false;
        claimed_ = true;
        if (clicked_ && !handActive_) { clicked_ = false; hoverId_.clear(); return true; }
        if (!handActive_) return false;
        if (!current) { hoverId_ = id; holdTime_ = 0; holdMax_ = hold; }
        holdTime_ += dt_;
        graceTime_ = 0;
        if (holdTime_ >= hold) { hoverId_.clear(); holdTime_ = 0; return true; }
        return false;
    }
    bool hovered(Rectangle r) const { return !blocked_ && inside(cursor_, r); }

    // Imlec ogeden kisa sure (0.25 sn) cikip geri donerse ilerleme kaybolmasin
    void end() {
        if (claimed_) return;
        graceTime_ += dt_;
        if (graceTime_ > 0.25f) { hoverId_.clear(); holdTime_ = 0; graceTime_ = 0; }
    }

    // Imleci ciz (en son, her seyin ustune): Kinect'teki gibi yari saydam beyaz bir el
    void drawCursor() const {
        if (!handActive_) return;
        Vector2 p = cursor_;
        bool leftHand = activeHand_ == 0;
        const RenderTexture2D& rt = handTexture();
        if (rt.id != 0) {
            // El tek parca doku olarak basilir: yari saydamlikta parcalar ust uste binip koyu iz birakmaz
            Rectangle src = { 0, 0, leftHand ? -128.f : 128.f, -128.f };   // render dokusu ters, sol el aynali
            const float sz = 60.f;
            Rectangle dst = { p.x - sz / 2, p.y - sz * 72.f / 128.f, sz, sz };
            DrawTexturePro(rt.texture, src, { dst.x + 3, dst.y + 4, sz, sz }, { 0, 0 }, 0, { 0, 0, 0, 70 });   // golge
            DrawTexturePro(rt.texture, src, dst, { 0, 0 }, 0, { 255, 255, 255, 205 });
        } else {
            drawHandShape(p, 1.f, { 255, 255, 255, 205 }, leftHand);
        }
        if (!hoverId_.empty() && holdMax_ > 0) {
            float a = std::clamp(holdTime_ / holdMax_, 0.f, 1.f);
            DrawRing(p, 40, 46, -90, 270, 48, { 255, 255, 255, 60 });
            DrawRing(p, 40, 46, -90, -90 + 360 * a, 48, { 255, 255, 255, 235 });
        }
    }

    // El imleci dokusu: pencere acildiktan sonra, ana dongu baslamadan bir kez cagrilir
    static RenderTexture2D& handTexture() { static RenderTexture2D rt{}; return rt; }
    static void buildHandTexture() {
        RenderTexture2D& rt = handTexture();
        rt = LoadRenderTexture(128, 128);
        SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
        BeginTextureMode(rt);
        ClearBackground(BLANK);
        drawHandShape({ 64, 72 }, 2.15f, WHITE, false);
        EndTextureMode();
    }

    // Acik el silueti: avuc + 4 parmak + basparmak. leftHand ise basparmak diger tarafta.
    static void drawHandShape(Vector2 c, float k, Color col, bool leftHand) {
        float d = leftHand ? 1.f : -1.f;   // basparmagin yonu
        // avuc
        DrawRectangleRounded({ c.x - 13 * k, c.y - 6 * k, 26 * k, 24 * k }, 0.55f, 10, col);
        // parmaklar (orta parmaklar daha uzun)
        const float fx[] = { -11.5f, -4.f, 3.5f, 11.f }, fl[] = { 15.f, 21.f, 20.f, 15.f };
        for (int i = 0; i < 4; ++i) {
            float x = c.x + fx[i] * k * (leftHand ? -1.f : 1.f);
            DrawRectangleRounded({ x - 3.2f * k, c.y - 6 * k - fl[i] * k, 6.4f * k, fl[i] * k + 6 * k }, 1.f, 8, col);
        }
        // basparmak: avucun yanindan disari egik
        Vector2 a = { c.x + d * 11 * k, c.y + 8 * k }, b = { c.x + d * 22 * k, c.y - 4 * k };
        DrawLineEx(a, b, 7 * k, col);
        DrawCircleV(b, 3.5f * k, col);
    }

    Vector2 cursor() const { return cursor_; }
    bool handActive() const { return handActive_; }

private:
    // Hangi el imleci yonetiyor (-1 yok, 0 sol, 1 sag) ve o elin "etkilesim kutusu"na gore ekran konumu.
    // Otomatik modda el, digeri belirgin sekilde daha yukari kalkmadikca degismez (imlec ziplamasin).
    bool pickHand(const Skeleton& n, Vector2& out) {
        const float t = (std::max)(n.torso(), 0.05f);
        const JointPos& hip = n[HipCenter];
        auto usable = [&](Joint j) { return n[j].state != TrackState::NotTracked && n[j].y < hip.y - t * 0.1f; };
        bool l = usable(HandLeft) && handMode_ != HAND_RIGHT;
        bool r = usable(HandRight) && handMode_ != HAND_LEFT;
        if (!l && !r) return false;

        int want = activeHand_;
        if (want == 0 && !l) want = -1;
        if (want == 1 && !r) want = -1;
        if (want < 0) want = (l && r) ? (n[HandLeft].y < n[HandRight].y ? 0 : 1) : (l ? 0 : 1);
        else if (l && r) {
            // Diger el, aktif elden en az 0.35 govde boyu yukaridaysa gecis yap
            Joint other = want == 0 ? HandRight : HandLeft, cur = want == 0 ? HandLeft : HandRight;
            if (n[other].y < n[cur].y - t * 0.35f) want = 1 - want;
        }
        if (want != activeHand_) handSeen_ = false;   // el degisti: yumusatmayi yeni konumdan baslat
        activeHand_ = want;

        // Kinect'teki gibi her elin kendi omzu etrafinda bir etkilesim kutusu var:
        // kolu cok uzatmadan, rahat bir hareketle tum ekrana ulasilir
        const JointPos& hand = n[want == 0 ? HandLeft : HandRight];
        const JointPos& sh = n[want == 0 ? ShoulderLeft : ShoulderRight];
        float side = want == 0 ? -1.f : 1.f;
        float cx = sh.x + side * t * 0.2f;           // kutunun merkezi: omzun biraz disi
        float cy = n[ShoulderCenter].y + t * 0.15f;  // gogus hizasi
        float u = 0.5f + (hand.x - cx) / (t * 1.5f);
        float v = 0.5f + (hand.y - cy) / (t * 1.1f);
        out = { std::clamp(u, 0.f, 1.f) * VW, std::clamp(v, 0.f, 1.f) * VH };
        return true;
    }

    int handMode_ = HAND_AUTO, activeHand_ = -1;
    float lostTime_ = 0, graceTime_ = 0, speed_ = 0;
    bool blocked_ = false;
    Vector2 filtered_{};
    float dt_ = 0, holdTime_ = 0, holdMax_ = HOLD, mouseTime_ = 0;
    bool claimed_ = false, clicked_ = false, handActive_ = false, handSeen_ = false;
    std::string hoverId_;
    Vector2 cursor_{}, handPos_{}, lastMouse_{};
};

// --- Basit cizim ikonlari (tum ikonlar koddan cizilir) ---
enum class Icon { None, Games, Store, Settings, Camera, Avatar, Power, Color, Play, Hand, Reset };

inline void drawIcon(Icon icon, Vector2 c, float s, Color col) {
    switch (icon) {
    case Icon::Games:   // 4 kare
        for (int i = 0; i < 4; ++i)
            DrawRectangleRec({ c.x - s * 0.5f + (i % 2) * s * 0.55f, c.y - s * 0.5f + (i / 2) * s * 0.55f, s * 0.45f, s * 0.45f }, col);
        break;
    case Icon::Store: { // canta
        DrawRectangleRec({ c.x - s * 0.45f, c.y - s * 0.2f, s * 0.9f, s * 0.7f }, col);
        DrawRing({ c.x, c.y - s * 0.2f }, s * 0.18f, s * 0.26f, 180, 360, 24, col);
        break; }
    case Icon::Settings: { // disli
        DrawRing(c, s * 0.18f, s * 0.36f, 0, 360, 32, col);
        for (int i = 0; i < 8; ++i) {
            float a = i * PI / 4;
            Vector2 a1 = { c.x + std::cos(a) * s * 0.3f, c.y + std::sin(a) * s * 0.3f };
            Vector2 a2 = { c.x + std::cos(a) * s * 0.48f, c.y + std::sin(a) * s * 0.48f };
            DrawLineEx(a1, a2, s * 0.16f, col);
        }
        break; }
    case Icon::Camera:
        DrawRectangleRounded({ c.x - s * 0.5f, c.y - s * 0.3f, s * 0.75f, s * 0.6f }, 0.25f, 8, col);
        DrawTriangle({ c.x + s * 0.25f, c.y }, { c.x + s * 0.5f, c.y + s * 0.25f }, { c.x + s * 0.5f, c.y - s * 0.25f }, col);
        break;
    case Icon::Avatar:
        DrawCircleV({ c.x, c.y - s * 0.22f }, s * 0.22f, col);
        DrawCircleSector({ c.x, c.y + s * 0.42f }, s * 0.42f, 180, 360, 32, col);
        break;
    case Icon::Power:
        DrawRing(c, s * 0.3f, s * 0.4f, -60, 240, 32, col);
        DrawLineEx({ c.x, c.y - s * 0.5f }, { c.x, c.y - s * 0.05f }, s * 0.1f, col);
        break;
    case Icon::Color:
        for (int i = 0; i < 3; ++i)
            DrawCircleV({ c.x - s * 0.25f + i * s * 0.25f, c.y + (i == 1 ? -s * 0.2f : s * 0.1f) }, s * 0.22f,
                        ColorAlpha(col, 0.55f + i * 0.15f));
        break;
    case Icon::Play:
        DrawTriangle({ c.x - s * 0.3f, c.y - s * 0.4f }, { c.x - s * 0.3f, c.y + s * 0.4f }, { c.x + s * 0.4f, c.y }, col);
        break;
    case Icon::Hand:    // avuc + 4 parmak + basparmak
        DrawRectangleRounded({ c.x - s * 0.3f, c.y - s * 0.05f, s * 0.6f, s * 0.5f }, 0.4f, 8, col);
        for (int i = 0; i < 4; ++i)
            DrawRectangleRounded({ c.x - s * 0.3f + i * s * 0.155f, c.y - s * 0.45f + (i == 0 || i == 3 ? s * 0.08f : 0), s * 0.13f, s * 0.5f }, 0.5f, 6, col);
        DrawLineEx({ c.x - s * 0.25f, c.y + s * 0.15f }, { c.x - s * 0.48f, c.y - s * 0.08f }, s * 0.13f, col);
        break;
    case Icon::Reset:   // geri donen ok
        DrawRing(c, s * 0.3f, s * 0.41f, 40, 330, 32, col);
        DrawTriangle({ c.x + s * 0.18f, c.y - s * 0.52f }, { c.x + s * 0.18f, c.y - s * 0.08f }, { c.x + s * 0.52f, c.y - s * 0.3f }, col);
        DrawTriangle({ c.x + s * 0.18f, c.y - s * 0.52f }, { c.x + s * 0.52f, c.y - s * 0.3f }, { c.x + s * 0.18f, c.y - s * 0.08f }, col);
        break;
    default: break;
    }
}

// Metro kutusu: duz renk, ortada ikon, sol altta yazi. Ustundeyken beyaz cerceve.
inline void drawTile(const Fonts& f, Rectangle r, Color bg, const std::string& label, Icon icon, bool hover,
                     float labelSize = 19) {
    DrawRectangleRec(r, hover ? lighten(bg, 0.12f) : bg);
    if (icon != Icon::None) drawIcon(icon, { r.x + r.width / 2, r.y + r.height / 2 - 10 }, (std::min)(r.width, r.height) * 0.32f, WHITE);
    if (!label.empty()) text(f.regular, label, r.x + 12, r.y + r.height - labelSize - 10, labelSize, WHITE);
    if (hover) DrawRectangleLinesEx({ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 3, WHITE);
}
