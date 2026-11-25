// Kinext - Bowling (3D, Kinect Sports tarzi)
//  0) El secimi: ayarlardaki kontrol eli; Otomatik ise oyun basinda kaldirdigin el
//  1) Topu al: elini yana uzat (top makinesi secilen elin tarafinda)
//  2) Nisan: saga sola adim at, oyuncu seritte o yere gecer
//  3) At: topu tutan kolu geriden one, asagidan yukari savur. Savurma hizi = top hizi,
//     savururken elin yana kaymasi = topun acisi ve falsosu
//  Gercek olculer (18.3 m serit, 10 lobut), carpismali lobut fizigi ve gercek bowling puanlamasi
#pragma once
#include "games.hpp"
#include "avatar3d.hpp"
#include <array>

class BowlingGame : public Game {
public:
    explicit BowlingGame(int* best) : best_(best) {}

    void reset() override {
        rolls_.clear(); frame_ = 0; rollInFrame_ = 0; over_ = false;
        for (auto& f : frameRolls_) f[0] = f[1] = f[2] = 0;
        for (int& n : rollCount_) n = 0;
        msgTime_ = 0; hist_.clear(); chooseTime_[0] = chooseTime_[1] = 0;
        rack(true); setState(hand_ < 0 ? State::ChooseHand : State::Pickup);
        camPos_ = { 0, 1.75f, -4.3f }; camTarget_ = { 0, 0.7f, 7.f };
    }
    int score() const override { return totalScore(); }
    bool finished() const override { return over_; }

    // Klavye ve test: topu al / at
    void grab(int hand) { if (state_ == State::Pickup) { holdHand_ = hand; setState(State::Hold); hist_.clear(); } }
    void chooseHand(int hand) { hand_ = hand; if (state_ == State::ChooseHand) setState(State::Pickup); }
    void release(Vector3 from, float speed, float angle, float hook) {
        if (state_ != State::Hold) return;
        ball_.p = { from.x, (std::max)(from.y, BALL_R), (std::max)(from.z, 0.05f) };
        speed = std::clamp(speed, 5.5f, 10.5f);
        ball_.v = { std::sin(angle) * speed, 0.f, std::cos(angle) * speed };
        ball_.hook = std::clamp(hook, -1.f, 1.f);
        ball_.gutter = false; ball_.spin = 0;
        setState(State::Rolling);
    }

