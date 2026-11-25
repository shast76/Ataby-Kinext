// Kinext - Kaleci (3D, Kinect Sports tarzi)
// Kamera kalecinin (senin) arkasinda. Karsida rakip oyuncu kosup sut atar; top kaleye gelince
// elinle, ayaginla, kafanla ya da govdenle degersen kurtaris. 10 sut, giderek hizlanir.
#pragma once
#include "games.hpp"
#include "avatar3d.hpp"

class KeeperGame : public Game {
public:
    explicit KeeperGame(int* best) : best_(best) {}

    void reset() override {
        shot_ = 0; saves_ = 0; streak_ = 0; over_ = false; msgTime_ = 0; flash_ = 0;
        for (bool& r : results_) r = false;
        camPos_ = { 0, 2.0f, -3.6f }; camTarget_ = { 0, 1.0f, 9.f };
        newShot();
    }
    int score() const override { return saves_; }
    bool finished() const override { return over_; }

    GameAction frame(GameContext& c) override {
        const float dt = c.dt;
        msgTime_ = (std::max)(0.f, msgTime_ - dt);
        flash_ = (std::max)(0.f, flash_ - dt);
        net_ = (std::max)(0.f, net_ - dt * 1.2f);

        // Kaleci: saga sola adim kalede genis bir alana karsilik gelir
        const bool present = c.body.valid;
        if (present) {
            float target = std::clamp(-(c.body[HipCenter].x - 0.5f) * 4.4f, -GOAL_HW + 0.5f, GOAL_HW - 0.5f);
            keeperX_ += (target - keeperX_) * (std::min)(1.f, dt * 10.f);
        }
        Vector3 root = { keeperX_, 0, KEEPER_Z };
        Body3 body = present ? toWorld(c.body, root, { -1, 0, 0 }, { 0, 0, 1 }, 0.52f)
                             : idleBody3(root, { -1, 0, 0 }, { 0, 0, 1 }, c.time, 0.52f);

        if (!over_ && (present || state_ != State::Wait)) stateTime_ += dt;
        switch (state_) {
        case State::Wait:    if (stateTime_ > waitTime_) setState(State::RunUp); break;
        case State::RunUp:   if (stateTime_ > RUN_TIME) { setState(State::Fly); } break;
        case State::Fly:     fly(body); break;
        case State::Done:
            stepLooseBall(dt);
            if (stateTime_ > 1.5f) {
                if (++shot_ >= SHOTS) { over_ = true; if (best_ && saves_ > *best_) *best_ = saves_; }
                else newShot();
            }
            break;
        }

        // kamera kalecinin arkasinda, hafifce takip eder
        Vector3 pos = { keeperX_ * 0.4f, 2.0f, -3.6f }, tgt = { keeperX_ * 0.15f, 1.0f, 9.f };
        camPos_ = Vector3Lerp(camPos_, pos, (std::min)(1.f, dt * 4.f));
        camTarget_ = Vector3Lerp(camTarget_, tgt, (std::min)(1.f, dt * 4.f));

        // ---------------- cizim ----------------
        DrawRectangleGradientV(0, 0, VW, VH / 2, { 30, 70, 150, 255 }, { 120, 170, 230, 255 });
        DrawRectangle(0, VH / 2, VW, VH / 2, { 60, 140, 70, 255 });
        Camera3D cam{ camPos_, camTarget_, { 0, 1, 0 }, 56.f, CAMERA_PERSPECTIVE };
        auto& L = r3d::light();
        L.eye = camPos_; L.sun = Vector3Normalize({ -0.4f, 1.f, -0.3f }); L.fill = Vector3Normalize({ 0.6f, 0.5f, 0.6f });
        L.ambient = 0.5f; L.sunK = 0.6f; L.fillK = 0.2f; L.specK = 0.35f; L.shininess = 20.f;
        L.fogColor = { 150, 185, 230, 255 }; L.fogStart = 25.f; L.fogEnd = 70.f;

        r3d::beginScene(cam);
        drawStadium(c.accent);
        Body3 shooter = shooterBody(c);
        drawAvatar3D(shooter, shooterAvatar(c.avatar), { 220, 50, 60, 255 }, camPos_);
        drawBall3D();
        drawAvatarShadow(body);
        drawAvatarShadow(shooter);
        r3d::shadow({ ball_.x, 0, ball_.z }, 0.13f + ball_.y * 0.05f, (std::max)(0.08f, 0.35f - ball_.y * 0.08f));
        drawNet();
        // kaleci en son ve yari saydam: arkasindaki top ve rakip gorunsun (Kinect Sports gibi)
        L.alpha = 0.6f;
        drawAvatar3D(body, c.avatar, c.accent, camPos_);
        drawGloves(body);
        L.alpha = 1.f;
        r3d::endScene();

        // ---------------- arayuz ----------------
        if (flash_ > 0) DrawRectangle(0, 0, VW, VH, ColorAlpha(RED, flash_ * 0.45f));
        DrawRectangleRounded({ VW / 2.f - 260, 12, 520, 62 }, 0.4f, 10, { 0, 0, 0, 150 });
        text(c.fonts.bold, T("Kurtarış ", "Saves ") + std::to_string(saves_), VW / 2.f - 240, 26, 30, { 255, 215, 80, 255 });
        for (int i = 0; i < SHOTS; ++i) {
            Vector2 p = { VW / 2.f - 10 + i * 24.f, 43 };
            if (i < shot_) DrawCircleV(p, 9, results_[i] ? Color{ 90, 220, 120, 255 } : Color{ 235, 70, 70, 255 });
            else DrawCircleLinesV(p, 8, { 200, 200, 210, 255 });
        }
        if (!present && !over_) waitingForPlayer(c);
        else if (shot_ == 0 && state_ == State::Wait && !over_) {
            std::string h = T("Topa elinle, ayağınla uzan, gol yeme!", "Reach for the ball with your hands or feet!");
            float w = textWidth(c.fonts.regular, h, 30) + 60;
            DrawRectangleRounded({ VW / 2.f - w / 2, VH - 92.f, w, 56 }, 0.5f, 12, { 0, 0, 0, 150 });
            textCenter(c.fonts.regular, h, VW / 2.f, VH - 80.f, 30, WHITE);
        }
        if (msgTime_ > 0) {
            float a = (std::min)(1.f, msgTime_ * 3.f), pop = 1.f + (std::max)(0.f, msgTime_ - 1.0f) * 1.5f;
            textCenterShadow(c.fonts.bold, msg_, VW / 2.f, VH * 0.3f, 100 * pop, ColorAlpha(msgColor_, a));
        }
        if (over_) return resultScreen(c, T("Kurtarış sayın", "Your saves"), saves_, best_ ? *best_ : saves_);
        return GameAction::None;
    }

private:
    enum class State { Wait, RunUp, Fly, Done };
    static constexpr int SHOTS = 10;
    static constexpr float GOAL_HW = 2.6f, GOAL_H = 2.25f, KEEPER_Z = 0.45f, SPOT_Z = 9.f, BALL_R = 0.15f, RUN_TIME = 0.9f;

