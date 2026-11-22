// Kinext 1.0 - Balon Patlat: maskotunla yukselen balonlari patlat (60 saniye)
#pragma once
#include "games.hpp"

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
        // Oyuncu ekranin ortasinda, buyuk; saga sola gidince maskot da gider
        Skeleton body = fitBody(c.body, VW / 2.f, VH * 0.6f, 170.f);
        const bool playing = !over_ && body.valid;
        const float dt = c.dt;

        if (playing) {
            time_ += dt;
            spawn_ -= dt;
            float progress = time_ / DURATION;
            if (spawn_ <= 0) { spawnBalloon(progress); spawn_ = (std::max)(0.35f, 1.0f - progress * 0.6f); }
            for (auto& b : balloons_) {
                b.pos.y -= b.speed * dt;
                b.pos.x += std::sin(time_ * 2.f + b.wobble) * 25.f * dt;
            }
            for (auto it = balloons_.begin(); it != balloons_.end();) {
                bool popped = false;
                for (Joint j : { HandLeft, HandRight, WristLeft, WristRight }) {
                    Vector2 d = vsub(jv(body, j), it->pos);
                    if (d.x * d.x + d.y * d.y < (it->r + 26.f) * (it->r + 26.f)) popped = true;
                }
                if (popped) { ++score_; burst(it->pos, it->color); it = balloons_.erase(it); }
                else if (it->pos.y < -it->r * 2.5f) { ++missed_; it = balloons_.erase(it); }
                else ++it;
            }
            if (time_ >= DURATION) { over_ = true; if (best_ && score_ > *best_) *best_ = score_; }
        }
        for (auto& p : parts_) { p.pos = vadd(p.pos, vmul(p.vel, dt)); p.vel.y += 700.f * dt; p.life -= dt; }
        parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.life <= 0; }), parts_.end());

        // --- Sahne: gokyuzu, bulutlar, tepeler ---
        DrawRectangleGradientV(0, 0, VW, VH, { 70, 150, 230, 255 }, { 190, 225, 255, 255 });
        for (int i = 0; i < 5; ++i) {
            float x = std::fmod(i * 310.f + c.time * (12.f + i * 4.f), VW + 300.f) - 150.f;
            float y = 110.f + (i % 3) * 70.f;
            DrawEllipse(static_cast<int>(x), static_cast<int>(y), 90, 28, { 255, 255, 255, 200 });
            DrawEllipse(static_cast<int>(x + 40), static_cast<int>(y - 18), 55, 26, { 255, 255, 255, 200 });
        }
        DrawEllipse(VW / 4, VH + 60, 520, 190, { 120, 195, 100, 255 });
        DrawEllipse(VW * 3 / 4, VH + 80, 600, 200, { 100, 180, 85, 255 });

        drawAvatar(body, c.avatar, c.accent);

        for (const auto& b : balloons_) {
            DrawLineEx({ b.pos.x, b.pos.y + b.r * 1.1f }, { b.pos.x + std::sin(b.wobble) * 8.f, b.pos.y + b.r * 2.2f }, 2, { 255, 255, 255, 220 });
            DrawTriangle({ b.pos.x, b.pos.y + b.r * 1.08f }, { b.pos.x - 7, b.pos.y + b.r * 1.25f }, { b.pos.x + 7, b.pos.y + b.r * 1.25f }, b.color);
            DrawEllipse(static_cast<int>(b.pos.x), static_cast<int>(b.pos.y), b.r, b.r * 1.15f, b.color);
            DrawEllipse(static_cast<int>(b.pos.x - b.r * 0.35f), static_cast<int>(b.pos.y - b.r * 0.45f), b.r * 0.18f, b.r * 0.26f, { 255, 255, 255, 190 });
        }
        for (const auto& p : parts_) DrawCircleV(p.pos, 7.f * (p.life / 0.6f) + 1.f, p.color);

        int left = static_cast<int>(std::ceil((std::max)(0.f, DURATION - time_)));
        topBar(c, "Puan " + std::to_string(score_), std::to_string(left), "Kaçan " + std::to_string(missed_));
        if (!body.valid && !over_) waitingForPlayer(c);
        else if (time_ < 2.5f) textCenterShadow(c.fonts.light, "Balonları ellerinle patlat!", VW / 2.f, VH * 0.18f, 46, WHITE);

        if (over_) return resultScreen(c, "Süre doldu!", score_, best_ ? *best_ : score_);
        return GameAction::None;
    }

private:
    struct Balloon { Vector2 pos; float r, speed, wobble; Color color; };
    struct Particle { Vector2 pos, vel; float life; Color color; };
    static constexpr float DURATION = 60.f;

    void spawnBalloon(float progress) {
        std::uniform_real_distribution<float> u(0.f, 1.f);
        Balloon b;
        b.r = 38.f + 22.f * u(rng_);
        b.pos = { 80.f + u(rng_) * (VW - 160.f), VH + b.r };
        b.speed = VH * (0.17f + 0.12f * u(rng_)) * (1.f + progress);
        b.wobble = u(rng_) * 6.28f;
        static const Color cols[] = { {230, 60, 70, 255}, {250, 200, 40, 255}, {80, 200, 90, 255}, {200, 90, 210, 255}, {40, 140, 235, 255}, {255, 140, 40, 255} };
        b.color = cols[rng_() % 6];
        balloons_.push_back(b);
    }
    void burst(Vector2 at, Color color) {
        std::uniform_real_distribution<float> a(0.f, 6.283f), s(160.f, 460.f);
        for (int i = 0; i < 18; ++i) {
            float ang = a(rng_), sp = s(rng_);
            parts_.push_back({ at, { std::cos(ang) * sp, std::sin(ang) * sp }, 0.6f, color });
        }
    }

    int* best_;
    std::vector<Balloon> balloons_;
    std::vector<Particle> parts_;
    std::mt19937 rng_{ std::random_device{}() };
    float time_ = 0, spawn_ = 0;
    int score_ = 0, missed_ = 0;
    bool over_ = false;
};
