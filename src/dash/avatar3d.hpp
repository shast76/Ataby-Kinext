// Kinext - 3D maskotlar
// Kameradan gelen 2D iskelet, oyun dunyasinda dikey bir duzleme yerlestirilir (Kinect Sports'taki
// gibi kamera genelde oyuncunun arkasindadir). Hazir maskotlar 3D sekillerden kurulur; kullanicinin
// PNG karakterleri ayni duzlemde dokulu parcalar olarak cizilir.
#pragma once
#include "avatar.hpp"
#include "r3d.hpp"
#include <array>

struct Body3 {
    std::array<Vector3, JOINT_COUNT> j{};
    bool valid = false;
    Vector3 right{ -1, 0, 0 }, up{ 0, 1, 0 }, fwd{ 0, 0, 1 };
    float torso = 0.55f;   // metre
    Vector3 operator[](Joint k) const { return j[k]; }
};

// norm: 0-1 aynali iskelet. root: kalcanin zemindeki izdusumu. right: ekranda saga dogru dunya yonu,
// fwd: maskotun baktigi yon. torsoM: govde boyu (metre). Ayaklar zemine oturtulur.
inline Body3 toWorld(const Skeleton& norm, Vector3 root, Vector3 right, Vector3 fwd, float torsoM = 0.5f, float lift = 0.f) {
    Body3 b;
    b.right = Vector3Normalize(right); b.fwd = Vector3Normalize(fwd); b.torso = torsoM;
    Skeleton s = completeBody(toPixels(norm));
    if (!s.valid) return b;
    float scale = torsoM / (std::max)(s.torso(), 1.f);
    Vector2 hip = jv(s, HipCenter);
    float footY = (std::max)(s[FootLeft].y, s[FootRight].y);
    float hipH = (footY - hip.y) * scale + 0.05f + lift;
    for (int k = 0; k < JOINT_COUNT; ++k) {
        float x = (s.joints[k].x - hip.x) * scale, y = hipH + (hip.y - s.joints[k].y) * scale;
        b.j[k] = Vector3Add(root, Vector3Add(Vector3Scale(b.right, x), { 0, y, 0 }));
    }
    // eller ve dirsekler govdenin biraz onunde: arkadan bakinca kollar govdenin icinde kaybolmasin
    for (Joint k : { HandLeft, HandRight, WristLeft, WristRight }) b.j[k] = Vector3Add(b.j[k], Vector3Scale(b.fwd, 0.08f));
    for (Joint k : { ElbowLeft, ElbowRight }) b.j[k] = Vector3Add(b.j[k], Vector3Scale(b.fwd, 0.04f));
    b.valid = true;
    return b;
}

// Bekleme durusu (oyuncu gorunmezken)
inline Body3 idleBody3(Vector3 root, Vector3 right, Vector3 fwd, float time, float torsoM = 0.5f) {
    Skeleton n = idleBody(0.5f * VW, 0.55f * VH, 100.f, time);
    for (auto& j : n.joints) { j.x /= VW; j.y /= VH; }
    // el sallamayi kapat: kollar yanda
    return toWorld(n, root, right, fwd, torsoM);
}

