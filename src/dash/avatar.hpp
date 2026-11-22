// Kinext 1.0 Dashboard - maskot avatarlar
// Oyuncunun iskeleti, kamera goruntusu yerine bu maskotlara "giydirilir".
// Tum maskotlar Kinext'e ozgu, tamamen koddan cizilir.
#pragma once
#include "ui.hpp"

enum AvatarId { AVATAR_ROBO, AVATAR_POFI, AVATAR_ASTRO, AVATAR_COUNT };

inline const char* avatarName(int id) {
    static const char* names[] = { "Robo", "Pofi", "Astro" };
    return names[std::clamp(id, 0, AVATAR_COUNT - 1)];
}
inline const char* avatarDesc(int id) {
    static const char* d[] = { "Neşeli küçük robot", "Turuncu tüylü dostun", "Uzay kaşifi" };
    return d[std::clamp(id, 0, AVATAR_COUNT - 1)];
}

inline Vector2 jv(const Skeleton& s, Joint j) { return { s[j].x, s[j].y }; }
inline Vector2 vadd(Vector2 a, Vector2 b) { return { a.x + b.x, a.y + b.y }; }
inline Vector2 vsub(Vector2 a, Vector2 b) { return { a.x - b.x, a.y - b.y }; }
inline Vector2 vmul(Vector2 a, float k) { return { a.x * k, a.y * k }; }
inline Vector2 vlerp(Vector2 a, Vector2 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t }; }

// 0-1 iskeleti sanal ekran pikseline cevirir
inline Skeleton toPixels(const Skeleton& n) {
    Skeleton s = n;
    for (auto& j : s.joints) { j.x *= VW; j.y *= VH; }
    return s;
}

// Kadrajda gorunmeyen eklemleri (bacaklar, kollar, bas) dogal bir durusla tamamlar:
// maskot her zaman eksiksiz gorunsun.
inline Skeleton completeBody(Skeleton s) {
    if (!s.valid) return s;
    const float t = (std::max)(s.torso(), 1.f);
    auto missing = [&](Joint j) { return s[j].state == TrackState::NotTracked; };
    auto put = [&](Joint j, Vector2 p) { s.joints[j] = { p.x, p.y, TrackState::Inferred }; };

    if (missing(Head)) put(Head, vadd(jv(s, ShoulderCenter), { 0, -t * 0.5f }));
    struct Side { Joint sh, el, wr, ha, hi, kn, an, ft; float sg; };
    // Aynali goruntude oyuncunun sol tarafi ekranin solunda
    const Side sides[] = {
        { ShoulderLeft, ElbowLeft, WristLeft, HandLeft, HipLeft, KneeLeft, AnkleLeft, FootLeft, -1.f },
        { ShoulderRight, ElbowRight, WristRight, HandRight, HipRight, KneeRight, AnkleRight, FootRight, 1.f } };
    for (const auto& d : sides) {
        if (missing(d.el)) put(d.el, vadd(jv(s, d.sh), { d.sg * t * 0.12f, t * 0.5f }));
        if (missing(d.wr)) put(d.wr, vadd(jv(s, d.el), { 0, t * 0.45f }));
        if (missing(d.ha)) put(d.ha, vadd(jv(s, d.wr), vmul(vsub(jv(s, d.wr), jv(s, d.el)), 0.25f)));
        if (missing(d.kn)) put(d.kn, vadd(jv(s, d.hi), { d.sg * t * 0.04f, t * 0.62f }));
        if (missing(d.an)) put(d.an, vadd(jv(s, d.kn), { 0, t * 0.6f }));
        if (missing(d.ft)) put(d.ft, vadd(jv(s, d.an), { d.sg * t * 0.05f, t * 0.12f }));
    }
    return s;
}

// Vucudu ekrana yerlestirir: kameraya yakin/uzak durmak maskotun boyunu degistirmez.
//  anchorY: kalcanin ekrandaki yuksekligi, torsoPx: govde boyu (piksel)
//  followX: true ise oyuncu saga sola gidince maskot da gider
inline Skeleton fitBody(const Skeleton& norm, float anchorX, float anchorY, float torsoPx, bool followX = true) {
    Skeleton s = completeBody(toPixels(norm));
    if (!s.valid) return s;
    float scale = torsoPx / (std::max)(s.torso(), 1.f);
    Vector2 hip = jv(s, HipCenter);
    float ax = followX ? hip.x : anchorX;
    for (auto& j : s.joints) { j.x = ax + (j.x - hip.x) * scale; j.y = anchorY + (j.y - hip.y) * scale; }
    return s;
}