    void setState(State s) { state_ = s; stateTime_ = 0; }

    void newShot() {
        setState(State::Wait);
        saved_ = false;
        float hard = shot_ / static_cast<float>(SHOTS);
        std::uniform_real_distribution<float> ux(-GOAL_HW + 0.4f, GOAL_HW - 0.4f), uy(0.2f, GOAL_H - 0.25f), u01(0, 1);
        target_ = { ux(rng_), uy(rng_), 0 };
        if (u01(rng_) < 0.25f + hard * 0.45f) {   // koseler
            target_.x = (rng_() % 2 ? 1.f : -1.f) * (GOAL_HW - 0.45f);
            target_.y = rng_() % 2 ? GOAL_H - 0.35f : 0.3f;
        }
        spot_ = { std::uniform_real_distribution<float>(-0.6f, 0.6f)(rng_), BALL_R, SPOT_Z };
        flightTime_ = 1.15f - hard * 0.4f;
        arc_ = std::uniform_real_distribution<float>(0.2f, 0.9f)(rng_);
        waitTime_ = shot_ == 0 ? 2.5f : 1.1f;
        ball_ = spot_; vel_ = {};
    }

    void fly(const Body3& body) {
        float t = std::clamp(stateTime_ / flightTime_, 0.f, 1.2f);
        Vector3 goal = { target_.x, target_.y, KEEPER_Z };
        Vector3 p = Vector3Lerp(spot_, goal, t);
        p.y += std::sin((std::min)(t, 1.f) * PI) * arc_;
        Vector3 prev = ball_;
        ball_ = p;
        spin_ += 0.4f;
        // top kalecinin hizasina gelince: herhangi bir uzva degiyor mu?
        if (t > 0.86f && t < 1.08f && touches(body)) {
            saved_ = true; ++saves_; ++streak_; results_[shot_] = true;
            Vector3 away = Vector3Subtract(ball_, body[Spine]);
            away.y = std::fabs(away.y) + 0.5f; away.z = 1.5f;
            vel_ = Vector3Scale(Vector3Normalize(away), 6.f);
            say(streak_ >= 3 ? T("SÜPER!", "SUPERB!") : T("KURTARIŞ!", "SAVE!"), { 110, 235, 140, 255 });
            setState(State::Done);
        } else if (t >= 1.08f) {
            results_[shot_] = false; streak_ = 0; net_ = 1.f; flash_ = 0.5f;
            vel_ = Vector3Scale(Vector3Subtract(ball_, prev), 1.f / GetFrameTimeSafe());
            say(T("GOL!", "GOAL!"), { 255, 90, 90, 255 });
            setState(State::Done);
        }
    }
    static float GetFrameTimeSafe() { float f = GetFrameTime(); return f > 0.001f ? f : 1 / 60.f; }

