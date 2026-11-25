// Kinext - Engelden Kac (3D): gece yolunda karsidan gelen bloklardan saga sola kayarak kac (3 can)
// Kamera oyuncunun arkasinda; bloklar uzaktan buyuyerek gelir.
#pragma once
#include "games.hpp"
#include "avatar3d.hpp"

class DodgeGame : public Game {
public:
    explicit DodgeGame(int* best) : best_(best) {}

    void reset() override {
        blocks_.clear();
        time_ = 0; spawn_ = 1.2f; lives_ = 3; dodged_ = 0; over_ = false; flash_ = 0; travel_ = 0;
    }
    int score() const override { return dodged_ * 10 + static_cast<int>(time_); }
    bool finished() const override { return over_; }

    GameAction frame(GameContext& c) override {
        const float dt = c.dt;
        const bool present = c.body.valid;
        // Serit: kalcanin yatay konumu (ekranda sag = dunyada -x)
        if (present && c.body[HipCenter].state != TrackState::NotTracked) {
            lane_ = std::clamp(static_cast<int>(c.body[HipCenter].x * LANES), 0, LANES - 1);   // 0: ekranin solu
        }
        float targetX = laneX(lane_);
        px_ += (targetX - px_) * (std::min)(1.f, dt * 10.f);
        Vector3 root = { px_, 0, 0 };
        // maskot yolun ilerisine bakar; kalcanin yatay hareketi zaten seridi belirledigi icin govde merkezde
        Skeleton centered = c.body;
        if (present) { float dx = 0.5f - c.body[HipCenter].x; for (auto& j : centered.joints) j.x += dx; }
        Body3 body = present ? toWorld(centered, root, { -1, 0, 0 }, { 0, 0, 1 }, 0.48f)
                             : idleBody3(root, { -1, 0, 0 }, { 0, 0, 1 }, c.time, 0.48f);

        float speed = 9.f + 0.35f * time_;
        if (!over_ && present) {
            time_ += dt;
            travel_ += speed * dt;
            spawn_ -= dt;
            if (spawn_ <= 0) { spawnRow(); spawn_ = (std::max)(0.55f, 1.4f - time_ * 0.02f); }
            for (auto& b : blocks_) {
                float before = b.z;
                b.z -= speed * dt;
                bool crossing = before > 0.4f && b.z <= 0.4f;
                if (crossing && !b.done) {
                    b.done = true;
                    if (b.lane == lane_) { b.hit = true; --lives_; flash_ = 0.5f; shake_ = 0.4f; }
                    else ++dodged_;
                }
            }
            blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(), [](const Block& b) { return b.z < -6.f; }), blocks_.end());
            if (lives_ <= 0) { over_ = true; if (best_ && score() > *best_) *best_ = score(); }
        }
        flash_ = (std::max)(0.f, flash_ - dt);
        shake_ = (std::max)(0.f, shake_ - dt);

        // ---------------- cizim ----------------
        DrawRectangleGradientV(0, 0, VW, VH, { 10, 10, 30, 255 }, { 50, 28, 80, 255 });
        for (int i = 0; i < 70; ++i) {   // yildizlar
            float x = std::fmod(i * 197.f, static_cast<float>(VW)), y = std::fmod(i * 131.f, VH * 0.45f);
            DrawCircleV({ x, y }, 1.2f + (i % 3) * 0.5f, { 255, 255, 255, static_cast<unsigned char>(90 + 60 * std::sin(c.time * 2 + i)) });
        }
        float sh = shake_ > 0 ? std::sin(c.time * 60.f) * shake_ * 0.15f : 0.f;
        Vector3 eye = { px_ * 0.5f + sh, 2.1f, -3.4f };
        Camera3D cam{ eye, { px_ * 0.3f, 0.9f, 8.f }, { 0, 1, 0 }, 58.f, CAMERA_PERSPECTIVE };
        auto& L = r3d::light();
        L.eye = eye; L.sun = Vector3Normalize({ 0.2f, 1.f, -0.4f }); L.fill = Vector3Normalize({ 0.f, 0.3f, 1.f });
        L.ambient = 0.45f; L.sunK = 0.5f; L.fillK = 0.35f; L.specK = 0.7f; L.shininess = 30.f;
        L.fogColor = { 40, 24, 70, 255 }; L.fogStart = 15.f; L.fogEnd = 55.f;

        r3d::beginScene(cam);
        drawRoad(c.accent);
        for (const auto& b : blocks_) {
            Vector3 p = { laneX(b.lane), 0.5f, b.z };
            Color col = b.hit ? Color{ 120, 120, 130, 255 } : Color{ 235, 60, 75, 255 };
            r3d::cube(p, { 0.86f, 1.0f, 0.6f }, col, 0, 0.8f);
            r3d::cube({ p.x, 1.03f, p.z }, { 0.9f, 0.06f, 0.64f }, b.hit ? col : Color{ 255, 220, 90, 255 }, 0, 0.8f);   // uyari seridi
        }
        drawAvatar3D(body, c.avatar, c.accent, eye);
        drawAvatarShadow(body);
        for (const auto& b : blocks_) r3d::shadow({ laneX(b.lane), 0, b.z }, 0.55f, 0.3f);
        r3d::endScene();

        if (flash_ > 0) DrawRectangle(0, 0, VW, VH, ColorAlpha(RED, flash_ * 0.7f));
        std::string hearts;
        for (int i = 0; i < 3; ++i) hearts += i < lives_ ? "● " : "○ ";
        topBar(c, T("Puan ", "Score ") + std::to_string(score()), "", T("Can ", "Lives ") + hearts);
        if (!present && !over_) waitingForPlayer(c);
        else if (time_ < 3.f && !over_) textCenterShadow(c.fonts.light, T("Sağa sola kayarak kaç!", "Lean left and right to dodge!"), VW / 2.f, VH * 0.2f, 46, WHITE);
        if (over_) return resultScreen(c, T("Oyun bitti!", "Game over!"), score(), best_ ? *best_ : score());
        return GameAction::None;
    }

