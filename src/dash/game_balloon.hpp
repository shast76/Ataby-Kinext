// Kinext - Balon Patlat (3D): parkta etrafinda yukselen balonlari ellerinle patlat (60 saniye)
// Kamera oyuncunun karsisinda (ayna gibi): saga uzanirsan maskot da ekranin sagina uzanir.
#pragma once
#include "games.hpp"
#include "avatar3d.hpp"

class BalloonGame : public Game {
public:
    explicit BalloonGame(int* best) : best_(best) {}

    void reset() override {
        balloons_.clear(); parts_.clear();
        time_ = 0; spawn_ = 0.6f; score_ = 0; missed_ = 0; over_ = false;
    }
    int score() const override { return score_; }
    bool finished() const override { return over_; }

    GameAction frame(GameContext& c) override {
        const float dt = c.dt;
        const bool present = c.body.valid;
        if (present) {
            float target = std::clamp(-(c.body[HipCenter].x - 0.5f) * 2.4f, -1.1f, 1.1f);
            px_ += (target - px_) * (std::min)(1.f, dt * 8.f);
        }
        Vector3 root = { px_, 0, 0 };
        // oyuncu kameraya bakiyor: ileri yonu -z
        Body3 body = present ? toWorld(c.body, root, { -1, 0, 0 }, { 0, 0, -1 }, 0.5f)
                             : idleBody3(root, { -1, 0, 0 }, { 0, 0, -1 }, c.time);

        if (!over_ && present) {
            time_ += dt;
            float progress = time_ / DURATION;
            spawn_ -= dt;
            if (spawn_ <= 0) { spawnBalloon(progress); spawn_ = (std::max)(0.32f, 0.95f - progress * 0.6f); }
            for (auto& b : balloons_) {
                b.p.y += b.speed * dt;
                b.p.x += std::sin(time_ * 1.7f + b.wobble) * 0.12f * dt;
            }
            for (auto it = balloons_.begin(); it != balloons_.end();) {
                bool popped = false;
                for (Joint j : { HandLeft, HandRight, WristLeft, WristRight }) {
                    Vector3 d = Vector3Subtract(body[j], it->p);
                    d.z *= 0.4f;   // derinlikte affedici: kamera derinligi olcemez
                    if (Vector3Length(d) < R + 0.12f) popped = true;
                }
                if (popped) { ++score_; burst(it->p, it->color); it = balloons_.erase(it); }
                else if (it->p.y > 3.2f) { ++missed_; it = balloons_.erase(it); }
                else ++it;
            }
            if (time_ >= DURATION) { over_ = true; if (best_ && score_ > *best_) *best_ = score_; }
        }
        for (auto& p : parts_) { p.p = Vector3Add(p.p, Vector3Scale(p.v, dt)); p.v.y -= 6.f * dt; p.life -= dt; }
        parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Part& p) { return p.life <= 0; }), parts_.end());

        // ---------------- cizim ----------------
        DrawRectangleGradientV(0, 0, VW, VH, { 70, 150, 235, 255 }, { 200, 230, 255, 255 });
        Vector3 eye = { px_ * 0.3f, 1.3f, -3.4f };
        Camera3D cam{ eye, { px_ * 0.15f, 1.15f, 0 }, { 0, 1, 0 }, 52.f, CAMERA_PERSPECTIVE };
        auto& L = r3d::light();
        L.eye = eye; L.sun = Vector3Normalize({ 0.4f, 1.f, -0.7f }); L.fill = Vector3Normalize({ -0.6f, 0.4f, -0.4f });
        L.ambient = 0.55f; L.sunK = 0.55f; L.fillK = 0.2f; L.specK = 0.6f; L.shininess = 28.f;
        L.fogColor = { 190, 222, 250, 255 }; L.fogStart = 18.f; L.fogEnd = 60.f;

        r3d::beginScene(cam);
        drawPark(c.time);
        drawAvatar3D(body, c.avatar, c.accent, eye);
        for (const auto& b : balloons_) {
            r3d::draw(r3d::sphere(), r3d::trs(b.p, { R, R * 1.15f, R }), b.color, 1.2f);
            r3d::ballLo({ b.p.x, b.p.y - R * 1.15f, b.p.z }, R * 0.12f, b.color);
            DrawLine3D({ b.p.x, b.p.y - R * 1.2f, b.p.z }, { b.p.x + std::sin(b.wobble) * 0.05f, b.p.y - R * 1.2f - 0.4f, b.p.z }, { 255, 255, 255, 200 });
        }
        for (const auto& p : parts_) r3d::ballLo(p.p, 0.025f, ColorAlpha(p.color, (std::min)(1.f, p.life * 2.f)));
        drawAvatarShadow(body);
        r3d::endScene();

        // ---------------- arayuz ----------------
        int left = static_cast<int>(std::ceil(DURATION - time_));
        topBar(c, T("Puan ", "Score ") + std::to_string(score_), std::to_string((std::max)(left, 0)), T("Kaçan ", "Missed ") + std::to_string(missed_));
        if (!present && !over_) waitingForPlayer(c);
        else if (time_ < 3.f && !over_) textCenterShadow(c.fonts.light, T("Balonları ellerinle patlat!", "Pop the balloons with your hands!"), VW / 2.f, VH * 0.16f, 46, WHITE);
        if (over_) return resultScreen(c, T("Süre bitti!", "Time's up!"), score_, best_ ? *best_ : score_);
        return GameAction::None;
    }