// --- cizim yardimcilari ---
inline void limb(Vector2 a, Vector2 b, float w, Color c) {
    DrawLineEx(a, b, w, c);
    DrawCircleV(a, w * 0.5f, c);
    DrawCircleV(b, w * 0.5f, c);
}
inline void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) {
    // raylib ucgenleri yon hassas: iki yonu de cizerek her durumda dolu gorunmesini sagla
    DrawTriangle(a, b, c, col); DrawTriangle(a, c, b, col);
    DrawTriangle(a, c, d, col); DrawTriangle(a, d, c, col);
}

struct AvatarLook {
    Color limb, limbDark, body, belly, head, hand, foot;
};

inline AvatarLook lookFor(int id, Color accent) {
    switch (id) {
    case AVATAR_POFI:  return { {245, 150, 60, 255}, {215, 115, 35, 255}, {245, 150, 60, 255}, {255, 222, 175, 255},
                                {248, 158, 66, 255}, {255, 205, 150, 255}, {200, 95, 30, 255} };
    case AVATAR_ASTRO: return { {232, 235, 240, 255}, {190, 195, 205, 255}, {240, 242, 246, 255}, accent,
                                {245, 247, 250, 255}, {210, 214, 222, 255}, {90, 95, 110, 255} };
    default:           return { {150, 162, 175, 255}, {85, 95, 110, 255}, {55, 165, 185, 255}, {35, 120, 140, 255},
                                {205, 210, 220, 255}, {120, 130, 145, 255}, {70, 80, 95, 255} };
    }
}

