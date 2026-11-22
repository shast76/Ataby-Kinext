// Kinext 1.0 - Engelden Kac: 3 seritte dusen bloklardan maskotunla kac (3 can)
#pragma once
#include "games.hpp"

class DodgeGame : public Game {
public:
    explicit DodgeGame(int* best) : best_(best) {}

    void reset() override {
        blocks_.clear();
        time_ = 0; spawn_ = 1.2f; lives_ = 3; dodged_ = 0; over_ = false; flash_ = 0;
    }
    int score() const override { return dodged_ * 10 + static_cast<int>(time_); }
    bool finished() const override { return over_; }

    GameAction frame(GameContext& c) override {
        const float laneW = VW / static_cast<float>(LANES);
        const float playerTop = VH * 0.66f, playerBottom = VH * 0.95f;
        const float dt = c.dt;

        // Serit: kalcanin yatay konumu (0-1). Maskot o seridin ortasina kayarak gider.
        if (c.body.valid && c.body[HipCenter].state != TrackState::NotTracked)
            lane_ = std::clamp(static_cast<int>(c.body[HipCenter].x * LANES), 0, LANES - 1);
        float targetX = laneW * (lane_ + 0.5f);
        avatarX_ += (targetX - avatarX_) * (std::min)(1.f, dt * 12.f);
        Skeleton body = fitBody(c.body, avatarX_, VH * 0.79f, 95.f, false);

        if (!over_ && body.valid) {
            time_ += dt;
            float speed = VH * (0.45f + 0.025f * time_);
            spawn_ -= dt;
            if (spawn_ <= 0) { spawnRow(); spawn_ = (std::max)(0.55f, 1.4f - time_ * 0.02f); }
            for (auto& b : blocks_) {
                b.y += speed * dt;
                bool overlap = b.y + BLOCK_H > playerTop && b.y < playerBottom;
                if (overlap && !b.done && b.lane == lane_) { b.done = true; b.hit = true; --lives_; flash_ = 0.45f; }
                if (b.y > playerBottom && !b.done) { b.done = true; ++dodged_; }
            }
            blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(), [](const Block& b) { return b.y > VH; }), blocks_.end());
            if (lives_ <= 0) { over_ = true; if (best_ && score() > *best_) *best_ = score(); }
        }
        flash_ = (std::max)(0.f, flash_ - dt);

        // --- Sahne: gece yolu ---
        DrawRectangleGradientV(0, 0, VW, VH, { 20, 22, 45, 255 }, { 45, 30, 70, 255 });
        for (int i = 0; i < 40; ++i) {   // yildizlar
            float x = std::fmod(i * 197.f, static_cast<float>(VW)), y = std::fmod(i * 131.f + c.time * 30.f, static_cast<float>(VH));
            DrawCircleV({ x, y }, 1.5f + (i % 3) * 0.6f, { 255, 255, 255, 110 });
        }
        DrawRectangle(static_cast<int>(laneW * lane_), 0, static_cast<int>(laneW), VH, ColorAlpha(c.accent, 0.18f));
        for (int i = 1; i < LANES; ++i) {   // kesik serit cizgileri
            float off = std::fmod(c.time * 260.f, 60.f);
            for (float y = -60 + off; y < VH; y += 60) DrawRectangleRec({ laneW * i - 3, y, 6, 32 }, { 255, 255, 255, 120 });
        }

        for (const auto& b : blocks_) {
            Rectangle r = { laneW * b.lane + laneW * 0.12f, b.y, laneW * 0.76f, BLOCK_H };
            Color col = b.hit ? Color{ 120, 120, 130, 255 } : Color{ 225, 60, 70, 255 };
            DrawRectangleRounded(r, 0.2f, 8, col);
            DrawRectangleRounded({ r.x + 8, r.y + 8, r.width - 16, r.height * 0.35f }, 0.4f, 8, ColorAlpha(WHITE, 0.25f));
        }

        drawAvatar(body, c.avatar, c.accent);
        if (flash_ > 0) DrawRectangle(0, 0, VW, VH, ColorAlpha(RED, flash_));

        std::string hearts;
        for (int i = 0; i < 3; ++i) hearts += i < lives_ ? "● " : "○ ";
        topBar(c, "Puan " + std::to_string(score()), "", "Can " + hearts);
        if (!body.valid && !over_) waitingForPlayer(c);
        else if (time_ < 3.f && !over_) textCenterShadow(c.fonts.light, "Sağa sola kayarak kaç!", VW / 2.f, VH * 0.25f, 46, WHITE);

        if (over_) return resultScreen(c, "Oyun bitti!", score(), best_ ? *best_ : score());
        return GameAction::None;
    }

private:
    struct Block { int lane; float y; bool done = false, hit = false; };
    static constexpr int LANES = 3;
    static constexpr float BLOCK_H = 90.f;

    void spawnRow() {
        int a = static_cast<int>(rng_() % LANES);
        blocks_.push_back({ a, -BLOCK_H });
        if (time_ > 20 && rng_() % 3 == 0) {   // 20 saniyeden sonra bazen iki serit (biri hep acik)
            int b = (a + 1 + static_cast<int>(rng_() % (LANES - 1))) % LANES;
            blocks_.push_back({ b, -BLOCK_H });
        }
    }

    int* best_;
    std::vector<Block> blocks_;
    std::mt19937 rng_{ std::random_device{}() };
    float time_ = 0, spawn_ = 0, flash_ = 0, avatarX_ = VW / 2.f;
    int lives_ = 3, dodged_ = 0, lane_ = 1;
    bool over_ = false;
};