namespace avatar3d {

inline Matrix basis(Vector3 r, Vector3 u, Vector3 f) {
    Matrix m = MatrixIdentity();
    m.m0 = r.x; m.m1 = r.y; m.m2 = r.z;
    m.m4 = u.x; m.m5 = u.y; m.m6 = u.z;
    m.m8 = f.x; m.m9 = f.y; m.m10 = f.z;
    return m;
}

// Govde yonleri: yukari (kalcadan omuza), sag, ileri
inline void frame(const Body3& b, Vector3& r, Vector3& u, Vector3& f) {
    u = Vector3Normalize(Vector3Subtract(b[ShoulderCenter], b[HipCenter]));
    f = b.fwd;
    r = Vector3Normalize(Vector3CrossProduct(u, f));
    if (Vector3DotProduct(r, b.right) < 0) r = Vector3Negate(r);
    f = Vector3Normalize(Vector3CrossProduct(r, u));
    if (Vector3DotProduct(f, b.fwd) < 0) f = Vector3Negate(f);
}

inline Color darker(Color c, float k) { return { static_cast<unsigned char>(c.r * k), static_cast<unsigned char>(c.g * k), static_cast<unsigned char>(c.b * k), c.a }; }

inline void drawCustom3D(const Body3& b, const CustomCharacter& ch, Vector3 eye) {
    Vector3 r, u, f; frame(b, r, u, f);
    // kamera karakterin arkasindaysa arka yuz resimleri (varsa) kullanilir
    bool fromBack = Vector3DotProduct(Vector3Subtract(eye, b[HipCenter]), b.fwd) < 0;
    std::vector<std::tuple<const char*, bool, Vector3, Vector3>> plan;
    customPartPlan<Vector3>([&](Joint k) { return b[k]; }, Vector3Add, Vector3Subtract, Vector3Scale,
                            [](Vector3 v) { float l = Vector3Length(v); return l < 1e-5f ? Vector3{ 0, -1, 0 } : Vector3Scale(v, 1.f / l); },
                            b.torso, u, plan);
    // arkadan bakarken kollar govdenin arkasinda kalmasin: once govde/bas, sonra uzuvlar
    if (fromBack) std::stable_sort(plan.begin(), plan.end(), [](auto& x, auto& y) {
        auto rank = [](const char* p) { std::string s = p; return (s == "torso" || s == "head") ? 0 : 1; };
        return rank(std::get<0>(x)) < rank(std::get<0>(y)); });
    for (auto& [part, left, a, bb] : plan) {
        std::string name = part;
        const Texture2D* tx = nullptr;
        if (fromBack && (name == "head" || name == "torso")) tx = ch.get(name + "_back");
        if (!tx) tx = ch.get(name, left);
        if (!tx || tx->id == 0) continue;
        bool hasLeft = left && ch.get(name, true) != ch.get(name, false);
        Vector3 d = Vector3Subtract(bb, a);
        float len = Vector3Length(d);
        if (len < 1e-4f) continue;
        float H = len / 0.8f, W = tx->width * H / tx->height;
        Vector3 dn = Vector3Scale(d, 1.f / len);
        Vector3 side = Vector3Normalize(Vector3CrossProduct(dn, f));
        if (Vector3DotProduct(side, r) < 0) side = Vector3Negate(side);
        Vector3 top = Vector3Subtract(a, Vector3Scale(dn, H * 0.1f)), bot = Vector3Add(bb, Vector3Scale(dn, H * 0.1f));
        Vector3 hw = Vector3Scale(side, W / 2);
        // resmin solu ekranin soluna gelsin (karakter bize bakiyor varsayimi)
        r3d::texQuad(*tx, Vector3Subtract(top, hw), Vector3Add(top, hw), Vector3Add(bot, hw), Vector3Subtract(bot, hw), WHITE, left && !hasLeft);
    }
}

} // namespace avatar3d