// Maskotu iskelete gore ciz (iskelet piksel koordinatinda, completeBody/fitBody'den gecmis olmali)
inline void drawAvatar(const Skeleton& s, int id, Color accent, float alpha = 1.f) {
    if (!s.valid) return;
    const float t = (std::max)(s.torso(), 8.f);
    AvatarLook L = lookFor(id, accent);
    auto A = [&](Color c) { return ColorAlpha(c, alpha * c.a / 255.f); };

    Vector2 shL = jv(s, ShoulderLeft), shR = jv(s, ShoulderRight), hiL = jv(s, HipLeft), hiR = jv(s, HipRight);
    Vector2 sc = jv(s, ShoulderCenter), hc = jv(s, HipCenter), head = jv(s, Head);
    // Bas her zaman omuzlarin ustunde, dogal mesafede dursun
    Vector2 up = vsub(head, sc);
    float ul = std::sqrt(up.x * up.x + up.y * up.y);
    if (ul < 1.f) up = { 0, -1 }; else up = vmul(up, 1.f / ul);
    float headR = t * (id == AVATAR_ROBO ? 0.42f : 0.5f);
    Vector2 hp = vadd(sc, vmul(up, t * 0.22f + headR));

    // 1) Bacaklar
    for (int side = 0; side < 2; ++side) {
        Joint hi = side ? HipRight : HipLeft, kn = side ? KneeRight : KneeLeft;
        Joint an = side ? AnkleRight : AnkleLeft, ft = side ? FootRight : FootLeft;
        limb(jv(s, hi), jv(s, kn), t * 0.3f, A(L.limbDark));
        limb(jv(s, kn), jv(s, an), t * 0.26f, A(L.limbDark));
        if (id == AVATAR_ROBO) DrawCircleV(jv(s, kn), t * 0.13f, A(L.foot));
        Vector2 f = jv(s, ft), a = jv(s, an);
        float dir = side ? 1.f : -1.f;
        DrawEllipse(static_cast<int>(f.x + dir * t * 0.08f), static_cast<int>((a.y + f.y) / 2 + t * 0.05f),
                    t * 0.24f, t * 0.12f, A(L.foot));
    }

    // 2) Govde
    Vector2 wide = vmul(vsub(shR, shL), 0.12f);
    quad(vsub(shL, wide), vadd(shR, wide), vadd(hiR, vmul(wide, 0.5f)), vsub(hiL, vmul(wide, 0.5f)), A(L.body));
    DrawCircleV(vlerp(sc, hc, 0.55f), t * 0.42f, A(L.body));   // yuvarlak hat
    switch (id) {
    case AVATAR_ROBO: {   // gogus paneli ve isik
        Vector2 c = vlerp(sc, hc, 0.4f);
        DrawRectangleRounded({ c.x - t * 0.25f, c.y - t * 0.18f, t * 0.5f, t * 0.36f }, 0.3f, 8, A(L.belly));
        DrawCircleV(c, t * 0.09f, A(accent));
        DrawCircleV(c, t * 0.05f, A(WHITE));
        break; }
    case AVATAR_POFI:     // acik renk gobek
        DrawEllipse(static_cast<int>((sc.x + hc.x) / 2), static_cast<int>(sc.y + (hc.y - sc.y) * 0.6f),
                    t * 0.3f, t * 0.36f, A(L.belly));
        break;
    case AVATAR_ASTRO: {  // kontrol kutusu + vurgu renkli serit
        Vector2 c = vlerp(sc, hc, 0.38f);
        DrawRectangleRounded({ c.x - t * 0.2f, c.y - t * 0.13f, t * 0.4f, t * 0.26f }, 0.25f, 8, A(L.limbDark));
        DrawCircleV({ c.x - t * 0.08f, c.y }, t * 0.04f, A(RED));
        DrawCircleV({ c.x + t * 0.06f, c.y }, t * 0.04f, A(GREEN));
        DrawLineEx(vlerp(shL, hiL, 0.85f), vlerp(shR, hiR, 0.85f), t * 0.1f, A(accent));
        break; }
    }

    // 3) Boyun ve bas
    limb(sc, vadd(sc, vmul(up, t * 0.25f)), t * 0.18f, A(L.limbDark));
    switch (id) {
    case AVATAR_ROBO: {
        Rectangle r = { hp.x - headR * 1.1f, hp.y - headR, headR * 2.2f, headR * 1.9f };
        DrawLineEx({ hp.x, r.y }, { hp.x, r.y - t * 0.22f }, t * 0.05f, A(L.limbDark));   // anten
        DrawCircleV({ hp.x, r.y - t * 0.25f }, t * 0.07f, A(accent));
        DrawRectangleRounded(r, 0.35f, 10, A(L.head));
        Rectangle visor = { r.x + r.width * 0.12f, r.y + r.height * 0.25f, r.width * 0.76f, r.height * 0.4f };
        DrawRectangleRounded(visor, 0.5f, 10, A({ 30, 40, 55, 255 }));
        DrawRectangleRounded({ visor.x + visor.width * 0.18f, visor.y + visor.height * 0.25f, visor.width * 0.18f, visor.height * 0.5f }, 0.5f, 6, A({ 90, 230, 255, 255 }));
        DrawRectangleRounded({ visor.x + visor.width * 0.64f, visor.y + visor.height * 0.25f, visor.width * 0.18f, visor.height * 0.5f }, 0.5f, 6, A({ 90, 230, 255, 255 }));
        DrawRectangleRounded({ hp.x - r.width * 0.15f, r.y + r.height * 0.75f, r.width * 0.3f, r.height * 0.08f }, 0.5f, 6, A(L.limbDark));
        break; }
    case AVATAR_POFI: {
        // kulaklar
        DrawTriangle({ hp.x - headR * 0.85f, hp.y - headR * 0.4f }, { hp.x - headR * 0.6f, hp.y - headR * 1.35f }, { hp.x - headR * 0.15f, hp.y - headR * 0.8f }, A(L.limbDark));
        DrawTriangle({ hp.x - headR * 0.85f, hp.y - headR * 0.4f }, { hp.x - headR * 0.15f, hp.y - headR * 0.8f }, { hp.x - headR * 0.6f, hp.y - headR * 1.35f }, A(L.limbDark));
        DrawTriangle({ hp.x + headR * 0.85f, hp.y - headR * 0.4f }, { hp.x + headR * 0.15f, hp.y - headR * 0.8f }, { hp.x + headR * 0.6f, hp.y - headR * 1.35f }, A(L.limbDark));
        DrawTriangle({ hp.x + headR * 0.85f, hp.y - headR * 0.4f }, { hp.x + headR * 0.6f, hp.y - headR * 1.35f }, { hp.x + headR * 0.15f, hp.y - headR * 0.8f }, A(L.limbDark));
        DrawCircleV(hp, headR, A(L.head));
        // gozler
        for (float sx : { -1.f, 1.f }) {
            Vector2 e = { hp.x + sx * headR * 0.36f, hp.y - headR * 0.08f };
            DrawCircleV(e, headR * 0.24f, A(WHITE));
            DrawCircleV({ e.x + sx * headR * 0.03f, e.y + headR * 0.04f }, headR * 0.12f, A({ 40, 30, 30, 255 }));
            DrawCircleV({ e.x - headR * 0.04f, e.y - headR * 0.05f }, headR * 0.04f, A(WHITE));
            DrawCircleV({ hp.x + sx * headR * 0.62f, hp.y + headR * 0.3f }, headR * 0.12f, A({ 255, 140, 150, 140 }));
        }
        DrawCircleV({ hp.x, hp.y + headR * 0.2f }, headR * 0.08f, A({ 90, 45, 30, 255 }));   // burun
        DrawRing({ hp.x, hp.y + headR * 0.22f }, headR * 0.2f, headR * 0.27f, 20, 160, 16, A({ 90, 45, 30, 255 }));   // gulumseme
        break; }
    case AVATAR_ASTRO: {
        DrawCircleV(hp, headR * 1.08f, A(L.limbDark));
        DrawCircleV(hp, headR, A(L.head));
        DrawEllipse(static_cast<int>(hp.x), static_cast<int>(hp.y + headR * 0.05f), headR * 0.72f, headR * 0.55f, A({ 25, 35, 60, 255 }));
        DrawEllipse(static_cast<int>(hp.x - headR * 0.25f), static_cast<int>(hp.y - headR * 0.15f), headR * 0.2f, headR * 0.12f, A({ 180, 210, 255, 200 }));
        DrawRing(hp, headR * 0.98f, headR * 1.1f, 200, 340, 24, A(accent));
        break; }
    }

    // 4) Kollar (govdenin onunde)
    for (int side = 0; side < 2; ++side) {
        Joint sh = side ? ShoulderRight : ShoulderLeft, el = side ? ElbowRight : ElbowLeft;
        Joint wr = side ? WristRight : WristLeft, ha = side ? HandRight : HandLeft;
        limb(jv(s, sh), jv(s, el), t * 0.24f, A(L.limb));
        limb(jv(s, el), jv(s, wr), t * 0.21f, A(L.limb));
        if (id == AVATAR_ROBO) DrawCircleV(jv(s, el), t * 0.11f, A(L.limbDark));
        if (id == AVATAR_ASTRO) DrawCircleV(jv(s, wr), t * 0.13f, A(accent));   // eldiven bilekligi
        DrawCircleV(jv(s, ha), t * 0.16f, A(L.hand));
    }
}