    GameAction frame(GameContext& c) override {
        const float dt = c.dt;
        stateTime_ += dt;
        msgTime_ = (std::max)(0.f, msgTime_ - dt);
        const bool present = c.body.valid;
        if (c.hand != 0 && hand_ != (c.hand == 1 ? 0 : 1)) chooseHand(c.hand == 1 ? 0 : 1);   // ayarlarda sabit el
        if (c.hand == 0 && handFromSettings_) { hand_ = -1; }
        handFromSettings_ = c.hand != 0;
        if (hand_ < 0 && state_ == State::Pickup) setState(State::ChooseHand);

        // --- oyuncunun yeri (nisan): kalcanin yatay konumu ---
        if (present && (state_ == State::Pickup || state_ == State::Hold || state_ == State::ChooseHand)) {
            float target = std::clamp(-(c.body[HipCenter].x - 0.5f) * 2.2f, -0.42f, 0.42f);
            playerX_ += (target - playerX_) * (std::min)(1.f, dt * 6.f);
        }
        Vector3 root = { playerX_, 0, -1.1f };
        Body3 body = present ? toWorld(c.body, root, { -1, 0, 0 }, { 0, 0, 1 }, 0.5f)
                             : idleBody3(root, { -1, 0, 0 }, { 0, 0, 1 }, c.time);

        // --- girdiler ---
        if (!over_) {
            if (state_ == State::ChooseHand) {
                if (present) detectHandChoice(c);
                if (IsKeyPressed(KEY_L)) chooseHand(0);
                if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE)) chooseHand(1);
            } else if (state_ == State::Pickup && stateTime_ > 0.4f) {
                if (present) detectGrab(c);
                if (IsKeyPressed(KEY_SPACE)) grab(hand_ < 0 ? 1 : hand_);
            } else if (state_ == State::Hold) {
                if (present && stateTime_ > 0.3f) detectSwing(c, body);
                if (IsKeyPressed(KEY_SPACE)) release(handPos(body), 8.f, 0.f, 0.f);
            }
        }
        if (state_ == State::Rolling || state_ == State::Settle) physics(dt);
        if (state_ == State::Rolling && (ball_.p.z > PIT_Z + 0.5f || stateTime_ > 7.f)) setState(State::Settle);
        if (state_ == State::Settle && stateTime_ > 1.6f) scoreRoll();
        if (state_ == State::Result && stateTime_ > 1.5f) nextRoll();

        updateCamera(dt, body);

        // ---------------- cizim ----------------
        DrawRectangleGradientV(0, 0, VW, VH, { 14, 10, 26, 255 }, { 34, 22, 48, 255 });
        Camera3D cam{ camPos_, camTarget_, { 0, 1, 0 }, 50.f, CAMERA_PERSPECTIVE };
        auto& L = r3d::light();
        L.eye = camPos_; L.sun = Vector3Normalize({ 0.3f, 1.f, -0.6f }); L.fill = Vector3Normalize({ -0.5f, 0.6f, 0.8f });
        L.ambient = 0.52f; L.sunK = 0.6f; L.fillK = 0.25f; L.specK = 0.5f; L.shininess = 30.f;
        L.fogColor = { 20, 14, 32, 255 }; L.fogStart = 14.f; L.fogEnd = 40.f;

        r3d::beginScene(cam);
        drawAlley(c.accent, c.time);
        for (const auto& p : pins_) if (!p.gone) drawPin(p);
        if (state_ == State::Hold) r3d::ball(handPos(body), BALL_R, ballColor(c.accent), 1.f);
        if (state_ == State::Rolling || state_ == State::Settle) drawBall(c.accent);
        drawAvatar3D(body, c.avatar, c.accent, camPos_);
        // golgeler (opak cizimlerden sonra)
        drawAvatarShadow(body);
        for (const auto& p : pins_) if (!p.gone) r3d::shadow({ p.x, 0, p.z }, 0.09f, 0.25f);
        if ((state_ == State::Rolling || state_ == State::Settle) && !ball_.gutter) r3d::shadow({ ball_.p.x, 0, ball_.p.z }, 0.14f, 0.35f);
        r3d::endScene();

        // ---------------- arayuz ----------------
        drawScoreboard(c);
        std::string hint;
        if (!present && !over_) waitingForPlayer(c);
        else if (state_ == State::ChooseHand) drawHandChoice(c);
        else if (state_ == State::Pickup) hint = hand_ == 0 ? T("Topu almak için sol elini yana uzat", "Reach out with your left hand to pick up the ball")
                                     : T("Topu almak için sağ elini yana uzat", "Reach out with your right hand to pick up the ball");
        else if (state_ == State::Hold && stateTime_ < 6.f) hint = T("Kolunu geriden öne savur, topu bırak!", "Swing your arm back and forward to bowl!");
        if (!hint.empty() && !over_) {
            float w = textWidth(c.fonts.regular, hint, 30) + 60;
            DrawRectangleRounded({ VW / 2.f - w / 2, VH - 92.f, w, 56 }, 0.5f, 12, { 0, 0, 0, 150 });
            textCenter(c.fonts.regular, hint, VW / 2.f, VH - 80.f, 30, WHITE);
        }
        if (msgTime_ > 0) {
            float a = (std::min)(1.f, msgTime_ * 3.f), pop = 1.f + (std::max)(0.f, msgTime_ - 1.25f) * 1.5f;
            textCenterShadow(c.fonts.bold, msg_, VW / 2.f, VH * 0.36f, 96 * pop, ColorAlpha(msgColor_, a));
        }
        if (over_) return resultScreen(c, T("Oyun bitti!", "Game over!"), totalScore(), best_ ? *best_ : totalScore());
        return GameAction::None;
    }

