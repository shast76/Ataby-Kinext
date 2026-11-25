// Kinext - kucuk 3D cizim katmani
// Isiklandirma islemcide, kose basina hesaplanir (ortam + iki yonlu isik + parlama + sis).
// Shader gerektirmez: her ekran kartinda ve yazilimsal cizicide ayni gorunur.
#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace r3d {

struct Mesh3 {
    std::vector<Vector3> pos, nrm;
    std::vector<float> shade;    // kose basina renk carpani (cizgili lobut gibi desenler icin 0-1, -1: ikinci renk)
};

// --- Sahne isigi ---
struct Lighting {
    Vector3 sun = Vector3Normalize({ -0.4f, 1.f, -0.5f });
    Vector3 fill = Vector3Normalize({ 0.6f, 0.4f, 0.7f });
    Color sunColor = { 255, 246, 230, 255 };
    float ambient = 0.42f, sunK = 0.62f, fillK = 0.22f, specK = 0.35f, shininess = 24.f;
    Vector3 eye{};
    Color fogColor = { 0, 0, 0, 0 };
    float fogStart = 30.f, fogEnd = 80.f;
    float alpha = 1.f;   // tum cizimlere saydamlik (yari saydam karakter icin)
};
inline Lighting& light() { static Lighting L; return L; }

// --- Birim sekiller (bir kez olusturulur) ---
inline Mesh3 makeSphere(int seg = 18, int rings = 12) {
    Mesh3 m;
    auto P = [&](int i, int j) {
        float th = PI * j / rings, ph = 2 * PI * i / seg;
        return Vector3{ std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph) };
    };
    for (int j = 0; j < rings; ++j)
        for (int i = 0; i < seg; ++i) {
            Vector3 a = P(i, j), b = P(i + 1, j), c = P(i + 1, j + 1), d = P(i, j + 1);
            for (Vector3 v : { a, c, b, a, d, c }) { m.pos.push_back(v); m.nrm.push_back(v); m.shade.push_back(1); }
        }
    return m;
}
// Y ekseni boyunca 0..1, yaricap 1 (kapakli)
inline Mesh3 makeCylinder(int seg = 16) {
    Mesh3 m;
    for (int i = 0; i < seg; ++i) {
        float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
        Vector3 n0 = { std::cos(a0), 0, std::sin(a0) }, n1 = { std::cos(a1), 0, std::sin(a1) };
        Vector3 b0 = n0, b1 = n1, t0 = { n0.x, 1, n0.z }, t1 = { n1.x, 1, n1.z };
        Vector3 vs[] = { b0, t1, b1, b0, t0, t1 }; Vector3 ns[] = { n0, n1, n1, n0, n0, n1 };
        for (int k = 0; k < 6; ++k) { m.pos.push_back(vs[k]); m.nrm.push_back(ns[k]); m.shade.push_back(1); }
        Vector3 up = { 0, 1, 0 }, dn = { 0, -1, 0 };
        for (Vector3 v : { Vector3{ 0, 1, 0 }, t0, t1 }) { m.pos.push_back(v); m.nrm.push_back(up); m.shade.push_back(1); }
        for (Vector3 v : { Vector3{ 0, 0, 0 }, b1, b0 }) { m.pos.push_back(v); m.nrm.push_back(dn); m.shade.push_back(1); }
    }
    return m;
}
// Merkezde, kenar 1
inline Mesh3 makeBox() {
    Mesh3 m;
    const Vector3 n[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    for (const auto& f : n) {
        Vector3 u = std::fabs(f.y) > 0.5f ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 };
        Vector3 v = Vector3CrossProduct(f, u);
        auto c = [&](float a, float b) { return Vector3Add(Vector3Scale(f, 0.5f), Vector3Add(Vector3Scale(u, a * 0.5f), Vector3Scale(v, b * 0.5f))); };
        Vector3 q[] = { c(-1, -1), c(1, -1), c(1, 1), c(-1, 1) };
        for (int k : { 0, 1, 2, 0, 2, 3 }) { m.pos.push_back(q[k]); m.nrm.push_back(f); m.shade.push_back(1); }
    }
    return m;
}
// Donel cisim: profil (yaricap, yukseklik) noktalarindan; band(y) ikinci renk bolgelerini isaretler
inline Mesh3 makeLathe(const std::vector<Vector2>& prof, int seg, const std::function<bool(float)>& band = nullptr) {
    Mesh3 m;
    for (size_t j = 0; j + 1 < prof.size(); ++j) {
        Vector2 p0 = prof[j], p1 = prof[j + 1];
        float dr = p1.x - p0.x, dy = p1.y - p0.y;
        float len = std::sqrt(dr * dr + dy * dy); if (len < 1e-6f) continue;
        float nr = dy / len, ny = -dr / len;   // profil normali (disari)
        float s = band && band((p0.y + p1.y) * 0.5f) ? -1.f : 1.f;
        for (int i = 0; i < seg; ++i) {
            float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
            auto V = [&](Vector2 p, float a) { return Vector3{ p.x * std::cos(a), p.y, p.x * std::sin(a) }; };
            auto N = [&](float a) { return Vector3Normalize({ nr * std::cos(a), ny, nr * std::sin(a) }); };
            Vector3 vs[] = { V(p0, a0), V(p1, a1), V(p0, a1), V(p0, a0), V(p1, a0), V(p1, a1) };
            Vector3 ns[] = { N(a0), N(a1), N(a1), N(a0), N(a0), N(a1) };
            for (int k = 0; k < 6; ++k) { m.pos.push_back(vs[k]); m.nrm.push_back(ns[k]); m.shade.push_back(s); }
        }
    }
    return m;
}

