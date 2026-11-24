// Kinext 1.0 Dashboard - oyun altyapisi, oyun katalogu (magaza) ve koddan cizilen kapaklar
#pragma once
#include "avatar.hpp"
#include <random>
#include <vector>

struct GameContext {
    const Fonts& fonts;
    Ui& ui;
    Color accent;
    int avatar;
    float dt;
    const Skeleton& body;     // 0-1, aynalanmis iskelet
    float time;               // toplam sure (animasyonlar icin)
};

enum class GameAction { None, Restart, Exit };

class Game {
public:
    virtual ~Game() = default;
    virtual void reset() = 0;
    virtual GameAction frame(GameContext& c) = 0;
    virtual int score() const = 0;
    virtual bool finished() const = 0;
};

// --- Katalog: magazada gorunen tum oyunlar ---
struct GameInfo {
    const char* id;
    const char* titleTr; const char* titleEn;
    const char* descTr;  const char* descEn;
    bool installed;
    const char* title() const { return T(titleTr, titleEn); }
    const char* desc() const { return T(descTr, descEn); }
};
inline const std::vector<GameInfo>& catalog() {
    static const std::vector<GameInfo> list = {
        { "balloon", "Balon Patlat", "Balloon Pop",  "Yükselen balonları ellerinle patlat", "Pop balloons with your hands", true  },
        { "dodge",   "Engelden Kaç", "Dodge",        "Sağa sola kayarak bloklardan kaç",    "Lean left and right to dodge", true  },
        { "bowling", "Bowling",      "Bowling",      "Kolunu savur, lobutları devir",       "Swing your arm, knock pins down",     false },
        { "pose",    "Pozu Tut",     "Strike a Pose","Ekrandaki pozu taklit et",            "Copy the pose on screen",                 false },
        { "keeper",  "Kaleci",       "Goalkeeper",   "Gelen toplara uzan, gol yeme",        "Stop the shots with your body",      false },
    };
    return list;
}
inline const GameInfo* findGame(const std::string& id) {
    for (const auto& g : catalog()) if (id == g.id) return &g;
    return nullptr;
}

// --- Kapak gorselleri: her oyun icin koddan cizilmis kucuk bir sahne ---
inline void drawCover(const std::string& id, Rectangle r, float time, Color accent) {
    auto rx = [&](float f) { return r.x + r.width * f; };
    auto ry = [&](float f) { return r.y + r.height * f; };
    float u = (std::min)(r.width, r.height);
    if (id == "balloon") {
        DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height), { 90, 170, 240, 255 }, { 200, 230, 255, 255 });
        const Color cols[] = { {230, 60, 70, 255}, {250, 200, 40, 255}, {80, 200, 90, 255}, {200, 90, 210, 255}, {40, 140, 235, 255} };
        const float px[] = { 0.18f, 0.38f, 0.58f, 0.76f, 0.9f }, py[] = { 0.55f, 0.35f, 0.6f, 0.3f, 0.65f };
        for (int i = 0; i < 5; ++i) {
            float bob = std::sin(time * 1.5f + i) * u * 0.02f;
            Vector2 c = { rx(px[i]), ry(py[i]) + bob };
            float br = u * 0.11f;
            DrawLineEx({ c.x, c.y + br }, { c.x + 3, c.y + br * 2.2f }, 2, { 255, 255, 255, 200 });
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), br, br * 1.15f, cols[i]);
            DrawCircleV({ c.x - br * 0.35f, c.y - br * 0.45f }, br * 0.18f, { 255, 255, 255, 200 });
        }
        DrawRectangleRec({ r.x, ry(0.86f), r.width, r.height * 0.14f }, { 110, 190, 90, 255 });
    } else if (id == "dodge") {
        DrawRectangleRec(r, { 25, 28, 40, 255 });
        for (int i = 1; i < 3; ++i) DrawLineEx({ rx(i / 3.f), r.y }, { rx(i / 3.f), r.y + r.height }, 2, { 255, 255, 255, 60 });
        float off = std::fmod(time * 0.25f, 1.f);
        const int lanes[] = { 0, 2, 1, 0 };
        for (int i = 0; i < 4; ++i) {
            float y = std::fmod(off + i * 0.3f, 1.2f) - 0.2f;
            Rectangle b = GetCollisionRec(r, { rx(lanes[i] / 3.f + 0.04f), ry(y), r.width / 3.f * 0.75f, r.height * 0.14f });
            if (b.width > 0 && b.height > 0) DrawRectangleRec(b, { 220, 60, 60, 255 });   // kutunun disina tasmasin
        }
        DrawRectangleRounded({ rx(1 / 3.f + 0.09f), ry(0.74f), r.width / 3.f * 0.45f, r.height * 0.2f }, 0.3f, 6, { 80, 220, 110, 255 });
    } else if (id == "bowling") {
        DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height), { 60, 40, 30, 255 }, { 170, 120, 70, 255 });
        for (int row = 0; row < 3; ++row)
            for (int i = 0; i <= row; ++i) {
                Vector2 p = { rx(0.5f + (i - row / 2.f) * 0.09f), ry(0.25f + row * 0.12f) };
                DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), u * 0.035f, u * 0.08f, WHITE);
                DrawRectangleRec({ p.x - u * 0.03f, p.y - u * 0.03f, u * 0.06f, u * 0.015f }, RED);
            }
        DrawCircleV({ rx(0.5f), ry(0.8f) }, u * 0.12f, { 30, 60, 160, 255 });
    } else if (id == "pose") {
        DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height), { 120, 40, 160, 255 }, { 230, 90, 150, 255 });
        Vector2 c = { rx(0.5f), ry(0.45f) };
        DrawCircleV({ c.x, c.y - u * 0.25f }, u * 0.09f, WHITE);
        DrawLineEx({ c.x, c.y - u * 0.15f }, { c.x, c.y + u * 0.15f }, u * 0.06f, WHITE);
        DrawLineEx({ c.x, c.y - u * 0.1f }, { c.x - u * 0.25f, c.y - u * 0.3f }, u * 0.05f, WHITE);
        DrawLineEx({ c.x, c.y - u * 0.1f }, { c.x + u * 0.25f, c.y - u * 0.3f }, u * 0.05f, WHITE);
        DrawLineEx({ c.x, c.y + u * 0.15f }, { c.x - u * 0.15f, c.y + u * 0.4f }, u * 0.05f, WHITE);
        DrawLineEx({ c.x, c.y + u * 0.15f }, { c.x + u * 0.15f, c.y + u * 0.4f }, u * 0.05f, WHITE);
    } else if (id == "keeper") {
        DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height), { 40, 130, 60, 255 }, { 70, 170, 80, 255 });
        DrawRectangleLinesEx({ rx(0.15f), ry(0.2f), r.width * 0.7f, r.height * 0.5f }, u * 0.03f, WHITE);
        for (int i = 1; i < 8; ++i) DrawLineEx({ rx(0.15f + i * 0.0875f), ry(0.2f) }, { rx(0.15f + i * 0.0875f), ry(0.7f) }, 1, { 255, 255, 255, 90 });
        DrawCircleV({ rx(0.62f), ry(0.42f) }, u * 0.08f, WHITE);
        DrawCircleV({ rx(0.62f), ry(0.42f) }, u * 0.03f, BLACK);
    } else {
        DrawRectangleRec(r, accent);
    }
}

