// Kinext 1.0 Dashboard - Xbox One (2013-2015) tarzi ana ekran, magaza ve oyunlar
//   El: elini kaldir, imleci bir kutunun ustunde tut (halka dolunca secilir)
//   Fare: tikla    ESC: oyundan cik / ana sayfaya don    F11: tam ekran
#include "../tracker.hpp"
#include "game_balloon.hpp"
#include "game_dodge.hpp"
#include "settings.hpp"
#include <filesystem>
#include <map>
#include <memory>

enum class Scene { PickColor, PickAvatar, Dashboard, Playing };
enum class Tab { Home, Games, Store, Settings };

class App {
public:
    App(Fonts& f, Settings& s, Tracker& tr, bool trackerOk, Texture2D logo)
        : f_(f), s_(s), tracker_(tr), trackerOk_(trackerOk), logo_(logo) {
        games_["balloon"] = std::make_unique<BalloonGame>(&s_.best["balloon"]);
        games_["dodge"] = std::make_unique<DodgeGame>(&s_.best["dodge"]);
        cameraNames_ = tracker_.cameraNames();
        scene_ = s_.setupDone ? Scene::Dashboard : Scene::PickColor;
    }

    bool quit = false;

    void frame(float dt) {
        time_ += dt;
        TrackerState ts = tracker_.state();
        body_ = ts.skeleton;
        fps_ = ts.fps;
        camIndex_ = ts.camIndex;
        backend_ = ts.backend;
        accent_ = ACCENTS[std::clamp(s_.accent, 0, ACCENT_COUNT - 1)].color;

        escHandled_ = confirmReset_ && IsKeyPressed(KEY_ESCAPE);
        ui_.setHandMode(s_.hand);
        ui_.begin(dt, body_);
        switch (scene_) {
        case Scene::PickColor:  pickColor(); break;
        case Scene::PickAvatar: pickAvatar(); break;
        case Scene::Dashboard:  dashboard(); break;
        case Scene::Playing:    playing(dt); break;
        }
        ui_.end();

        drawToast(dt);
        if (fade_ > 0) { DrawRectangle(0, 0, VW, VH, ColorAlpha(BLACK, fade_)); fade_ = (std::max)(0.f, fade_ - dt * 3.f); }
        // Oyun oynanirken imlec gorunmesin; menulerde ve oyun sonu ekraninda gorunsun
        if (scene_ != Scene::Playing || (current_ && current_->finished())) ui_.drawCursor();

        if (IsKeyPressed(KEY_ESCAPE) && !escHandled_) {
            if (scene_ == Scene::Playing) go(Scene::Dashboard);
            else if (scene_ == Scene::Dashboard && tab_ != Tab::Home) tab_ = Tab::Home;
            else if (scene_ != Scene::Dashboard && s_.setupDone) go(Scene::Dashboard);
        }
    }

private:
    // ---------------- ortak ----------------
    void go(Scene s) { scene_ = s; fade_ = 1.f; }
    void toast(const std::string& msg) { toast_ = msg; toastTime_ = 2.6f; }

    void drawToast(float dt) {
        if (toastTime_ <= 0) return;
        toastTime_ -= dt;
        float a = (std::min)(1.f, toastTime_ * 3.f);
        float w = textWidth(f_.regular, toast_, 24) + 60;
        Rectangle r = { VW / 2.f - w / 2, VH - 110.f, w, 56 };
        DrawRectangleRec(r, ColorAlpha(accent_, 0.95f * a));
        textCenter(f_.regular, toast_, VW / 2.f, r.y + 14, 24, ColorAlpha(WHITE, a));
    }

    void background() {
        DrawRectangleGradientV(0, 0, VW, VH, { 24, 24, 28, 255 }, { 8, 8, 10, 255 });
    }

    void header() {
        // Sol ust: profil rozeti ve ad
        DrawRectangle(40, 26, 46, 46, accent_);
        drawIcon(Icon::Avatar, { 63, 49 }, 30, WHITE);
        text(f_.regular, s_.name, 98, 34, 26, { 230, 230, 235, 255 });
        // Sag ust: Ataby + KINEXT logosu (beyaz)
        if (logo_.id != 0) {
            float h = 34, w = h * logo_.width / static_cast<float>(logo_.height);
            DrawTexturePro(logo_, { 0, 0, static_cast<float>(logo_.width), static_cast<float>(logo_.height) },
                           { VW - 40 - w, 32, w, h }, { 0, 0 }, 0, WHITE);
        } else {
            text(f_.bold, "KINEXT", VW - 40 - textWidth(f_.bold, "KINEXT", 34), 30, 34, WHITE);
        }
    }