private:
    struct Balloon { Vector3 p; float speed, wobble; Color color; };
    struct Part { Vector3 p, v; float life; Color color; };
    static constexpr float DURATION = 60.f, R = 0.16f;

    void spawnBalloon(float progress) {
        static const Color cols[] = { {235, 55, 70, 255}, {250, 200, 40, 255}, {70, 200, 90, 255}, {190, 80, 220, 255}, {40, 140, 240, 255}, {255, 130, 40, 255} };
        std::uniform_real_distribution<float> ux(-1.0f, 1.0f), uz(-0.15f, 0.15f), us(0.35f, 0.65f), uw(0, 6.28f);
        balloons_.push_back({ { px_ + ux(rng_), -0.3f, uz(rng_) }, us(rng_) * (1.f + progress * 0.9f), uw(rng_), cols[rng_() % 6] });
    }
    void burst(Vector3 p, Color col) {
        std::uniform_real_distribution<float> u(-1, 1);
        for (int i = 0; i < 14; ++i) parts_.push_back({ p, { u(rng_) * 1.6f, u(rng_) * 1.6f + 0.8f, u(rng_) * 1.2f }, 0.7f, col });
    }

    void drawPark(float time) {
        r3d::ground(-30, -6, 30, 40, 0, { 110, 190, 95, 255 });
        r3d::ground(-1.8f, -1.2f, 1.8f, 1.2f, 0.002f, { 200, 180, 140, 255 });   // oyuncunun durdugu kum alan
        // tepeler
        r3d::ellipsoid({ -9, -2.f, 22 }, { 14, 5, 8 }, { 100, 180, 85, 255 });
        r3d::ellipsoid({ 10, -2.5f, 26 }, { 16, 5.5f, 9 }, { 90, 170, 80, 255 });
        // agaclar
        std::mt19937 r(11);
        for (int i = 0; i < 14; ++i) {
            float x = std::uniform_real_distribution<float>(-12, 12)(r), z = std::uniform_real_distribution<float>(5, 18)(r);
            if (std::fabs(x) < 2.5f && z < 8) x += 5.f;
            float h = std::uniform_real_distribution<float>(1.4f, 2.4f)(r);
            r3d::rod({ x, 0, z }, { x, h, z }, 0.12f, { 120, 80, 50, 255 });
            r3d::ball({ x, h + 0.5f, z }, 0.9f, { 60, 150, 70, 255 });
            r3d::ball({ x + 0.4f, h + 0.2f, z - 0.3f }, 0.6f, { 70, 165, 75, 255 });
        }
        // bulutlar
        for (int i = 0; i < 5; ++i) {
            float x = std::fmod(i * 9.f + time * 0.3f, 50.f) - 25.f;
            Vector3 c = { x, 8.f + (i % 3) * 1.5f, 30.f };
            r3d::ballLo(c, 1.6f, { 255, 255, 255, 255 });
            r3d::ballLo({ c.x + 1.6f, c.y - 0.3f, c.z }, 1.2f, { 255, 255, 255, 255 });
            r3d::ballLo({ c.x - 1.5f, c.y - 0.4f, c.z }, 1.1f, { 255, 255, 255, 255 });
        }
    }

    int* best_;
    std::vector<Balloon> balloons_;
    std::vector<Part> parts_;
    std::mt19937 rng_{ std::random_device{}() };
    float time_ = 0, spawn_ = 0, px_ = 0;
    int score_ = 0, missed_ = 0;
    bool over_ = false;
};