// --- Oyunlarin ortak parcalari ---
inline Rectangle centeredButton(float cx, float y, float w, float h) { return { cx - w / 2, y, w, h }; }

inline bool gameButton(GameContext& c, const std::string& id, Rectangle r, const std::string& label, Color bg) {
    bool hover = c.ui.hovered(r);
    DrawRectangleRec(r, hover ? lighten(bg, 0.15f) : bg);
    if (hover) DrawRectangleLinesEx({ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 3, WHITE);
    textCenter(c.fonts.regular, label, r.x + r.width / 2, r.y + r.height / 2 - 16, 32, WHITE);
    return c.ui.select(id, r);
}

// Oyun sonu ekrani: basligi, puani, TEKRAR ve MENU butonlarini cizer
inline GameAction resultScreen(GameContext& c, const std::string& title, int score, int best) {
    DrawRectangle(0, 0, VW, VH, { 0, 0, 0, 170 });
    textCenterShadow(c.fonts.light, title, VW / 2.f, VH * 0.2f, 72, WHITE);
    textCenterShadow(c.fonts.bold, std::to_string(score), VW / 2.f, VH * 0.34f, 96, { 255, 215, 80, 255 });
    textCenter(c.fonts.regular, score >= best && score > 0 ? T("Yeni rekor!", "New record!") : T("En iyi: ", "Best: ") + std::to_string(best),
               VW / 2.f, VH * 0.52f, 28, { 220, 220, 230, 255 });
    if (gameButton(c, "res_again", { VW / 2.f - 290, VH * 0.64f, 270, 84 }, T("Tekrar", "Again"), c.accent)) return GameAction::Restart;
    if (gameButton(c, "res_menu", { VW / 2.f + 20, VH * 0.64f, 270, 84 }, T("Ana Sayfa", "Home"), { 70, 72, 80, 255 })) return GameAction::Exit;
    return GameAction::None;
}

inline void topBar(GameContext& c, const std::string& left, const std::string& mid, const std::string& right) {
    DrawRectangle(0, 0, VW, 64, { 0, 0, 0, 120 });
    text(c.fonts.bold, left, 28, 14, 34, { 255, 215, 80, 255 });
    textCenter(c.fonts.bold, mid, VW / 2.f, 12, 38, WHITE);
    text(c.fonts.regular, right, VW - 28 - textWidth(c.fonts.regular, right, 28), 18, 28, { 230, 230, 235, 255 });
}

inline void waitingForPlayer(GameContext& c) {
    DrawRectangle(0, VH / 2 - 60, VW, 120, { 0, 0, 0, 140 });
    textCenter(c.fonts.light, T("Kameranın önüne geç", "Step in front of the camera"), VW / 2.f, VH / 2.f - 30, 52, WHITE);
}