    // Oyuncunun canli maskotu (yoksa el sallayan varsayilan durus)
    void liveAvatar(Rectangle r, float torso) {
        float cx = r.x + r.width / 2, hipY = r.y + r.height * 0.62f;
        Skeleton b = body_.valid ? fitBody(body_, cx, hipY, torso, false) : idleBody(cx, hipY, torso, time_);
        drawAvatar(b, s_.avatar, accent_);
    }

    void statusBar() {
        std::string cam = (camIndex_ >= 0 && camIndex_ < static_cast<int>(cameraNames_.size())) ? cameraNames_[camIndex_] : "-";
        std::string st = trackerOk_
            ? "Kamera: " + cam + "   ·   " + std::to_string(static_cast<int>(fps_)) + " FPS   ·   " + backend_
            : "Hareket takibi başlatılamadı: " + tracker_.error();
        text(f_.regular, st, 40, VH - 44, 18, trackerOk_ ? Color{ 140, 140, 150, 255 } : Color{ 255, 120, 110, 255 });
        std::string hint = body_.valid ? "Elini kaldır ve bir kutunun üstünde tut" : "Kameranın önüne geç";
        text(f_.regular, hint, VW - 40 - textWidth(f_.regular, hint, 18), VH - 44, 18, { 140, 140, 150, 255 });
    }

    void startGame(const std::string& id) {
        auto it = games_.find(id);
        if (it == games_.end()) { toast("Bu oyun yakında geliyor!"); return; }
        current_ = it->second.get();
        currentId_ = id;
        current_->reset();
        saved_ = false;
        s_.lastGame = id;
        s_.save();
        go(Scene::Playing);
    }

    // ---------------- ilk kurulum: renk ve maskot ----------------
    void pickColor() {
        background();
        textCenter(f_.light, s_.setupDone ? "Vurgu rengini seç" : "Hoş geldin! Önce rengini seç", VW / 2.f, 70, 56, WHITE);
        textCenter(f_.regular, "Kutular ve menüler bu renge bürünecek", VW / 2.f, 150, 24, { 170, 170, 180, 255 });
        const float sz = 170, gap = 18;
        const float x0 = (VW - (5 * sz + 4 * gap)) / 2;
        for (int i = 0; i < ACCENT_COUNT; ++i) {
            Rectangle r = { x0 + (i % 5) * (sz + gap), 220 + (i / 5) * (sz + gap), sz, sz };
            bool hover = ui_.hovered(r);
            drawTile(f_, r, ACCENTS[i].color, ACCENTS[i].name, Icon::None, hover || i == s_.accent, 22);
            if (ui_.select("color" + std::to_string(i), r)) {
                s_.accent = i;
                s_.save();
                go(s_.setupDone ? Scene::Dashboard : Scene::PickAvatar);
            }
        }
        statusBar();
    }