    void stepLooseBall(float dt) {
        vel_.y -= 9.8f * dt;
        ball_ = Vector3Add(ball_, Vector3Scale(vel_, dt));
        if (ball_.y < BALL_R) { ball_.y = BALL_R; vel_.y = -vel_.y * 0.4f; vel_.x *= 0.8f; vel_.z *= 0.8f; }
        if (!saved_ && ball_.z < -1.1f) { ball_.z = -1.1f; vel_.z = 0; vel_.x *= 0.3f; }   // agda kalir
        spin_ += dt * 10.f;
    }

    bool touches(const Body3& b) const {
        struct Part { Joint j; float r; };
        const Part parts[] = { { HandLeft, 0.16f }, { HandRight, 0.16f }, { WristLeft, 0.12f }, { WristRight, 0.12f },
            { ElbowLeft, 0.1f }, { ElbowRight, 0.1f }, { Head, 0.17f }, { ShoulderCenter, 0.16f }, { Spine, 0.2f },
            { HipCenter, 0.18f }, { KneeLeft, 0.11f }, { KneeRight, 0.11f }, { FootLeft, 0.13f }, { FootRight, 0.13f } };
        for (const auto& p : parts) {
            Vector3 d = Vector3Subtract(b[p.j], ball_);
            d.z *= 0.5f;   // derinlikte daha affedici
            if (Vector3Length(d) < p.r + BALL_R + 0.05f) return true;
        }
        const std::pair<Joint, Joint> bones[] = { { ElbowLeft, HandLeft }, { ElbowRight, HandRight }, { ShoulderLeft, ElbowLeft },
            { ShoulderRight, ElbowRight }, { KneeLeft, FootLeft }, { KneeRight, FootRight } };
        for (auto [a, c] : bones) {
            Vector3 A = b[a], B = b[c], AB = Vector3Subtract(B, A), AP = Vector3Subtract(ball_, A);
            float tt = std::clamp(Vector3DotProduct(AP, AB) / (std::max)(Vector3DotProduct(AB, AB), 1e-5f), 0.f, 1.f);
            Vector3 q = Vector3Add(A, Vector3Scale(AB, tt));
            if (Vector3Distance(q, ball_) < BALL_R + 0.12f) return true;
        }
        return false;
    }