private:
    struct Block { int lane; float z; bool done = false, hit = false; };
    static constexpr int LANES = 3;
    static float laneX(int lane) { return (1 - lane) * 1.1f; }   // 0: sol (ekran), 2: sag

    void spawnRow() {
        int a = static_cast<int>(rng_() % LANES);
        blocks_.push_back({ a, 45.f });
        if (time_ > 20 && rng_() % 3 == 0) {   // 20 saniyeden sonra bazen iki serit (biri hep acik)
            int b = (a + 1 + static_cast<int>(rng_() % (LANES - 1))) % LANES;
            blocks_.push_back({ b, 45.f });
        }
    }

    void drawRoad(Color accent) {
        r3d::ground(-40, -10, 40, 80, -0.02f, { 26, 20, 44, 255 });
        r3d::ground(-1.75f, -10, 1.75f, 80, 0.f, { 46, 44, 62, 255 }, 0.5f);
        // seritin isigi
        r3d::flatQuad({ laneX(lane_) - 0.55f, 0.003f, -1.5f }, { laneX(lane_) + 0.55f, 0.003f, -1.5f },
                      { laneX(lane_) + 0.55f, 0.003f, 30.f }, { laneX(lane_) - 0.55f, 0.003f, 30.f }, ColorAlpha(accent, 0.16f));
        // kayan serit cizgileri
        float off = std::fmod(travel_, 3.f);
        for (float x : { -0.55f, 0.55f })
            for (float z = -6.f - off; z < 70.f; z += 3.f)
                r3d::flatQuad({ x - 0.04f, 0.004f, z }, { x + 0.04f, 0.004f, z }, { x + 0.04f, 0.004f, z + 1.4f }, { x - 0.04f, 0.004f, z + 1.4f }, { 230, 230, 240, 200 });
        for (float x : { -1.72f, 1.72f }) r3d::flatQuad({ x - 0.05f, 0.004f, -10 }, { x + 0.05f, 0.004f, -10 }, { x + 0.05f, 0.004f, 80 }, { x - 0.05f, 0.004f, 80 }, accent);
        // yanlardan gecen neon direkler
        float poff = std::fmod(travel_, 6.f);
        for (float z = -6.f - poff; z < 70.f; z += 6.f)
            for (float sx : { -1.f, 1.f }) {
                r3d::cube({ sx * 2.6f, 1.2f, z }, { 0.25f, 2.4f, 0.25f }, { 60, 56, 90, 255 }, 0, 0.6f);
                r3d::cube({ sx * 2.6f, 2.45f, z }, { 0.35f, 0.12f, 0.35f }, accent, 0, 1.f);
            }
    }

    int* best_;
    std::vector<Block> blocks_;
    std::mt19937 rng_{ std::random_device{}() };
    float time_ = 0, spawn_ = 0, flash_ = 0, shake_ = 0, px_ = 0, travel_ = 0;
    int lives_ = 3, dodged_ = 0, lane_ = 1;
    bool over_ = false;
};