// Oyuncu gorunmezken (ya da onizlemelerde) kullanilan varsayilan durus: hafifce sallanan, el sallayan maskot
inline Skeleton idleBody(float cx, float hipY, float t, float time) {
    Skeleton s;
    s.valid = true;
    auto put = [&](Joint j, float x, float y) { s.joints[j] = { cx + x * t, hipY + y * t, TrackState::Tracked }; };
    float sway = std::sin(time * 1.6f) * 0.03f;
    float wave = std::sin(time * 5.f) * 0.25f;
    put(HipCenter, 0, 0);          put(Spine, sway * 0.5f, -0.35f);
    put(ShoulderCenter, sway, -1.f); put(Head, sway * 1.2f, -1.5f);
    put(ShoulderLeft, -0.38f + sway, -0.96f); put(ElbowLeft, -0.5f, -0.5f); put(WristLeft, -0.52f, -0.06f); put(HandLeft, -0.53f, 0.06f);
    put(ShoulderRight, 0.38f + sway, -0.96f); put(ElbowRight, 0.75f, -1.15f); put(WristRight, 0.85f + wave * 0.3f, -1.6f); put(HandRight, 0.88f + wave * 0.35f, -1.72f);
    put(HipLeft, -0.22f, 0.04f);  put(KneeLeft, -0.25f, 0.62f); put(AnkleLeft, -0.26f, 1.2f); put(FootLeft, -0.31f, 1.3f);
    put(HipRight, 0.22f, 0.04f);  put(KneeRight, 0.25f, 0.62f); put(AnkleRight, 0.26f, 1.2f); put(FootRight, 0.31f, 1.3f);
    return s;
}