    void say(const std::string& m, Color col) { msg_ = m; msgColor_ = col; msgTime_ = 1.3f; }

    static int shooterAvatar(int player) { return (std::clamp(player, 0, 2) + 1) % AVATAR_COUNT; }

    // Rakip oyuncu: bekler, kosar, vurur
    Body3 shooterBody(GameContext& c) {
        float t = 0, kick = 0;
        Vector3 root = { spot_.x + 0.35f, 0, SPOT_Z + 2.2f };
        if (state_ == State::RunUp) {
            t = stateTime_ / RUN_TIME;
            root = Vector3Lerp({ spot_.x + 0.9f, 0, SPOT_Z + 3.2f }, { spot_.x + 0.3f, 0, SPOT_Z + 0.35f }, t);
            kick = std::clamp((t - 0.7f) / 0.3f, 0.f, 1.f);
        } else if (state_ == State::Fly || state_ == State::Done) {
            root = { spot_.x + 0.3f, 0, SPOT_Z + 0.35f };
            kick = std::clamp(1.f - stateTime_ * 2.f, 0.f, 1.f);
        } else root.z += std::sin(c.time * 2.f) * 0.05f;
        Skeleton n = idleBody(0.5f * VW, 0.55f * VH, 100.f, c.time * (state_ == State::RunUp ? 6.f : 1.f));
        // kollar yanda, kosarken sallanir
        float swing = state_ == State::RunUp ? std::sin(t * 18.f) * 0.3f : 0.f;
        auto put = [&](Joint j, float x, float y) { n.joints[j] = { 0.5f * VW + x * 100.f, 0.55f * VH + y * 100.f, TrackState::Tracked }; };
        put(ElbowLeft, -0.5f, -0.5f + swing); put(WristLeft, -0.52f, -0.08f + swing); put(HandLeft, -0.53f, 0.04f + swing);
        put(ElbowRight, 0.5f, -0.5f - swing); put(WristRight, 0.52f, -0.08f - swing); put(HandRight, 0.53f, 0.04f - swing);
        // vuran ayak (sag) kalkar
        put(KneeRight, 0.25f, 0.62f - kick * 0.35f); put(AnkleRight, 0.26f + kick * 0.05f, 1.2f - kick * 0.75f); put(FootRight, 0.31f, 1.3f - kick * 0.8f);
        for (auto& j : n.joints) { j.x /= VW; j.y /= VH; }
        Body3 b = toWorld(n, root, { -1, 0, 0 }, { 0, 0, -1 }, 0.5f);
        for (Joint j : { KneeRight, AnkleRight, FootRight }) b.j[j] = Vector3Add(b.j[j], { 0, 0, -kick * 0.35f });
        return b;
    }

    void drawBall3D() {
        Matrix rot = MatrixMultiply(MatrixRotateX(spin_), MatrixRotateY(spin_ * 0.3f));
        r3d::draw(r3d::sphere(), r3d::trs(ball_, { BALL_R, BALL_R, BALL_R }, rot), { 248, 248, 248, 255 }, 0.8f);
        for (int i = 0; i < 6; ++i) {   // siyah yamalar
            float a = i * 1.047f;
            Vector3 o = Vector3Transform({ std::cos(a) * 0.8f, (i % 2 ? 0.55f : -0.55f), std::sin(a) * 0.8f }, rot);
            r3d::ballLo(Vector3Add(ball_, Vector3Scale(Vector3Normalize(o), BALL_R * 0.86f)), BALL_R * 0.3f, { 25, 25, 30, 255 });
        }
    }