inline const Mesh3& sphere()   { static Mesh3 m = makeSphere();   return m; }
inline const Mesh3& sphereLo() { static Mesh3 m = makeSphere(10, 7); return m; }
inline const Mesh3& cylinder() { static Mesh3 m = makeCylinder(); return m; }
inline const Mesh3& box()      { static Mesh3 m = makeBox();      return m; }

// --- Isik hesabi ---
inline Color shadeColor(Color base, Vector3 p, Vector3 n, float spec) {
    const Lighting& L = light();
    float d1 = (std::max)(0.f, Vector3DotProduct(n, L.sun));
    float d2 = (std::max)(0.f, Vector3DotProduct(n, L.fill));
    float k = L.ambient + L.sunK * d1 + L.fillK * d2;
    float r = base.r / 255.f * k * L.sunColor.r / 255.f;
    float g = base.g / 255.f * k * L.sunColor.g / 255.f;
    float b = base.b / 255.f * k * L.sunColor.b / 255.f;
    if (spec > 0) {
        Vector3 v = Vector3Normalize(Vector3Subtract(L.eye, p));
        Vector3 h = Vector3Normalize(Vector3Add(v, L.sun));
        float s = std::pow((std::max)(0.f, Vector3DotProduct(n, h)), L.shininess) * spec * L.specK;
        r += s; g += s; b += s;
    }
    if (L.fogColor.a > 0) {
        float dist = Vector3Distance(L.eye, p);
        float f = std::clamp((dist - L.fogStart) / (L.fogEnd - L.fogStart), 0.f, 1.f) * L.fogColor.a / 255.f;
        r = r * (1 - f) + L.fogColor.r / 255.f * f;
        g = g * (1 - f) + L.fogColor.g / 255.f * f;
        b = b * (1 - f) + L.fogColor.b / 255.f * f;
    }
    auto c8 = [](float x) { return static_cast<unsigned char>(std::clamp(x, 0.f, 1.f) * 255.f); };
    return { c8(r), c8(g), c8(b), static_cast<unsigned char>(base.a * L.alpha) };
}

// Mesh'i donusumle ciz. col2: desen bolgelerinin rengi (lobut seridi gibi)
inline void draw(const Mesh3& m, const Matrix& M, Color col, float spec = 0.f, Color col2 = BLANK) {
    Matrix N = MatrixTranspose(MatrixInvert(M));
    const size_t n = m.pos.size();
    rlBegin(RL_TRIANGLES);
    for (size_t i = 0; i < n; ++i) {
        if (i % 3 == 0) rlCheckRenderBatchLimit(3);
        Vector3 p = Vector3Transform(m.pos[i], M);
        Vector3 q = { m.nrm[i].x * N.m0 + m.nrm[i].y * N.m4 + m.nrm[i].z * N.m8,
                      m.nrm[i].x * N.m1 + m.nrm[i].y * N.m5 + m.nrm[i].z * N.m9,
                      m.nrm[i].x * N.m2 + m.nrm[i].y * N.m6 + m.nrm[i].z * N.m10 };
        q = Vector3Normalize(q);
        Color base = (m.shade[i] < 0 && col2.a > 0) ? col2 : col;
        Color c = shadeColor(base, p, q, spec);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(p.x, p.y, p.z);
    }
    rlEnd();
}

// 3D sahne basi/sonu: kendi sekillerimiz iki yuzlu cizilir (yuz yonune bagli kesilme olmasin)
inline void beginScene(const Camera3D& cam) { BeginMode3D(cam); rlDisableBackfaceCulling(); }
inline void endScene() { rlEnableBackfaceCulling(); EndMode3D(); }