    void pickAvatar() {
        background();
        textCenter(f_.light, "Maskotunu seç", VW / 2.f, 60, 56, WHITE);
        textCenter(f_.regular, "Oyunlarda kamera görüntüsü yerine bu maskot olarak görüneceksin", VW / 2.f, 140, 24, { 170, 170, 180, 255 });
        const float w = 320, h = 430, gap = 36;
        const float x0 = (VW - (static_cast<float>(AVATAR_COUNT) * w + (AVATAR_COUNT - 1) * gap)) / 2;
        for (int i = 0; i < AVATAR_COUNT; ++i) {
            Rectangle r = { x0 + i * (w + gap), 200, w, h };
            bool hover = ui_.hovered(r);
            DrawRectangleRec(r, hover ? lighten(accent_, 0.1f) : ColorAlpha(accent_, 0.55f));
            float cx = r.x + w / 2, hipY = r.y + 230;
            Skeleton b = body_.valid ? fitBody(body_, cx, hipY, 70, false) : idleBody(cx, hipY, 70, time_ + i);
            drawAvatar(b, i, accent_);
            DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + h - 86), static_cast<int>(w), 86, { 0, 0, 0, 110 });
            text(f_.bold, avatarName(i), r.x + 16, r.y + h - 80, 32, WHITE);
            text(f_.regular, avatarDesc(i), r.x + 16, r.y + h - 40, 20, { 225, 225, 230, 255 });
            if (hover || i == s_.avatar) DrawRectangleLinesEx({ r.x - 3, r.y - 3, w + 6, h + 6 }, 3, WHITE);
            if (ui_.select("avatar" + std::to_string(i), r)) {
                s_.avatar = i;
                bool first = !s_.setupDone;
                s_.setupDone = true;
                s_.save();
                tab_ = first ? Tab::Home : Tab::Settings;
                go(Scene::Dashboard);
                if (first) toast(std::string("Hoş geldin ") + s_.name + "!");
            }
        }
        statusBar();
    }

    // ---------------- dashboard ----------------
    void dashboard() {
        if (confirmReset_) ui_.setBlocked(true);
        background();
        header();
        tabs();
        switch (tab_) {
        case Tab::Home:     home(); break;
        case Tab::Games:    gamesTab(); break;
        case Tab::Store:    storeTab(); break;
        case Tab::Settings: settingsTab(); break;
        }
        statusBar();
        if (confirmReset_) resetDialog();
    }

    void tabs() {
        const char* names[] = { "Ana Sayfa", "Oyunlar", "Mağaza", "Ayarlar" };
        float x = 40;
        for (int i = 0; i < 4; ++i) {
            float w = textWidth(f_.light, names[i], 42);
            Rectangle r = { x - 8, 88, w + 16, 56 };
            bool sel = static_cast<int>(tab_) == i;
            bool hover = ui_.hovered(r);
            text(f_.light, names[i], x, 92, 42, sel ? WHITE : (hover ? Color{ 200, 200, 205, 255 } : Color{ 255, 255, 255, 80 }));
            if (!sel && ui_.select(std::string("tab") + names[i], r, 0.7f)) tab_ = static_cast<Tab>(i);
            x += w + 44;
        }
    }

    // Oyun kutusu: kapak + alt seritte ad
    bool gameTile(const std::string& id, Rectangle r, const std::string& title, float titleSize, const std::string& sub = "") {
        bool hover = ui_.hovered(r);
        drawCover(id, r, time_, accent_);
        float stripH = sub.empty() ? titleSize + 22 : titleSize + 44;
        DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + r.height - stripH), static_cast<int>(r.width), static_cast<int>(stripH), { 0, 0, 0, 150 });
        text(f_.regular, title, r.x + 14, r.y + r.height - stripH + 8, titleSize, WHITE);
        if (!sub.empty()) text(f_.regular, sub, r.x + 14, r.y + r.height - 30, 18, { 210, 210, 215, 255 });
        if (hover) DrawRectangleLinesEx({ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 3, WHITE);
        return ui_.select("game_" + id + std::to_string(static_cast<int>(r.x)) + std::to_string(static_cast<int>(r.y)), r);
    }

    std::string bestText(const std::string& id) {
        int b = s_.best.count(id) ? s_.best.at(id) : 0;
        return b > 0 ? "Rekor: " + std::to_string(b) : "Henüz oynanmadı";
    }

    void home() {
        // Profil kutusu: canli maskot + skorlar
        Rectangle prof = { 40, 170, 210, 430 };
        DrawRectangleRec(prof, accent_);
        liveAvatar({ prof.x, prof.y + 10, prof.width, 260 }, 58);
        text(f_.bold, s_.name, prof.x + 14, prof.y + 300, 26, WHITE);
        text(f_.regular, "Toplam puan", prof.x + 14, prof.y + 340, 18, { 230, 230, 235, 255 });
        text(f_.bold, std::to_string(s_.total()), prof.x + 14, prof.y + 362, 34, WHITE);

        // Buyuk kutu: son oynanan oyun
        const GameInfo* last = findGame(s_.lastGame);
        if (!last || !last->installed) last = &catalog()[0];
        if (gameTile(last->id, { 266, 170, 560, 315 }, last->title, 34, "Devam et  ·  " + bestText(last->id))) startGame(last->id);

        // Sag kutular
        Rectangle tGames = { 842, 170, 150, 150 }, tStore = { 842, 335, 150, 150 };
        drawTile(f_, tGames, accent_, "Oyunlarım", Icon::Games, ui_.hovered(tGames));
        if (ui_.select("home_games", tGames)) tab_ = Tab::Games;
        drawTile(f_, tStore, accent_, "Mağaza", Icon::Store, ui_.hovered(tStore));
        if (ui_.select("home_store", tStore)) tab_ = Tab::Store;

        // Alt sira
        Rectangle tSet = { 266, 501, 133, 99 }, tAv = { 414, 501, 133, 99 }, tCam = { 562, 501, 133, 99 };
        drawTile(f_, tSet, accent_, "Ayarlar", Icon::Settings, ui_.hovered(tSet), 17);
        if (ui_.select("home_settings", tSet)) tab_ = Tab::Settings;
        drawTile(f_, tAv, accent_, "Maskot", Icon::Avatar, ui_.hovered(tAv), 17);
        if (ui_.select("home_avatar", tAv)) go(Scene::PickAvatar);
        drawTile(f_, tCam, accent_, "Kamera", Icon::Camera, ui_.hovered(tCam), 17);
        if (ui_.select("home_cam", tCam)) nextCamera();
        // Diger kurulu oyun
        for (const auto& g : catalog())
            if (g.installed && last->id != std::string(g.id)) {
                if (gameTile(g.id, { 710, 501, 282, 99 }, g.title, 20)) startGame(g.id);
                break;
            }

        // One cikanlar (magazadan)
        text(f_.regular, "ÖNE ÇIKANLAR", 1012, 168, 16, { 160, 160, 170, 255 });
        int k = 0;
        for (const auto& g : catalog()) {
            if (g.installed) continue;
            Rectangle r = { 1012, 196.f + k * 206, 228, 194 };
            if (gameTile(g.id, r, g.title, 22, "Yakında")) { tab_ = Tab::Store; toast(std::string(g.title) + " yakında geliyor!"); }
            if (++k == 2) break;
        }
    }

    void gamesTab() {
        text(f_.regular, "Yüklü oyunlar", 40, 168, 22, { 170, 170, 180, 255 });
        int i = 0;
        for (const auto& g : catalog()) {
            if (!g.installed) continue;
            Rectangle r = { 40.f + i * 400, 206, 380, 260 };
            if (gameTile(g.id, r, g.title, 28, bestText(g.id))) startGame(g.id);
            ++i;
        }
        Rectangle more = { 40.f + i * 400, 206, 200, 260 };
        drawTile(f_, more, ColorAlpha(accent_, 0.6f), "Daha fazla oyun", Icon::Store, ui_.hovered(more));
        if (ui_.select("games_more", more)) tab_ = Tab::Store;
    }

    void storeTab() {
        text(f_.regular, "Kinext Mağaza  ·  hareketle oynanan oyunlar", 40, 168, 22, { 170, 170, 180, 255 });
        int i = 0;
        for (const auto& g : catalog()) {
            Rectangle r = { 40.f + (i % 4) * 300, 206.f + (i / 4) * 222, 280, 200 };
            if (gameTile(g.id, r, g.title, 22, g.desc)) {
                if (g.installed) startGame(g.id);
                else toast(std::string(g.title) + " yakında geliyor!");
            }
            // Rozet
            const char* badge = g.installed ? "YÜKLÜ" : "YAKINDA";
            float bw = textWidth(f_.bold, badge, 15) + 18;
            DrawRectangleRec({ r.x + r.width - bw - 8, r.y + 8, bw, 26 }, g.installed ? accent_ : Color{ 60, 60, 66, 230 });
            text(f_.bold, badge, r.x + r.width - bw + 1, r.y + 12, 15, WHITE);
            ++i;
        }
    }

    void settingsTab() {
        const float w = 200, h = 165, gap = 20;
        struct Item { const char* id; std::string label; Icon icon; };
        std::string camName = (camIndex_ >= 0 && camIndex_ < static_cast<int>(cameraNames_.size())) ? cameraNames_[camIndex_] : "-";
        Item items[] = {
            { "set_color", "Vurgu rengi", Icon::Color },
            { "set_avatar", "Maskot", Icon::Avatar },
            { "set_cam", "Kamera", Icon::Camera },
            { "set_hand", std::string("Kontrol eli: ") + Ui::handModeName(s_.hand), Icon::Hand },
            { "set_full", IsWindowFullscreen() || IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "Pencere modu" : "Tam ekran", Icon::Play },
            { "set_reset", "Ayarları sıfırla", Icon::Reset },
            { "set_quit", "Kapat", Icon::Power },
        };
        const int n = static_cast<int>(sizeof(items) / sizeof(items[0]));
        for (int i = 0; i < n; ++i) {
            Rectangle r = { 40 + (i % 4) * (w + gap), 176 + (i / 4) * (h + gap), w, h };
            Color bg = i >= 5 ? Color{ 70, 70, 78, 255 } : accent_;
            drawTile(f_, r, bg, items[i].label, items[i].icon, ui_.hovered(r), 17);
            if (!ui_.select(items[i].id, r)) continue;
            switch (i) {
            case 0: go(Scene::PickColor); break;
            case 1: go(Scene::PickAvatar); break;
            case 2: nextCamera(); break;
            case 3:   // Otomatik -> Sol -> Sag -> Otomatik
                s_.hand = (s_.hand + 1) % 3;
                s_.save();
                toast(std::string("Kontrol eli: ") + Ui::handModeName(s_.hand));
                break;
            case 4: ToggleBorderlessWindowed(); break;
            case 5: confirmReset_ = true; break;
            case 6: s_.save(); quit = true; break;
            }
        }
        // Bilgi (sagda)
        float ix = 40 + 4 * (w + gap) + 10;
        text(f_.regular, "Kamera", ix, 180, 18, { 140, 140, 150, 255 });
        text(f_.regular, camName, ix, 204, 22, { 220, 220, 228, 255 });
        text(f_.regular, "Vurgu rengi", ix, 246, 18, { 140, 140, 150, 255 });
        text(f_.regular, ACCENTS[s_.accent].name, ix, 270, 22, { 220, 220, 228, 255 });
        text(f_.regular, "Maskot", ix, 312, 18, { 140, 140, 150, 255 });
        text(f_.regular, avatarName(s_.avatar), ix, 336, 22, { 220, 220, 228, 255 });
        text(f_.regular, "Kontrol eli", ix, 378, 18, { 140, 140, 150, 255 });
        text(f_.regular, Ui::handModeName(s_.hand), ix, 402, 22, { 220, 220, 228, 255 });
        text(f_.regular, "Kinext 1.0  ·  Microsoft ile bağlantılı değildir", 40, 580, 18, { 120, 120, 130, 255 });
    }

    // Sifirlama onay penceresi (arkadaki her sey kilitli)
    void resetDialog() {
        DrawRectangle(0, 0, VW, VH, { 0, 0, 0, 170 });
        Rectangle box = { VW / 2.f - 360, VH / 2.f - 170, 720, 340 };
        DrawRectangleRec(box, { 30, 30, 36, 255 });
        DrawRectangle(static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.width), 6, accent_);
        textCenter(f_.light, "Ayarlar sıfırlansın mı?", VW / 2.f, box.y + 34, 44, WHITE);
        textCenter(f_.regular, "Vurgu rengi, maskot, kamera, kontrol eli ve", VW / 2.f, box.y + 104, 22, { 200, 200, 210, 255 });
        textCenter(f_.regular, "tüm rekorların silinip ilk kurulum yeniden başlayacak.", VW / 2.f, box.y + 134, 22, { 200, 200, 210, 255 });

        ui_.setBlocked(false);
        Rectangle yes = { VW / 2.f - 300, box.y + 210, 280, 84 }, no = { VW / 2.f + 20, box.y + 210, 280, 84 };
        drawTile(f_, yes, { 190, 40, 50, 255 }, "", Icon::None, ui_.hovered(yes));
        textCenter(f_.regular, "Evet, sıfırla", yes.x + yes.width / 2, yes.y + 26, 28, WHITE);
        drawTile(f_, no, { 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(no));
        textCenter(f_.regular, "Vazgeç", no.x + no.width / 2, no.y + 26, 28, WHITE);
        // Yanlislikla sifirlanmasin: "evet" icin daha uzun tutmak gerekir
        if (ui_.select("reset_yes", yes, 1.8f)) {
            confirmReset_ = false;
            s_.reset();
            tracker_.selectCamera(0);
            tab_ = Tab::Home;
            go(Scene::PickColor);
            toast("Ayarlar sıfırlandı");
        } else if (ui_.select("reset_no", no) || IsKeyPressed(KEY_ESCAPE)) {
            confirmReset_ = false;
        }
    }

    void nextCamera() {
        if (cameraNames_.empty()) { toast("Kamera bulunamadı"); return; }
        int n = static_cast<int>(cameraNames_.size());
        int next = ((camIndex_ < 0 ? 0 : camIndex_) + 1) % n;
        tracker_.selectCamera(next);
        s_.camera = next;
        s_.save();
        toast("Kamera: " + cameraNames_[next]);
    }

    // ---------------- oyun ----------------
    void playing(float dt) {
        if (!current_) { go(Scene::Dashboard); return; }
        GameContext ctx{ f_, ui_, accent_, s_.avatar, dt, body_, time_ };
        GameAction a = current_->frame(ctx);
        if (current_->finished() && !saved_) { s_.save(); saved_ = true; }
        if (a == GameAction::Restart) { current_->reset(); saved_ = false; }
        if (a == GameAction::Exit) go(Scene::Dashboard);
    }

    Fonts& f_;
    Settings& s_;
    Tracker& tracker_;
    bool trackerOk_;
    Texture2D logo_;
    Ui ui_;
    Scene scene_ = Scene::Dashboard;
    Tab tab_ = Tab::Home;
    std::map<std::string, std::unique_ptr<Game>> games_;
    Game* current_ = nullptr;
    std::string currentId_;
    bool saved_ = false, confirmReset_ = false, escHandled_ = false;
    std::vector<std::string> cameraNames_;
    Skeleton body_;
    Color accent_{};
    float time_ = 0, fade_ = 1.f, fps_ = 0, toastTime_ = 0;
    int camIndex_ = -1;
    std::string backend_, toast_;
};