    void drawGloves(const Body3& b) {
        if (!b.valid) return;
        for (Joint h : { HandLeft, HandRight }) {
            r3d::ball(b[h], b.torso * 0.19f, { 250, 205, 50, 255 }, 0.6f);
            r3d::ball(Vector3Add(b[h], Vector3Scale(b.fwd, b.torso * 0.12f)), b.torso * 0.12f, { 255, 240, 170, 255 });
        }
    }

    void drawStadium(Color accent) {
        // cim seritleri
        for (int i = -2; i < 12; ++i)
            r3d::ground(-16.f, i * 2.5f, 16.f, i * 2.5f + 2.5f, 0.f, (i % 2) ? Color{ 64, 150, 70, 255 } : Color{ 74, 166, 80, 255 });
        // saha cizgileri
        auto line = [&](Vector3 a, Vector3 b, float w) {
            Vector3 d = Vector3Normalize(Vector3Subtract(b, a)), s = Vector3Scale(Vector3CrossProduct(d, { 0, 1, 0 }), w);
            r3d::flatQuad(Vector3Add(a, s), Vector3Add(b, s), Vector3Subtract(b, s), Vector3Subtract(a, s), { 245, 245, 245, 230 });
        };
        const float y = 0.004f;
        line({ -16, y, 0 }, { 16, y, 0 }, 0.06f);                         // kale cizgisi
        line({ -4.5f, y, 0 }, { -4.5f, y, 3.2f }, 0.06f); line({ 4.5f, y, 0 }, { 4.5f, y, 3.2f }, 0.06f); line({ -4.5f, y, 3.2f }, { 4.5f, y, 3.2f }, 0.06f);
        line({ -10, y, 0 }, { -10, y, 12.5f }, 0.06f); line({ 10, y, 0 }, { 10, y, 12.5f }, 0.06f); line({ -10, y, 12.5f }, { 10, y, 12.5f }, 0.06f);
        for (int i = 0; i < 16; ++i) {   // ceza sahasi yayi
            float a0 = PI * (0.2f + 0.6f * i / 16.f), a1 = PI * (0.2f + 0.6f * (i + 1) / 16.f);
            line({ std::cos(a0) * 3.f, y, SPOT_Z + std::sin(a0) * 3.f }, { std::cos(a1) * 3.f, y, SPOT_Z + std::sin(a1) * 3.f }, 0.05f);
        }
        r3d::shadow({ 0, 0, SPOT_Z }, 0.12f, 0.9f);
        // tribunler: basamaklar ve renkli seyirciler
        for (int side = 0; side < 3; ++side) {
            for (int row = 0; row < 6; ++row) {
                Vector3 c, s;
                if (side == 0) { c = { 0, 0.6f + row * 0.7f, 26.f + row * 1.2f }; s = { 44, 0.7f, 1.2f }; }
                else { float sx = side == 1 ? -1.f : 1.f; c = { sx * (17.f + row * 1.2f), 0.6f + row * 0.7f, 10.f }; s = { 1.2f, 0.7f, 34.f }; }
                r3d::cube(c, s, row % 2 ? Color{ 60, 62, 78, 255 } : Color{ 70, 72, 90, 255 });
            }
        }
        std::mt19937 r(7);
        const Color crowd[] = { accent, { 230, 60, 60, 255 }, { 250, 210, 70, 255 }, { 240, 240, 245, 255 }, { 60, 120, 220, 255 } };
        for (int i = 0; i < 260; ++i) {
            int row = r() % 6, side = r() % 3;
            float u = std::uniform_real_distribution<float>(-1, 1)(r);
            Vector3 p = side == 0 ? Vector3{ u * 21.f, 1.05f + row * 0.7f, 26.f + row * 1.2f }
                                  : Vector3{ (side == 1 ? -1.f : 1.f) * (17.f + row * 1.2f), 1.05f + row * 0.7f, 10.f + u * 16.f };
            r3d::cube(p, { 0.35f, 0.45f, 0.35f }, crowd[r() % 5]);
        }
        // isik direkleri
        for (float sx : { -1.f, 1.f }) {
            r3d::rod({ sx * 20.f, 0, 30.f }, { sx * 20.f, 14.f, 30.f }, 0.25f, { 90, 92, 100, 255 });
            r3d::cube({ sx * 20.f, 14.5f, 30.f }, { 4.f, 1.5f, 0.4f }, { 250, 250, 235, 255 });
        }
        // kale direkleri
        Color post = { 250, 250, 250, 255 };
        r3d::rod({ -GOAL_HW, 0, 0 }, { -GOAL_HW, GOAL_H, 0 }, 0.06f, post, 0.6f);
        r3d::rod({ GOAL_HW, 0, 0 }, { GOAL_HW, GOAL_H, 0 }, 0.06f, post, 0.6f);
        r3d::rod({ -GOAL_HW - 0.06f, GOAL_H, 0 }, { GOAL_HW + 0.06f, GOAL_H, 0 }, 0.06f, post, 0.6f);
        // reklam panolari
        for (int i = -4; i < 4; ++i)
            r3d::cube({ i * 4.f + 2.f, 0.45f, 15.5f }, { 3.9f, 0.9f, 0.1f }, i % 2 ? accent : Color{ 30, 32, 44, 255 }, 0, 0.3f);
    }