// --- Kolay cizim yardimcilari ---
inline Matrix trs(Vector3 t, Vector3 s, Matrix rot = MatrixIdentity()) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), rot), MatrixTranslate(t.x, t.y, t.z));
}
// Y eksenini d yonune ceviren donus
inline Matrix alignY(Vector3 d) {
    Vector3 y = { 0, 1, 0 };
    d = Vector3Normalize(d);
    float c = Vector3DotProduct(y, d);
    if (c > 0.9999f) return MatrixIdentity();
    if (c < -0.9999f) return MatrixRotateX(PI);
    Vector3 ax = Vector3Normalize(Vector3CrossProduct(y, d));
    return MatrixRotate(ax, std::acos(c));
}

inline void ball(Vector3 c, float r, Color col, float spec = 0.f) { draw(sphere(), trs(c, { r, r, r }), col, spec); }
inline void ballLo(Vector3 c, float r, Color col) { draw(sphereLo(), trs(c, { r, r, r }), col); }
inline void ellipsoid(Vector3 c, Vector3 r, Color col, Matrix rot = MatrixIdentity(), float spec = 0.f) { draw(sphere(), trs(c, r, rot), col, spec); }
inline void ellipsoid(Vector3 c, Vector3 r, Matrix rot, Color col, float spec = 0.f) { draw(sphere(), trs(c, r, rot), col, spec); }
inline void cube(Vector3 c, Vector3 size, Color col, float yaw = 0.f, float spec = 0.f) { draw(box(), trs(c, size, MatrixRotateY(yaw)), col, spec); }
inline void cubeRot(Vector3 c, Vector3 size, Matrix rot, Color col, float spec = 0.f) { draw(box(), trs(c, size, rot), col, spec); }
// a'dan b'ye yuvarlak uclu cubuk (kol, bacak)
inline void capsule(Vector3 a, Vector3 b, float r, Color col, float spec = 0.f) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len > 1e-4f) draw(cylinder(), trs(a, { r, len, r }, alignY(d)), col, spec);
    ballLo(a, r, col); ballLo(b, r, col);
}
inline void rod(Vector3 a, Vector3 b, float r, Color col, float spec = 0.f) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len > 1e-4f) draw(cylinder(), trs(a, { r, len, r }, alignY(d)), col, spec);
}

// Yere dusen yumusak golge (sahnenin opak kisimlarindan sonra cizilmeli)
inline void shadow(Vector3 p, float r, float alpha = 0.35f, float groundY = 0.f) {
    const int seg = 20;
    rlCheckRenderBatchLimit(seg * 3 * 2);
    rlBegin(RL_TRIANGLES);
    for (int ring = 0; ring < 2; ++ring) {
        float rr = ring ? r : r * 0.6f, a = ring ? alpha * 0.5f : alpha;
        for (int i = 0; i < seg; ++i) {
            float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
            rlColor4f(0, 0, 0, a);
            rlVertex3f(p.x, groundY + 0.004f + ring * 0.001f, p.z);
            rlVertex3f(p.x + std::cos(a1) * rr, groundY + 0.004f + ring * 0.001f, p.z + std::sin(a1) * rr);
            rlVertex3f(p.x + std::cos(a0) * rr, groundY + 0.004f + ring * 0.001f, p.z + std::sin(a0) * rr);
        }
    }
    rlEnd();
}

// Duz renkli (isiksiz) dortgen: zemin cizgileri, isaretler
inline void flatQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col) {
    rlCheckRenderBatchLimit(6);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(col.r, col.g, col.b, col.a);
    for (Vector3 v : { a, b, c, a, c, d }) rlVertex3f(v.x, v.y, v.z);
    for (Vector3 v : { a, c, b, a, d, c }) rlVertex3f(v.x, v.y, v.z);
    rlEnd();
}
// Isikli yatay zemin parcasi (yukari bakan)
inline void ground(float x0, float z0, float x1, float z1, float y, Color col, float spec = 0.f) {
    Vector3 c = { (x0 + x1) / 2, y - 0.005f, (z0 + z1) / 2 };
    draw(box(), trs(c, { x1 - x0, 0.01f, z1 - z0 }), col, spec);
}

// Dokulu dortgen (karakter parcalari icin): 4 kose, saat yonunde ust-sol'dan
inline void texQuad(Texture2D tex, Vector3 tl, Vector3 tr, Vector3 br, Vector3 bl, Color tint, bool flipX = false) {
    rlCheckRenderBatchLimit(8);
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    float u0 = flipX ? 1.f : 0.f, u1 = flipX ? 0.f : 1.f;
    // iki yuzlu: on ve arka
    rlTexCoord2f(u0, 0); rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(u0, 1); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(u1, 1); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(u1, 0); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(u0, 0); rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(u1, 0); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(u1, 1); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(u0, 1); rlVertex3f(bl.x, bl.y, bl.z);
    rlEnd();
    rlSetTexture(0);
}

} // namespace r3d