int main() {
    // Calisma klasoru: exe'nin yani (models/ ve assets/ orada)
    ChangeDirectory(GetApplicationDirectory());

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(VW, VH, "Kinext 1.0");
    SetWindowMinSize(640, 360);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);   // ESC pencereyi kapatmasin, menude geri gitsin

    Fonts fonts;
    fonts.load();
    Texture2D logo{};
    if (FileExists("assets/kinext_logo_white.png")) {
        logo = LoadTexture("assets/kinext_logo_white.png");
        SetTextureFilter(logo, TEXTURE_FILTER_BILINEAR);
    }
    Ui::buildHandTexture();
    RenderTexture2D target = LoadRenderTexture(VW, VH);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    Settings settings;
    settings.load();

    Tracker tracker;
    std::string model = FileExists("models/movenet_thunder.onnx") ? "models/movenet_thunder.onnx" : "models/movenet_lightning.onnx";
    bool trackerOk = tracker.start(model, settings.camera);

    App app(fonts, settings, tracker, trackerOk, logo);

    while (!WindowShouldClose() && !app.quit) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        // Sanal 1280x720 ekrani pencereye sigdir (kenarlarda siyah bant)
        float scale = (std::min)(GetScreenWidth() / static_cast<float>(VW), GetScreenHeight() / static_cast<float>(VH));
        float ox = (GetScreenWidth() - VW * scale) / 2, oy = (GetScreenHeight() - VH * scale) / 2;
        SetMouseOffset(static_cast<int>(-ox), static_cast<int>(-oy));
        SetMouseScale(1 / scale, 1 / scale);

        BeginTextureMode(target);
        ClearBackground(BLACK);
        app.frame((std::min)(GetFrameTime(), 0.1f));
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture, { 0, 0, static_cast<float>(VW), -static_cast<float>(VH) },
                       { ox, oy, VW * scale, VH * scale }, { 0, 0 }, 0, WHITE);
        EndDrawing();
    }

    settings.save();
    tracker.stop();
    UnloadRenderTexture(target);
    if (logo.id != 0) UnloadTexture(logo);
    CloseWindow();
    return 0;
}