    // Ag: kalecinin arkasinda; gol olunca disari dogru siser
    void drawNet() {
        const float depth = -1.2f, bulge = net_ * 0.35f;
        Color nc = { 255, 255, 255, 38 };
        auto back = [&](float x, float y) {
            float k = (1.f - std::pow(x / GOAL_HW, 2.f)) * (1.f - std::pow(y / GOAL_H - 0.5f, 2.f) * 2.f);
            return Vector3{ x, y, depth - bulge * (std::max)(0.f, k) };
        };
        for (float x = -GOAL_HW; x <= GOAL_HW + 0.01f; x += 0.25f)
            for (float y = 0; y < GOAL_H; y += 0.25f) DrawLine3D(back(x, y), back(x, (std::min)(y + 0.25f, GOAL_H)), nc);
        for (float y = 0; y <= GOAL_H + 0.01f; y += 0.25f)
            for (float x = -GOAL_HW; x < GOAL_HW; x += 0.25f) DrawLine3D(back(x, y), back((std::min)(x + 0.25f, GOAL_HW), y), nc);
        for (float x = -GOAL_HW; x <= GOAL_HW + 0.01f; x += 0.25f) DrawLine3D({ x, GOAL_H, 0 }, back(x, GOAL_H), nc);   // ust ag
        for (float s : { -1.f, 1.f })
            for (float y = 0; y <= GOAL_H + 0.01f; y += 0.25f) DrawLine3D({ s * GOAL_HW, y, 0 }, back(s * GOAL_HW, y), nc);   // yan aglar
    }

    int* best_;
    std::mt19937 rng_{ std::random_device{}() };
    State state_ = State::Wait;
    Vector3 spot_{}, target_{}, ball_{}, vel_{}, camPos_{}, camTarget_{};
    float stateTime_ = 0, waitTime_ = 1.f, flightTime_ = 1.f, arc_ = 0.5f, spin_ = 0;
    float keeperX_ = 0, msgTime_ = 0, flash_ = 0, net_ = 0;
    int shot_ = 0, saves_ = 0, streak_ = 0;
    bool over_ = false, saved_ = false;
    bool results_[SHOTS] = {};
    std::string msg_;
    Color msgColor_ = WHITE;
};