// Maskotu 3D ciz. Golgeler ayrica drawAvatarShadow ile (opak cizimlerden sonra) cizilir.
inline void drawAvatar3D(const Body3& b, int id, Color accent, Vector3 eye) {
    using namespace avatar3d;
    if (!b.valid) return;
    if (const CustomCharacter* ch = customAvatar(id)) { drawCustom3D(b, *ch, eye); return; }
    id = std::clamp(id, 0, AVATAR_COUNT - 1);
    AvatarLook L = lookFor(id, accent);
    Vector3 r, u, f; frame(b, r, u, f);
    const float t = b.torso;
    const float spec = id == AVATAR_ROBO ? 1.0f : id == AVATAR_ASTRO ? 0.6f : 0.15f;

    // --- bacaklar ve ayaklar ---
    for (int side = 0; side < 2; ++side) {
        Joint hi = side ? HipRight : HipLeft, kn = side ? KneeRight : KneeLeft, an = side ? AnkleRight : AnkleLeft;
        r3d::capsule(b[hi], b[kn], t * 0.15f, L.limbDark, spec);
        r3d::capsule(b[kn], b[an], t * 0.13f, L.limbDark, spec);
        if (id == AVATAR_ROBO) r3d::ball(b[kn], t * 0.17f, L.foot, spec);
        Vector3 foot = Vector3Add(b[an], Vector3Add(Vector3Scale(f, t * 0.12f), { 0, -t * 0.06f, 0 }));
        r3d::ellipsoid(foot, { t * 0.17f, t * 0.11f, t * 0.27f }, L.foot, basis(r, { 0, 1, 0 }, Vector3Normalize({ f.x, 0, f.z })), spec);
    }

    // --- govde ---
    Vector3 sc = b[ShoulderCenter], hc = b[HipCenter];
    float shoulderW = Vector3Distance(b[ShoulderLeft], b[ShoulderRight]);
    Vector3 mid = Vector3Lerp(sc, hc, 0.5f);
    Matrix R = basis(r, u, f);
    float len = Vector3Distance(sc, hc);
    if (id == AVATAR_ROBO) {
        r3d::cubeRot(mid, { shoulderW * 1.25f, len * 1.1f, t * 0.42f }, R, L.body, spec);
        r3d::cubeRot(Vector3Add(mid, Vector3Scale(f, t * 0.21f)), { shoulderW * 0.7f, len * 0.5f, t * 0.04f }, R, L.belly, spec);
        r3d::ball(Vector3Add(Vector3Lerp(sc, hc, 0.4f), Vector3Scale(f, t * 0.24f)), t * 0.08f, accent, 1.f);
        r3d::cubeRot(Vector3Add(mid, Vector3Scale(f, -t * 0.24f)), { shoulderW * 0.8f, len * 0.7f, t * 0.12f }, R, L.limbDark, spec);   // sirt paneli
        for (int i = 0; i < 3; ++i)   // havalandirma cizgileri
            r3d::cubeRot(Vector3Add(Vector3Add(mid, Vector3Scale(u, (i - 1) * len * 0.18f)), Vector3Scale(f, -t * 0.31f)), { shoulderW * 0.6f, t * 0.03f, t * 0.02f }, R, accent);
    } else {
        r3d::ellipsoid(mid, { shoulderW * 0.68f, len * 0.68f, t * 0.27f }, R, L.body, spec);
        if (id == AVATAR_POFI) {
            r3d::ellipsoid(Vector3Add(Vector3Lerp(sc, hc, 0.58f), Vector3Scale(f, t * 0.12f)), { shoulderW * 0.42f, len * 0.42f, t * 0.17f }, R, L.belly, spec);
            // kuyruk: kalcanin arkasindan yukari kivrilan
            Vector3 p = Vector3Add(hc, Vector3Scale(f, -t * 0.25f));
            for (int i = 0; i < 6; ++i) {
                float a = i * 0.32f;
                Vector3 q = Vector3Add(p, Vector3Add(Vector3Scale(f, -t * 0.16f * std::cos(a)), Vector3Scale(u, t * 0.16f * i)));
                r3d::ball(q, t * (0.13f + 0.02f * i), i == 5 ? L.belly : L.limb, spec);
            }
        } else {   // Astro: sirt cantasi ve vurgu serit
            r3d::cubeRot(Vector3Add(Vector3Lerp(sc, hc, 0.4f), Vector3Scale(f, -t * 0.34f)), { shoulderW * 0.95f, len * 0.85f, t * 0.3f }, R, L.limbDark, spec);
            r3d::cubeRot(Vector3Add(Vector3Lerp(sc, hc, 0.25f), Vector3Scale(f, -t * 0.5f)), { shoulderW * 0.6f, t * 0.08f, t * 0.04f }, R, accent);
            r3d::rod(Vector3Add(Vector3Lerp(sc, hc, 0.85f), Vector3Scale(r, -shoulderW * 0.62f)), Vector3Add(Vector3Lerp(sc, hc, 0.85f), Vector3Scale(r, shoulderW * 0.62f)), t * 0.06f, accent);
            r3d::cubeRot(Vector3Add(Vector3Lerp(sc, hc, 0.38f), Vector3Scale(f, t * 0.27f)), { shoulderW * 0.45f, len * 0.25f, t * 0.06f }, R, L.limbDark, spec);
        }
    }

    // --- boyun ve bas ---
    Vector3 neckTop = Vector3Add(sc, Vector3Scale(u, t * 0.2f));
    r3d::rod(sc, neckTop, t * 0.1f, L.limbDark);
    float hr = t * (id == AVATAR_ROBO ? 0.38f : 0.42f);
    Vector3 hp = Vector3Add(neckTop, Vector3Scale(u, hr * 0.9f));
    Matrix HR = basis(r, u, f);
    if (id == AVATAR_ROBO) {
        r3d::cubeRot(hp, { hr * 2.2f, hr * 1.9f, hr * 1.9f }, HR, L.head, spec);
        r3d::cubeRot(Vector3Add(hp, Vector3Scale(f, hr * 0.92f)), { hr * 1.7f, hr * 0.8f, hr * 0.12f }, HR, { 30, 40, 55, 255 }, 1.f);
        for (float sx : { -1.f, 1.f })
            r3d::ball(Vector3Add(Vector3Add(hp, Vector3Scale(f, hr * 1.0f)), Vector3Scale(r, sx * hr * 0.42f)), hr * 0.16f, { 90, 230, 255, 255 }, 1.f);
        Vector3 top = Vector3Add(hp, Vector3Scale(u, hr * 0.95f));
        r3d::rod(top, Vector3Add(top, Vector3Scale(u, t * 0.25f)), t * 0.025f, L.limbDark);
        r3d::ball(Vector3Add(top, Vector3Scale(u, t * 0.28f)), t * 0.06f, accent, 1.f);
        for (float sx : { -1.f, 1.f }) r3d::rod(Vector3Add(hp, Vector3Scale(r, sx * hr * 1.1f)), Vector3Add(hp, Vector3Scale(r, sx * hr * 1.25f)), hr * 0.3f, L.limbDark);   // kulaklik
    } else if (id == AVATAR_POFI) {
        r3d::ball(hp, hr, L.head, spec);
        for (float sx : { -1.f, 1.f }) {   // kulaklar
            Vector3 base = Vector3Add(Vector3Add(hp, Vector3Scale(u, hr * 0.7f)), Vector3Scale(r, sx * hr * 0.55f));
            Vector3 dir = Vector3Normalize(Vector3Add(u, Vector3Scale(r, sx * 0.35f)));
            r3d::ellipsoid(Vector3Add(base, Vector3Scale(dir, hr * 0.3f)), { hr * 0.3f, hr * 0.5f, hr * 0.14f }, r3d::alignY(dir), L.limbDark, spec);
        }
        for (float sx : { -1.f, 1.f }) {
            // goz: kafa yuzeyinin uzerinde (yuzeye dik yerlestirilir)
            Vector3 dir = Vector3Normalize(Vector3Add(Vector3Add(Vector3Scale(f, 0.86f), Vector3Scale(r, sx * 0.4f)), Vector3Scale(u, 0.18f)));
            Vector3 e = Vector3Add(hp, Vector3Scale(dir, hr * 0.92f));
            r3d::ball(e, hr * 0.24f, WHITE, 0.5f);
            r3d::ball(Vector3Add(e, Vector3Scale(dir, hr * 0.16f)), hr * 0.13f, { 40, 30, 30, 255 }, 1.f);
            r3d::ballLo(Vector3Add(Vector3Add(e, Vector3Scale(dir, hr * 0.27f)), Vector3Scale(u, hr * 0.05f)), hr * 0.04f, WHITE);
        }
        r3d::ellipsoid(Vector3Add(Vector3Add(hp, Vector3Scale(f, hr * 0.75f)), Vector3Scale(u, -hr * 0.3f)), { hr * 0.42f, hr * 0.3f, hr * 0.3f }, HR, L.belly);
        r3d::ball(Vector3Add(Vector3Add(hp, Vector3Scale(f, hr * 1.02f)), Vector3Scale(u, -hr * 0.18f)), hr * 0.09f, { 90, 45, 30, 255 }, 1.f);
    } else {   // Astro: kask
        r3d::ball(hp, hr * 1.12f, L.head, 1.f);
        r3d::ellipsoid(Vector3Add(hp, Vector3Scale(f, hr * 0.62f)), { hr * 0.8f, hr * 0.62f, hr * 0.55f }, HR, { 25, 35, 60, 255 }, 1.5f);
        r3d::rod(Vector3Add(hp, Vector3Scale(u, -hr * 0.95f)), Vector3Add(hp, Vector3Scale(u, -hr * 0.75f)), hr * 0.85f, accent);
    }

    // --- kollar ---
    for (int side = 0; side < 2; ++side) {
        Joint sh = side ? ShoulderRight : ShoulderLeft, el = side ? ElbowRight : ElbowLeft;
        Joint wr = side ? WristRight : WristLeft, ha = side ? HandRight : HandLeft;
        r3d::capsule(b[sh], b[el], t * 0.12f, L.limb, spec);
        r3d::capsule(b[el], b[wr], t * 0.105f, L.limb, spec);
        if (id == AVATAR_ROBO) r3d::ball(b[el], t * 0.14f, L.limbDark, spec);
        if (id == AVATAR_ASTRO) r3d::rod(Vector3Lerp(b[el], b[wr], 0.75f), b[wr], t * 0.13f, accent);
        r3d::ball(b[ha], t * 0.15f, L.hand, spec);
    }
}

inline void drawAvatarShadow(const Body3& b, float groundY = 0.f) {
    if (!b.valid) return;
    r3d::shadow({ b[HipCenter].x, 0, b[HipCenter].z }, b.torso * 0.9f, 0.3f, groundY);
    for (Joint k : { FootLeft, FootRight }) r3d::shadow({ b[k].x, 0, b[k].z }, b.torso * 0.3f, 0.3f, groundY);
}