private:
    enum class State { ChooseHand, Pickup, Hold, Rolling, Settle, Result };
    struct Pin { float x, z, vx = 0, vz = 0, tilt = 0, w = 0; Vector2 dir{ 0, 1 }; bool moving = false, gone = false, counted = false; };
    struct BallS { Vector3 p{}, v{}; float hook = 0, spin = 0; bool gutter = false; };
    struct Sample { float t, y, x; };

    // gercek olculer (metre)
    static constexpr float LANE_HW = 0.53f, GUTTER_W = 0.24f, HEAD_Z = 18.29f, PIT_Z = 19.3f;
    static constexpr float BALL_R = 0.109f, PIN_R = 0.06f, PIN_H = 0.381f;
    static constexpr float BALL_M = 6.5f, PIN_M = 1.5f;
    static constexpr int FRAMES = 5;
    static constexpr float ASPECT = static_cast<float>(VW) / VH;   // 0-1 koordinatlarda yatay mesafe duzeltmesi

    void setState(State s) { state_ = s; stateTime_ = 0; }

    void rack(bool full) {
        if (full) {
            int k = 0;
            for (int row = 0; row < 4; ++row)
                for (int i = 0; i <= row; ++i) pins_[k++] = { (i - row / 2.f) * 0.305f, HEAD_Z + row * 0.264f };
            return;
        }
        // ikinci atis: devrilenler toplanir, ayaktakiler yerinde kalir
        for (auto& p : pins_) {
            if (p.tilt > 0.35f || p.gone) p.gone = true;
            else { p.vx = p.vz = 0; p.tilt = 0; p.w = 0; p.moving = false; }
        }
    }

    // --- girdi algilama (0-1 aynali iskelet) ---
    void detectGrab(GameContext& c) {
        const Skeleton& b = c.body;
        float t = (std::max)(b.torso(), 0.01f);
        const Joint wr[2] = { WristLeft, WristRight }, sh[2] = { ShoulderLeft, ShoulderRight };
        for (int h = 0; h < 2; ++h) {
            if (h != hand_ || b[wr[h]].state == TrackState::NotTracked) continue;   // sadece secilen el
            float out = (b[wr[h]].x - b[sh[h]].x) * ASPECT / t * (h == 0 ? -1.f : 1.f);   // disari dogru
            float down = (b[wr[h]].y - b[sh[h]].y) / t;
            if (out > 0.85f && down < 1.0f) { grab(h); return; }
        }
    }

    // Topu tutan el kalca hizasinin altindan bel/gogus hizasina hizla kalkinca birakilir
    void detectSwing(GameContext& c, const Body3& body) {
        const Skeleton& b = c.body;
        float t = (std::max)(b.torso(), 0.01f);
        Joint wr = holdHand_ ? WristRight : WristLeft, sh = holdHand_ ? ShoulderRight : ShoulderLeft;
        if (b[wr].state == TrackState::NotTracked) return;
        float y = (b[wr].y - b[sh].y) / t, x = (b[wr].x - b[sh].x) * ASPECT / t;
        hist_.push_back({ c.time, y, x });
        while (!hist_.empty() && c.time - hist_.front().t > 1.0f) hist_.erase(hist_.begin());
        if (y > 0.6f) return;   // el henuz one/yukari gelmedi
        for (auto it = hist_.rbegin(); it != hist_.rend(); ++it) {
            if (it->y < 0.9f) continue;   // savurmanin en alt noktasi (kalca alti)
            float dtv = (std::max)(c.time - it->t, 0.05f);
            float up = (it->y - y) / dtv;
            if (up < 1.3f) break;   // yavas kaldirma: atis degil
            float speed = 5.8f + 4.5f * std::clamp((up - 1.3f) / 5.f, 0.f, 1.f);
            // el ekranda saga kaydiysa dunyada -x: top o yone acilanir (en fazla ~4 derece)
            float angle = std::clamp(-(x - it->x) * 0.05f, -0.07f, 0.07f);
            float hook = std::clamp(-(x - it->x) * 0.6f, -0.6f, 0.6f);
            release(handPos(body), speed, angle, hook);
            hist_.clear();
            return;
        }
    }

    // Hangi el: basin ustune kaldirilip 0.6 sn tutulan el secilir
    void detectHandChoice(GameContext& c) {
        const Skeleton& b = c.body;
        const Joint wr[2] = { WristLeft, WristRight };
        for (int h = 0; h < 2; ++h) {
            bool up = b[wr[h]].state != TrackState::NotTracked && b[wr[h]].y < b[Head].y;
            chooseTime_[h] = up ? chooseTime_[h] + c.dt : 0.f;
            if (chooseTime_[h] > 0.6f) { chooseHand(h); toastHand_ = 1.2f; return; }
        }
    }
    void drawHandChoice(GameContext& c) {
        textCenterShadow(c.fonts.light, T("Hangi elinle oynayacaksın?", "Which hand will you bowl with?"), VW / 2.f, 150, 50, WHITE);
        textCenter(c.fonts.regular, T("O elini başının üstüne kaldır", "Raise that hand above your head"), VW / 2.f, 214, 26, { 220, 220, 230, 255 });
        for (int h = 0; h < 2; ++h) {
            // ekranin solu = oyuncunun sol eli (arkadan bakiyoruz)
            Rectangle r = { h == 0 ? VW / 2.f - 330 : VW / 2.f + 30, 270, 300, 200 };
            float k = std::clamp(chooseTime_[h] / 0.6f, 0.f, 1.f);
            DrawRectangleRounded(r, 0.12f, 10, ColorAlpha(k > 0 ? c.accent : Color{ 30, 30, 40, 255 }, 0.85f));
            drawIcon(Icon::Hand, { r.x + r.width / 2, r.y + 80 }, 70, WHITE);
            textCenter(c.fonts.bold, h == 0 ? T("Sol el", "Left hand") : T("Sağ el", "Right hand"), r.x + r.width / 2, r.y + 140, 32, WHITE);
            if (k > 0) DrawRectangleRec({ r.x + 20, r.y + r.height - 16, (r.width - 40) * k, 6 }, WHITE);
        }
        textCenter(c.fonts.regular, T("Ayarlar → Kontrol eli ile sabitleyebilirsin", "You can fix it in Settings → Control hand"), VW / 2.f, 500, 20, { 190, 190, 200, 255 });
    }
    // top makinesi secilen elin tarafinda (sol el: dunyada +x, ekranin solu)
    float machineX() const { return hand_ == 0 ? LANE_HW + 0.72f : -LANE_HW - 0.72f; }

    Vector3 handPos(const Body3& b) const {
        Vector3 h = b[holdHand_ ? HandRight : HandLeft];
        return { h.x, h.y - 0.06f, h.z + 0.05f };
    }

    // --- fizik ---
    void physics(float dt) {
        const int steps = 4;
        float h = dt / steps;
        for (int s = 0; s < steps; ++s) { stepBall(h); stepPins(h); }
    }

    void stepBall(float h) {
        BallS& b = ball_;
        if (b.p.z > PIT_Z + 1.f) return;
        b.v.y -= 9.8f * h;
        b.p = Vector3Add(b.p, Vector3Scale(b.v, h));
        float floor = b.p.z > PIT_Z ? -1.f : (b.gutter ? BALL_R - 0.06f : BALL_R);   // cukura duser
        if (b.p.y < floor) { b.p.y = floor; if (b.v.y < 0) b.v.y = -b.v.y * 0.25f; if (b.v.y < 0.3f) b.v.y = 0; }
        if (!b.gutter) {
            if (b.p.z > 11.f && b.p.z < HEAD_Z) b.v.x += b.hook * 0.9f * h;   // kuru bolgede falso tutar
            if (std::fabs(b.p.x) > LANE_HW) {   // oluga dustu
                b.gutter = true; b.v.x = 0;
                b.p.x = std::copysign(LANE_HW + GUTTER_W * 0.5f, b.p.x);
            }
        }
        b.v.z *= std::exp(-0.03f * h);
        b.spin += std::hypot(b.v.x, b.v.z) / BALL_R * h;
        if (b.gutter) return;
        // top - lobut
        for (auto& p : pins_) {
            if (p.gone || b.p.y > PIN_H + BALL_R) continue;
            Vector2 c = pinCenter(p);
            float dx = c.x - b.p.x, dz = c.y - b.p.z, d = std::sqrt(dx * dx + dz * dz);
            if (d >= BALL_R + PIN_R || d < 1e-5f) continue;
            float nx = dx / d, nz = dz / d;
            float vr = (b.v.x - p.vx) * nx + (b.v.z - p.vz) * nz;
            if (vr <= 0) continue;
            float j = (1.f + 0.55f) * vr / (1.f / BALL_M + 1.f / PIN_M);
            b.v.x -= j / BALL_M * nx; b.v.z -= j / BALL_M * nz;
            p.vx += j / PIN_M * nx;   p.vz += j / PIN_M * nz;
            float over = BALL_R + PIN_R - d;
            p.x += nx * over; p.z += nz * over;
            knock(p);
        }
    }

    Vector2 pinCenter(const Pin& p) const {
        float off = std::sin(p.tilt) * PIN_H * 0.45f;   // devrilirken govdesi o yone kayar
        return { p.x + p.dir.x * off, p.z + p.dir.y * off };
    }

    void knock(Pin& p) {
        float sp = std::hypot(p.vx, p.vz);
        if (sp > 0.25f && !p.moving) { p.dir = { p.vx / sp, p.vz / sp }; p.w = (std::max)(p.w, sp * 4.f); p.moving = true; }
    }

    void stepPins(float h) {
        for (auto& p : pins_) {
            if (p.gone) continue;
            p.x += p.vx * h; p.z += p.vz * h;
            float fr = std::exp(-(p.tilt > 1.2f ? 3.5f : 1.8f) * h);
            p.vx *= fr; p.vz *= fr;
            if (p.moving && p.tilt < PI / 2) {
                p.w += 9.8f / (PIN_H * 0.6f) * std::sin((std::max)(p.tilt, 0.05f)) * h;   // yercekimi devirmeyi hizlandirir
                p.tilt = (std::min)(PI / 2, p.tilt + p.w * h);
                if (p.tilt >= PI / 2) p.w = 0;
            }
            if (std::fabs(p.x) > LANE_HW + 0.35f || p.z > PIT_Z || p.z < HEAD_Z - 1.5f) p.gone = true;
        }
        // lobut - lobut
        for (size_t i = 0; i < pins_.size(); ++i)
            for (size_t k = i + 1; k < pins_.size(); ++k) {
                Pin &a = pins_[i], &b = pins_[k];
                if (a.gone || b.gone) continue;
                Vector2 ca = pinCenter(a), cb = pinCenter(b);
                float ra = a.tilt > 1.f ? 0.075f : PIN_R, rb = b.tilt > 1.f ? 0.075f : PIN_R;
                float dx = cb.x - ca.x, dz = cb.y - ca.y, d = std::sqrt(dx * dx + dz * dz);
                if (d >= ra + rb || d < 1e-5f) continue;
                float nx = dx / d, nz = dz / d;
                float vr = (a.vx - b.vx) * nx + (a.vz - b.vz) * nz;
                // devrilen lobutun ust kismi da carpar: donme hizi itme ekler
                float swing = (a.moving ? a.w * PIN_H * 0.5f * (a.dir.x * nx + a.dir.y * nz) : 0.f)
                            - (b.moving ? b.w * PIN_H * 0.5f * (b.dir.x * nx + b.dir.y * nz) : 0.f);
                vr += (std::max)(0.f, swing) * 0.5f;
                if (vr <= 0) continue;
                float j = (1.f + 0.7f) * vr / 2.f;
                a.vx -= j * nx; a.vz -= j * nz;
                b.vx += j * nx; b.vz += j * nz;
                float over = (ra + rb - d) * 0.5f;
                a.x -= nx * over; a.z -= nz * over; b.x += nx * over; b.z += nz * over;
                knock(a); knock(b);
            }
    }

    // --- puanlama ---
    void scoreRoll() {
        int down = 0;
        for (auto& p : pins_) if (!p.counted && (p.gone || p.tilt > 0.35f)) { ++down; p.counted = true; }
        rolls_.push_back(down);
        int standing = 0;
        for (const auto& p : pins_) if (!p.counted) ++standing;
        bool last = frame_ == FRAMES - 1;
        if (standing == 0 && down == 10) say("STRIKE!", { 255, 210, 60, 255 });
        else if (standing == 0) say("SPARE!", { 110, 220, 255, 255 });
        else if (down == 0) say(ball_.gutter ? T("Oluk!", "Gutter!") : T("Iska!", "Miss!"), { 235, 235, 240, 255 });
        else say(std::to_string(down) + (down == 1 ? T(" lobut", " pin") : T(" lobut", " pins")), WHITE);
        frameRolls_[frame_][rollInFrame_] = down;
        bool frameDone;
        if (!last) frameDone = standing == 0 || rollInFrame_ == 1;
        else {   // son frame: strike/spare yapilirsa 3. atis hakki
            int a = frameRolls_[frame_][0], b = frameRolls_[frame_][1];
            if (rollInFrame_ == 0) frameDone = false;
            else if (rollInFrame_ == 1) frameDone = !(a == 10 || a + b == 10);
            else frameDone = true;
        }
        nextRack_ = frameDone || standing == 0;
        frameDone_ = frameDone;
        setState(State::Result);
    }
    void nextRoll() {
        if (frameDone_) {
            rollCount_[frame_] = rollInFrame_ + 1;
            ++frame_; rollInFrame_ = 0;
            if (frame_ >= FRAMES) {
                over_ = true; frame_ = FRAMES - 1;
                if (best_ && totalScore() > *best_) *best_ = totalScore();
                setState(State::Result);
                return;
            }
        } else ++rollInFrame_;
        rack(nextRack_);
        if (nextRack_) for (auto& p : pins_) p.counted = false;
        setState(State::Pickup);
    }
    int rollsBefore(int f) const {
        int idx = 0;
        for (int i = 0; i < f; ++i) idx += rollCount_[i];
        return idx;
    }
    int rollsIn(int f) const {
        if (f < frame_ || over_) return rollCount_[f];
        if (f == frame_) return static_cast<int>(rolls_.size()) - rollsBefore(f);
        return 0;
    }
    // Gercek bowling puani (strike: sonraki iki atis, spare: sonraki bir atis bonus)
    int frameScore(int f, bool& known) const {
        int idx = rollsBefore(f);
        auto R = [&](int k) -> int { return k < static_cast<int>(rolls_.size()) ? rolls_[k] : -1; };
        known = true;
        int a = R(idx);
        if (a < 0 || rollsIn(f) == 0) { known = false; return 0; }
        if (f == FRAMES - 1) {
            int s = 0;
            for (int k = 0; k < rollsIn(f); ++k) s += R(idx + k);
            known = over_;
            return s;
        }
        if (a == 10) { int b = R(idx + 1), c = R(idx + 2); if (b < 0 || c < 0) known = false; return 10 + (std::max)(b, 0) + (std::max)(c, 0); }
        int b = R(idx + 1);
        if (b < 0) { known = false; return a; }
        if (a + b == 10) { int c = R(idx + 2); if (c < 0) known = false; return 10 + (std::max)(c, 0); }
        return a + b;
    }
    int totalScore() const {
        int s = 0;
        for (int f = 0; f < FRAMES; ++f) { bool k; s += frameScore(f, k); }
        return s;
    }
    void say(const std::string& m, Color col) { msg_ = m; msgColor_ = col; msgTime_ = 1.6f; }

    // --- kamera: atista topu takip eder ---
    void updateCamera(float dt, const Body3& body) {
        Vector3 pos, tgt;
        if (state_ == State::Rolling || state_ == State::Settle || (state_ == State::Result && !over_)) {
            float z = std::clamp(ball_.p.z, 0.f, HEAD_Z - 3.2f);
            if (state_ != State::Rolling) z = HEAD_Z - 3.2f;
            pos = { ball_.p.x * 0.35f, 1.15f, z - 2.6f };
            tgt = { 0, 0.25f, z + 5.f };
        } else {
            float hx = body.valid ? body[HipCenter].x : playerX_;
            pos = { hx * 0.6f, 1.75f, -4.3f };
            tgt = { hx * 0.25f, 0.7f, 7.f };
        }
        float k = (std::min)(1.f, dt * (state_ == State::Rolling ? 5.f : 2.5f));
        camPos_ = Vector3Lerp(camPos_, pos, k);
        camTarget_ = Vector3Lerp(camTarget_, tgt, k);
    }

    // --- cizim ---
    static Color ballColor(Color a) { return { static_cast<unsigned char>(a.r * 0.75f), static_cast<unsigned char>(a.g * 0.75f), static_cast<unsigned char>(a.b * 0.75f), 255 }; }

    void drawBall(Color accent) {
        Matrix rot = MatrixMultiply(MatrixRotateX(ball_.spin), MatrixRotateY(std::atan2(ball_.v.x, ball_.v.z)));
        r3d::draw(r3d::sphere(), r3d::trs(ball_.p, { BALL_R, BALL_R, BALL_R }, rot), ballColor(accent), 1.f);
        for (int i = 0; i < 3; ++i) {   // parmak delikleri donerken gorunur
            Vector3 o = Vector3Transform({ (i - 1) * 0.3f, 0.75f, 0.55f + (i == 1 ? 0.12f : 0.f) }, rot);
            r3d::ballLo(Vector3Add(ball_.p, Vector3Scale(Vector3Normalize(o), BALL_R * 0.97f)), BALL_R * 0.13f, { 15, 15, 20, 255 });
        }
    }

    static const r3d::Mesh3& pinMesh() {
        static r3d::Mesh3 m = r3d::makeLathe({ { 0.0f, 0.f }, { 0.026f, 0.f }, { 0.04f, 0.02f }, { 0.055f, 0.07f }, { 0.0605f, 0.115f },
            { 0.057f, 0.15f }, { 0.046f, 0.19f }, { 0.031f, 0.23f }, { 0.023f, 0.26f }, { 0.024f, 0.28f }, { 0.031f, 0.315f },
            { 0.032f, 0.34f }, { 0.026f, 0.365f }, { 0.012f, 0.379f }, { 0.f, 0.381f } }, 18,
            [](float y) { return (y > 0.223f && y < 0.243f) || (y > 0.25f && y < 0.262f); });
        return m;
    }
    void drawPin(const Pin& p) const {
        Matrix rot = MatrixIdentity();
        if (p.tilt > 0.001f) rot = MatrixRotate(Vector3Normalize({ p.dir.y, 0, -p.dir.x }), p.tilt);
        r3d::draw(pinMesh(), r3d::trs({ p.x, 0, p.z }, { 1, 1, 1 }, rot), { 248, 248, 246, 255 }, 0.8f, { 210, 30, 40, 255 });
    }

    void drawAlley(Color accent, float time) {
        r3d::ground(-6.f, -6.f, 6.f, PIT_Z + 1.f, -0.08f, { 30, 24, 40, 255 });   // salon zemini
        for (int lane = -1; lane <= 1; ++lane) {   // yan seritler derinlik hissi verir
            float ox = lane * 2.1f;
            for (int i = 0; i < 10; ++i) {   // tahta serit
                float x0 = ox - LANE_HW + i * (2 * LANE_HW / 10), x1 = x0 + 2 * LANE_HW / 10;
                Color w = (i % 2) ? Color{ 222, 176, 118, 255 } : Color{ 206, 160, 104, 255 };
                r3d::ground(x0, -0.2f, x1, PIT_Z - 0.4f, 0.f, w, 0.9f);
            }
            r3d::ground(ox - LANE_HW - 0.04f, -4.8f, ox + LANE_HW + 0.04f, -0.2f, 0.f, { 196, 150, 98, 255 }, 0.5f);   // yaklasma alani
            for (float s : { -1.f, 1.f }) {   // oluklar ve ayiricilar
                float gx0 = s < 0 ? ox - LANE_HW - GUTTER_W : ox + LANE_HW, gx1 = gx0 + GUTTER_W;
                r3d::ground(gx0, -0.2f, gx1, PIT_Z, -0.06f, { 70, 72, 84, 255 }, 0.6f);
                r3d::cube({ ox + s * (LANE_HW + GUTTER_W + 0.06f), 0.05f, (PIT_Z - 4.8f) / 2 }, { 0.12f, 0.12f, PIT_Z + 4.8f }, { 44, 40, 60, 255 }, 0, 0.6f);
            }
            if (lane != 0)
                for (int row = 0; row < 4; ++row)
                    for (int i = 0; i <= row; ++i) drawPin(Pin{ ox + (i - row / 2.f) * 0.305f, HEAD_Z + row * 0.264f });
        }
        // atis cizgisi, nisan oklari ve noktalari
        r3d::flatQuad({ -LANE_HW, 0.002f, -0.02f }, { LANE_HW, 0.002f, -0.02f }, { LANE_HW, 0.002f, 0.02f }, { -LANE_HW, 0.002f, 0.02f }, { 30, 30, 40, 255 });
        for (int i = -3; i <= 3; ++i) {
            float x = i * 0.13f, z = 4.6f + std::abs(i) * 0.3f;
            r3d::flatQuad({ x, 0.002f, z + 0.25f }, { x - 0.03f, 0.002f, z }, { x + 0.03f, 0.002f, z }, { x, 0.002f, z + 0.25f }, { 120, 64, 36, 255 });
            r3d::flatQuad({ x - 0.015f, 0.002f, 2.f }, { x + 0.015f, 0.002f, 2.f }, { x + 0.015f, 0.002f, 2.03f }, { x - 0.015f, 0.002f, 2.03f }, { 120, 64, 36, 255 });
        }
        // nisan izi: topu tutarken oyuncunun onunden duz bir isik
        if (state_ == State::Hold)
            for (float z = 0.4f; z < 12.f; z += 0.5f) {
                float a = (1.f - z / 12.f) * (0.5f + 0.5f * std::sin(time * 6.f - z));
                r3d::flatQuad({ playerX_ - 0.03f, 0.003f, z }, { playerX_ + 0.03f, 0.003f, z }, { playerX_ + 0.03f, 0.003f, z + 0.2f }, { playerX_ - 0.03f, 0.003f, z + 0.2f }, ColorAlpha(accent, a));
            }
        // lobut alani: arka duvar, ust perde ve neon serit
        r3d::cube({ 0, 0.6f, PIT_Z + 0.6f }, { 8.f, 1.4f, 0.2f }, { 22, 18, 32, 255 });
        r3d::cube({ 0, 1.15f, HEAD_Z - 0.6f }, { 8.f, 0.5f, 0.3f }, { 34, 28, 48, 255 }, 0, 0.4f);
        r3d::rod({ -4.f, 0.88f, HEAD_Z - 0.76f }, { 4.f, 0.88f, HEAD_Z - 0.76f }, 0.025f, accent);
        // top makinesi (ekranin sag tarafinda) ve uzerinde bekleyen top
        r3d::cube({ machineX(), 0.38f, -1.5f }, { 0.42f, 0.76f, 0.9f }, { 52, 48, 70, 255 }, 0, 0.6f);
        r3d::cube({ machineX(), 0.78f, -1.5f }, { 0.36f, 0.06f, 0.8f }, accent, 0, 0.8f);
        if (state_ == State::Pickup || state_ == State::ChooseHand) r3d::ball({ machineX(), 0.81f + BALL_R, -1.5f }, BALL_R, ballColor(accent), 1.f);
    }

    void drawScoreboard(GameContext& c) {
        const float bw = 92, bh = 70, x0 = VW / 2.f - (FRAMES * bw) / 2.f - 60.f, y = 14;
        DrawRectangleRounded({ x0 - 12, y - 8, FRAMES * bw + 160.f, bh + 16 }, 0.2f, 8, { 0, 0, 0, 160 });
        int running = 0;
        for (int f = 0; f < FRAMES; ++f) {
            Rectangle r = { x0 + f * bw, y, bw - 6, bh };
            bool cur = f == frame_ && !over_;
            DrawRectangleRec(r, cur ? ColorAlpha(c.accent, 0.9f) : Color{ 40, 40, 52, 230 });
            text(c.fonts.regular, std::to_string(f + 1), r.x + 5, r.y + 2, 14, { 200, 200, 210, 255 });
            int idx = rollsBefore(f), n = rollsIn(f);
            std::string marks;
            for (int k = 0; k < n; ++k) {
                int v = rolls_[idx + k];
                bool spare = false;
                if (k > 0) {
                    int prev = rolls_[idx + k - 1];
                    bool prevFresh = k == 1 ? prev != 10 : (rolls_[idx] == 10 && prev != 10);   // onceki atis yeni dizilmis bir setin ilki miydi
                    spare = prevFresh && prev + v == 10;
                }
                marks += spare ? "/" : v == 10 ? "X" : v == 0 ? "-" : std::to_string(v);
                marks += " ";
            }
            text(c.fonts.bold, marks, r.x + 18, r.y + 4, 22, WHITE);
            bool known;
            int fs = frameScore(f, known);
            running += fs;
            if (n > 0 && known) textCenter(c.fonts.bold, std::to_string(running), r.x + r.width / 2, r.y + 34, 28, { 255, 215, 80, 255 });
        }
        text(c.fonts.light, std::to_string(totalScore()), x0 + FRAMES * bw + 16, y + 6, 54, WHITE);
    }

    int* best_;
    std::array<Pin, 10> pins_{};
    BallS ball_;
    std::vector<int> rolls_;
    int frameRolls_[FRAMES][3] = {};
    int rollCount_[FRAMES] = {};
    std::vector<Sample> hist_;
    State state_ = State::Pickup;
    Vector3 camPos_{}, camTarget_{};
    float stateTime_ = 0, msgTime_ = 0, playerX_ = 0;
    int frame_ = 0, rollInFrame_ = 0, holdHand_ = 1, hand_ = -1;   // hand_: -1 secilmedi, 0 sol, 1 sag
    float chooseTime_[2] = {}, toastHand_ = 0;
    bool handFromSettings_ = false;
    bool over_ = false, nextRack_ = true, frameDone_ = false;
    std::string msg_;
    Color msgColor_ = WHITE;
};
