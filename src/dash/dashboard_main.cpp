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

enum class Scene { PickLanguage, PickColor, PickAvatar, Dashboard, Playing, Calibrate, Phone };
enum class Tab { Home, Games, Store, Settings };

class App {
public:
    App(Fonts& f, Settings& s, Tracker& tr, bool trackerOk, Texture2D logo)
        : f_(f), s_(s), tracker_(tr), trackerOk_(trackerOk), logo_(logo) {
        games_["balloon"] = std::make_unique<BalloonGame>(&s_.best["balloon"]);
        games_["dodge"] = std::make_unique<DodgeGame>(&s_.best["dodge"]);
        cameraNames_ = tracker_.cameraNames();
        setLang(s_.lang);
        scene_ = s_.setupDone ? Scene::Dashboard : Scene::PickLanguage;
    }

    bool quit = false;

    void frame(float dt) {
        time_ += dt;
        setLang(s_.lang);
        TrackerState ts = tracker_.state();
        body_ = ts.skeleton;
        fps_ = ts.fps;
        camIndex_ = ts.camIndex;
        backend_ = ts.backend;
        phoneActive_ = ts.phone;
        phoneUrlNow_ = ts.phoneUrl;
        phoneError_ = ts.phoneError;
        phoneConnecting_ = ts.phoneConnecting;
        accent_ = ACCENTS[std::clamp(s_.accent, 0, ACCENT_COUNT - 1)].color;

        escHandled_ = confirmReset_ && IsKeyPressed(KEY_ESCAPE);
        ui_.setHandMode(s_.hand);
        // Kalibrasyon sirasinda eski olcum kullanilmasin (yanlis olcum imleci kilitleyebilir)
        Ui::Calibration cal;
        if (s_.calValid && !(scene_ == Scene::Calibrate && calStep_ < 3))
            cal = { true, s_.calSide, { s_.calTLx, s_.calTLy }, { s_.calBRx, s_.calBRy } };
        ui_.setCalibration(cal);
        ui_.begin(dt, body_);
        switch (scene_) {
        case Scene::PickLanguage: pickLanguage(); break;
        case Scene::PickColor:  pickColor(); break;
        case Scene::PickAvatar: pickAvatar(); break;
        case Scene::Dashboard:  dashboard(); break;
        case Scene::Playing:    playing(dt); break;
        case Scene::Calibrate:  calibrate(dt); break;
        case Scene::Phone:      phoneScreen(); break;
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
            // Karakterli logo: kafa ustten, ayak alttan tasar; harflerin goze carpan boyu ~34 px olsun
            float h = 50, w = h * logo_.width / static_cast<float>(logo_.height);
            DrawTexturePro(logo_, { 0, 0, static_cast<float>(logo_.width), static_cast<float>(logo_.height) },
                           { VW - 40 - w, 22, w, h }, { 0, 0 }, 0, WHITE);
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

    std::string cameraLabel() const {
        if (phoneActive_) return T("Telefon (", "Phone (") + shortUrl(phoneUrlNow_) + ")";
        return (camIndex_ >= 0 && camIndex_ < static_cast<int>(cameraNames_.size())) ? cameraNames_[camIndex_] : "-";
    }
    static std::string shortUrl(std::string u) {
        if (u.rfind("http://", 0) == 0) u = u.substr(7);
        auto sl = u.find('/');
        return sl == std::string::npos ? u : u.substr(0, sl);
    }

    void statusBar() {
        std::string cam = cameraLabel();
        std::string st = trackerOk_
            ? T("Kamera: ", "Camera: ") + cam + "   ·   " + std::to_string(static_cast<int>(fps_)) + " FPS   ·   " + backend_
            : T("Hareket takibi başlatılamadı: ", "Motion tracking could not start: ") + translateError(tracker_.error());
        text(f_.regular, st, 40, VH - 44, 18, trackerOk_ ? Color{ 140, 140, 150, 255 } : Color{ 255, 120, 110, 255 });
        std::string hint = body_.valid ? T("Elini kaldır ve bir kutunun üstünde tut", "Raise your hand and hold it over a tile") : T("Kameranın önüne geç", "Step in front of the camera");
        text(f_.regular, hint, VW - 40 - textWidth(f_.regular, hint, 18), VH - 44, 18, { 140, 140, 150, 255 });
    }

    void startGame(const std::string& id) {
        auto it = games_.find(id);
        if (it == games_.end()) { toast(T("Bu oyun yakında geliyor!", "This game is coming soon!")); return; }
        current_ = it->second.get();
        currentId_ = id;
        current_->reset();
        saved_ = false;
        s_.lastGame = id;
        s_.save();
        go(Scene::Playing);
    }

    // ---------------- ilk kurulum: renk ve maskot ----------------
    void pickLanguage() {
        background();
        textCenter(f_.light, "Dil seç  ·  Choose your language", VW / 2.f, 110, 52, WHITE);
        const float w = 380, h = 260, gap = 40;
        const char* names[] = { "Türkçe", "English" };
        const char* subs[] = { "Arayüz ve oyunlar Türkçe", "Menus and games in English" };
        for (int i = 0; i < 2; ++i) {
            Rectangle r = { VW / 2.f - w - gap / 2 + i * (w + gap), 230, w, h };
            bool hover = ui_.hovered(r);
            DrawRectangleRec(r, hover ? lighten(ACCENTS[0].color, 0.12f) : ACCENTS[0].color);
            textCenter(f_.light, names[i], r.x + w / 2, r.y + 70, 64, WHITE);
            textCenter(f_.regular, subs[i], r.x + w / 2, r.y + 170, 22, { 225, 230, 240, 255 });
            if (hover || i == s_.lang) DrawRectangleLinesEx({ r.x - 3, r.y - 3, w + 6, h + 6 }, 3, WHITE);
            if (ui_.select(std::string("lang") + std::to_string(i), r)) {
                s_.lang = i;
                setLang(i);
                s_.save();
                go(s_.setupDone ? Scene::Dashboard : Scene::PickColor);
            }
        }
        statusBar();
    }

    void pickColor() {
        background();
        textCenter(f_.light, s_.setupDone ? T("Vurgu rengini seç", "Choose your accent color") : T("Hoş geldin! Önce rengini seç", "Welcome! First, pick your color"), VW / 2.f, 70, 56, WHITE);
        textCenter(f_.regular, T("Kutular ve menüler bu renge bürünecek", "Tiles and menus will take on this color"), VW / 2.f, 150, 24, { 170, 170, 180, 255 });
        const float sz = 170, gap = 18;
        const float x0 = (VW - (5 * sz + 4 * gap)) / 2;
        for (int i = 0; i < ACCENT_COUNT; ++i) {
            Rectangle r = { x0 + (i % 5) * (sz + gap), 220 + (i / 5) * (sz + gap), sz, sz };
            bool hover = ui_.hovered(r);
            drawTile(f_, r, ACCENTS[i].color, ACCENTS[i].name(), Icon::None, hover || i == s_.accent, 22);
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
        textCenter(f_.light, T("Maskotunu seç", "Choose your mascot"), VW / 2.f, 60, 56, WHITE);
        textCenter(f_.regular, T("Oyunlarda kamera görüntüsü yerine bu maskot olarak görüneceksin", "In games you'll appear as this mascot instead of the camera image"), VW / 2.f, 140, 24, { 170, 170, 180, 255 });
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
                if (first) toast(std::string(T("Hoş geldin ", "Welcome ")) + s_.name + "!");
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
        const char* names[] = { T("Ana Sayfa", "Home"), T("Oyunlar", "Games"), T("Mağaza", "Store"), T("Ayarlar", "Settings") };
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
        return b > 0 ? T("Rekor: ", "Best: ") + std::to_string(b) : T("Henüz oynanmadı", "Not played yet");
    }

    void home() {
        // Profil kutusu: canli maskot + skorlar
        Rectangle prof = { 40, 170, 210, 430 };
        DrawRectangleRec(prof, accent_);
        liveAvatar({ prof.x, prof.y + 10, prof.width, 260 }, 58);
        text(f_.bold, s_.name, prof.x + 14, prof.y + 300, 26, WHITE);
        text(f_.regular, T("Toplam puan", "Total score"), prof.x + 14, prof.y + 340, 18, { 230, 230, 235, 255 });
        text(f_.bold, std::to_string(s_.total()), prof.x + 14, prof.y + 362, 34, WHITE);

        // Buyuk kutu: son oynanan oyun
        const GameInfo* last = findGame(s_.lastGame);
        if (!last || !last->installed) last = &catalog()[0];
        if (gameTile(last->id, { 266, 170, 560, 315 }, last->title(), 34, T("Devam et  ·  ", "Continue  ·  ") + bestText(last->id))) startGame(last->id);

        // Sag kutular
        Rectangle tGames = { 842, 170, 150, 150 }, tStore = { 842, 335, 150, 150 };
        drawTile(f_, tGames, accent_, T("Oyunlarım", "My games"), Icon::Games, ui_.hovered(tGames));
        if (ui_.select("home_games", tGames)) tab_ = Tab::Games;
        drawTile(f_, tStore, accent_, T("Mağaza", "Store"), Icon::Store, ui_.hovered(tStore));
        if (ui_.select("home_store", tStore)) tab_ = Tab::Store;

        // Alt sira
        Rectangle tSet = { 266, 501, 133, 99 }, tAv = { 414, 501, 133, 99 }, tCam = { 562, 501, 133, 99 };
        drawTile(f_, tSet, accent_, T("Ayarlar", "Settings"), Icon::Settings, ui_.hovered(tSet), 17);
        if (ui_.select("home_settings", tSet)) tab_ = Tab::Settings;
        drawTile(f_, tAv, accent_, T("Maskot", "Mascot"), Icon::Avatar, ui_.hovered(tAv), 17);
        if (ui_.select("home_avatar", tAv)) go(Scene::PickAvatar);
        drawTile(f_, tCam, accent_, T("Kamera", "Camera"), Icon::Camera, ui_.hovered(tCam), 17);
        if (ui_.select("home_cam", tCam)) nextCamera();
        // Diger kurulu oyun
        for (const auto& g : catalog())
            if (g.installed && last->id != std::string(g.id)) {
                if (gameTile(g.id, { 710, 501, 282, 99 }, g.title(), 20)) startGame(g.id);
                break;
            }

        // One cikanlar (magazadan)
        text(f_.regular, T("ÖNE ÇIKANLAR", "FEATURED"), 1012, 168, 16, { 160, 160, 170, 255 });
        int k = 0;
        for (const auto& g : catalog()) {
            if (g.installed) continue;
            Rectangle r = { 1012, 196.f + k * 206, 228, 194 };
            if (gameTile(g.id, r, g.title(), 22, T("Yakında", "Coming soon"))) { tab_ = Tab::Store; toast(std::string(g.title()) + T(" yakında geliyor!", " is coming soon!")); }
            if (++k == 2) break;
        }
    }

    void gamesTab() {
        text(f_.regular, T("Yüklü oyunlar", "Installed games"), 40, 168, 22, { 170, 170, 180, 255 });
        int i = 0;
        for (const auto& g : catalog()) {
            if (!g.installed) continue;
            Rectangle r = { 40.f + i * 400, 206, 380, 260 };
            if (gameTile(g.id, r, g.title(), 28, bestText(g.id))) startGame(g.id);
            ++i;
        }
        Rectangle more = { 40.f + i * 400, 206, 200, 260 };
        drawTile(f_, more, ColorAlpha(accent_, 0.6f), T("Daha fazla oyun", "More games"), Icon::Store, ui_.hovered(more));
        if (ui_.select("games_more", more)) tab_ = Tab::Store;
    }

    void storeTab() {
        text(f_.regular, T("Kinext Mağaza  ·  hareketle oynanan oyunlar", "Kinext Store  ·  games you play with your body"), 40, 168, 22, { 170, 170, 180, 255 });
        int i = 0;
        for (const auto& g : catalog()) {
            Rectangle r = { 40.f + (i % 4) * 300, 206.f + (i / 4) * 222, 280, 200 };
            if (gameTile(g.id, r, g.title(), 22, g.desc())) {
                if (g.installed) startGame(g.id);
                else toast(std::string(g.title()) + T(" yakında geliyor!", " is coming soon!"));
            }
            // Rozet
            const char* badge = g.installed ? T("YÜKLÜ", "INSTALLED") : T("YAKINDA", "SOON");
            float bw = textWidth(f_.bold, badge, 15) + 18;
            DrawRectangleRec({ r.x + r.width - bw - 8, r.y + 8, bw, 26 }, g.installed ? accent_ : Color{ 60, 60, 66, 230 });
            text(f_.bold, badge, r.x + r.width - bw + 1, r.y + 12, 15, WHITE);
            ++i;
        }
    }

    void settingsTab() {
        const float w = 200, h = 140, gap = 18;
        struct Item { const char* id; std::string label; Icon icon; };
        Item items[] = {
            { "set_color", T("Vurgu rengi", "Accent color"), Icon::Color },
            { "set_avatar", T("Maskot", "Mascot"), Icon::Avatar },
            { "set_cam", T("Kamera", "Camera"), Icon::Camera },
            { "set_hand", std::string(T("Kontrol eli: ", "Control hand: ")) + Ui::handModeName(s_.hand), Icon::Hand },
            { "set_cal", T("Kalibrasyon", "Calibration"), Icon::Target },
            { "set_phone", T("Telefon kamerası", "Phone camera"), Icon::Phone },
            { "set_lang", "Dil / Language", Icon::Globe },
            { "set_full", IsWindowFullscreen() || IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? T("Pencere modu", "Windowed") : T("Tam ekran", "Full screen"), Icon::Play },
            { "set_reset", T("Ayarları sıfırla", "Reset settings"), Icon::Reset },
            { "set_quit", T("Kapat", "Quit"), Icon::Power },
        };
        const int n = static_cast<int>(sizeof(items) / sizeof(items[0]));
        for (int i = 0; i < n; ++i) {
            Rectangle r = { 40 + (i % 4) * (w + gap), 176 + (i / 4) * (h + gap), w, h };
            Color bg = i >= 8 ? Color{ 70, 70, 78, 255 } : accent_;
            drawTile(f_, r, bg, items[i].label, items[i].icon, ui_.hovered(r), 17);
            if (!ui_.select(items[i].id, r)) continue;
            switch (i) {
            case 0: go(Scene::PickColor); break;
            case 1: go(Scene::PickAvatar); break;
            case 2: nextCamera(); break;
            case 3:   // Otomatik -> Sol -> Sag -> Otomatik
                s_.hand = (s_.hand + 1) % 3;
                s_.save();
                toast(std::string(T("Kontrol eli: ", "Control hand: ")) + Ui::handModeName(s_.hand));
                break;
            case 4: startCalibration(); break;
            case 5: phoneInput_ = s_.phoneUrl.empty() ? phoneInput_ : shortUrl(s_.phoneUrl); go(Scene::Phone); break;
            case 6: go(Scene::PickLanguage); break;
            case 7: ToggleBorderlessWindowed(); break;
            case 8: confirmReset_ = true; break;
            case 9: s_.save(); quit = true; break;
            }
        }
        // Bilgi (sagda)
        float ix = 40 + 4 * (w + gap) + 10;
        auto info = [&](float y, const std::string& k, const std::string& v) {
            text(f_.regular, k, ix, y, 18, { 140, 140, 150, 255 });
            text(f_.regular, v, ix, y + 22, 21, { 220, 220, 228, 255 });
        };
        info(176, T("Kamera", "Camera"), cameraLabel());
        info(234, T("Vurgu rengi", "Accent color"), ACCENTS[s_.accent].name());
        info(292, T("Maskot", "Mascot"), avatarName(s_.avatar));
        info(350, T("Kontrol eli", "Control hand"), Ui::handModeName(s_.hand));
        info(466, "Dil / Language", s_.lang == LANG_EN ? "English" : "Türkçe");
        info(408, T("Kalibrasyon", "Calibration"), s_.calValid ? std::string(T("Sana özel", "Personal")) : std::string(T("Varsayılan", "Default")));
        text(f_.regular, "Kinext 1.0", ix, 560, 18, { 120, 120, 130, 255 });
        text(f_.regular, T("Microsoft ile bağlantılı değildir", "Not affiliated with Microsoft"), ix, 584, 16, { 110, 110, 120, 255 });
    }

    // Sifirlama onay penceresi (arkadaki her sey kilitli)
    void resetDialog() {
        DrawRectangle(0, 0, VW, VH, { 0, 0, 0, 170 });
        Rectangle box = { VW / 2.f - 360, VH / 2.f - 170, 720, 340 };
        DrawRectangleRec(box, { 30, 30, 36, 255 });
        DrawRectangle(static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.width), 6, accent_);
        textCenter(f_.light, T("Ayarlar sıfırlansın mı?", "Reset all settings?"), VW / 2.f, box.y + 34, 44, WHITE);
        textCenter(f_.regular, T("Vurgu rengi, maskot, kamera, kontrol eli ve", "Your accent color, mascot, camera, control hand and"), VW / 2.f, box.y + 104, 22, { 200, 200, 210, 255 });
        textCenter(f_.regular, T("tüm rekorların silinip ilk kurulum yeniden başlayacak.", "all high scores will be erased and setup will start over."), VW / 2.f, box.y + 134, 22, { 200, 200, 210, 255 });

        ui_.setBlocked(false);
        Rectangle yes = { VW / 2.f - 300, box.y + 210, 280, 84 }, no = { VW / 2.f + 20, box.y + 210, 280, 84 };
        drawTile(f_, yes, { 190, 40, 50, 255 }, "", Icon::None, ui_.hovered(yes));
        textCenter(f_.regular, T("Evet, sıfırla", "Yes, reset"), yes.x + yes.width / 2, yes.y + 26, 28, WHITE);
        drawTile(f_, no, { 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(no));
        textCenter(f_.regular, T("Vazgeç", "Cancel"), no.x + no.width / 2, no.y + 26, 28, WHITE);
        // Yanlislikla sifirlanmasin: "evet" icin daha uzun tutmak gerekir
        if (ui_.select("reset_yes", yes, 1.8f)) {
            confirmReset_ = false;
            s_.reset();
            tracker_.selectCamera(0);
            tab_ = Tab::Home;
            go(Scene::PickColor);
            toast(T("Ayarlar sıfırlandı", "Settings reset"));
        } else if (ui_.select("reset_no", no) || IsKeyPressed(KEY_ESCAPE)) {
            confirmReset_ = false;
        }
    }

    void nextCamera() {
        if (cameraNames_.empty()) { toast(T("Kamera bulunamadı", "No camera found")); return; }
        if (phoneActive_ || s_.usePhone) {   // telefondan bilgisayar kamerasina don
            s_.usePhone = false;
            tracker_.selectCamera(std::clamp(s_.camera, 0, static_cast<int>(cameraNames_.size()) - 1));
            s_.save();
            toast(T("Kamera: ", "Camera: ") + cameraNames_[std::clamp(s_.camera, 0, static_cast<int>(cameraNames_.size()) - 1)]);
            return;
        }
        int n = static_cast<int>(cameraNames_.size());
        int next = ((camIndex_ < 0 ? 0 : camIndex_) + 1) % n;
        tracker_.selectCamera(next);
        s_.camera = next;
        s_.save();
        toast(T("Kamera: ", "Camera: ") + cameraNames_[next]);
    }

    // ---------------- kalibrasyon ----------------
    // 0: vucut kadrajda mi  1: sol-ust nokta  2: sag-alt nokta  3: sonuc
    void startCalibration() { calStep_ = 0; calTimer_ = 0; calHistory_.clear(); calMsg_.clear(); go(Scene::Calibrate); }

    // Elin kalibrasyon icin kullanilacak tarafi (ayara gore; otomatikte o an hangisi yukaridaysa)
    int calibrationSide() const {
        if (s_.hand == 1) return 0;
        if (s_.hand == 2) return 1;
        if (calSideLocked_ >= 0) return calSideLocked_;
        const JointPos& l = body_[HandLeft]; const JointPos& r = body_[HandRight];
        if (l.state == TrackState::NotTracked) return 1;
        if (r.state == TrackState::NotTracked) return 0;
        return l.y < r.y ? 0 : 1;
    }

    void calibrate(float dt) {
        background();
        textCenter(f_.light, T("Kalibrasyon", "Calibration"), VW / 2.f, 40, 52, WHITE);
        // Canli maskot (solda)
        Rectangle stage = { 60, 150, 300, 440 };
        DrawRectangleRec(stage, ColorAlpha(accent_, 0.25f));
        liveAvatar({ stage.x, stage.y + 40, stage.width, 330 }, 80);

        const float tx = 420;   // yazi sutunu
        auto line = [&](float y, const std::string& t, Color c = { 220, 220, 228, 255 }, float size = 26) { text(f_.regular, t, tx, y, size, c); };
        Color ok = { 120, 220, 120, 255 }, wait = { 150, 150, 160, 255 };

        if (calStep_ == 0) {
            bool bodyOk = body_.valid;
            bool headOk = bodyOk && body_[Head].state != TrackState::NotTracked;
            bool feetOk = bodyOk && (body_[AnkleLeft].state == TrackState::Tracked || body_[AnkleRight].state == TrackState::Tracked);
            line(160, T("1. Kameranın önüne geç", "1. Step in front of the camera"), WHITE, 32);
            line(206, T("Başından ayaklarına kadar görünecek kadar geride dur.", "Stand back far enough to be seen from head to toe."));
            line(258, std::string(bodyOk ? "●  " : "○  ") + T("Gövde", "Body"), bodyOk ? ok : wait);
            line(294, std::string(headOk ? "●  " : "○  ") + T("Baş", "Head"), headOk ? ok : wait);
            line(330, std::string(feetOk ? "●  " : "○  ") + T("Ayaklar", "Feet") + (feetOk ? "" : T("  (önerilir, şart değil)", "  (recommended, not required)")), feetOk ? ok : wait);
            calTimer_ = (bodyOk && headOk) ? calTimer_ + dt : 0;
            if (calTimer_ > 0) {
                DrawRectangleRec({ tx, 390, 500, 10 }, { 60, 60, 66, 255 });
                DrawRectangleRec({ tx, 390, 500 * std::clamp(calTimer_ / 1.5f, 0.f, 1.f), 10 }, accent_);
            }
            if (calTimer_ >= 1.5f) { calStep_ = 1; calTimer_ = 0; calHistory_.clear(); calSideLocked_ = -1; }
        } else if (calStep_ == 1 || calStep_ == 2) {
            bool tl = calStep_ == 1;
            line(160, tl ? T("2. Sol üst köşe", "2. Top-left corner") : T("3. Sağ alt köşe", "3. Bottom-right corner"), WHITE, 32);
            line(206, std::string(T("Kontrol elini (", "Reach with your control hand (")) + (s_.hand == 1 ? T("sol el", "left hand") : s_.hand == 2 ? T("sağ el", "right hand") : T("hangisi rahatsa", "either hand")) + T(") rahatça uzanabildiğin", ") as far as"));
            line(240, tl ? T("en sol üst noktaya götür ve orada tut.", "you comfortably can to the top-left, then hold.") : T("en sağ alt noktaya götür ve orada tut.", "you comfortably can to the bottom-right, then hold."));
            line(274, T("Zorlama: burası imlecin ekranın köşesine ulaşacağı yer olacak.", "Don't strain: this is where the cursor will reach the screen corner."), { 160, 160, 170, 255 }, 20);
            // Hedef koseyi goster
            Vector2 corner = tl ? Vector2{ 70, 70 } : Vector2{ VW - 70.f, VH - 70.f };
            float pulse = 0.5f + 0.5f * std::sin(time_ * 5.f);
            DrawRing(corner, 34, 42 + pulse * 6, 0, 360, 48, accent_);
            drawIcon(Icon::Target, corner, 44, WHITE);

            int side = calibrationSide();
            const JointPos& hand = body_[side == 0 ? HandLeft : HandRight];
            bool handOk = body_.valid && hand.state != TrackState::NotTracked;
            if (handOk) {
                Vector2 rel = Ui::handRel(body_, side);
                calHistory_.push_back({ time_, rel });
                while (!calHistory_.empty() && time_ - calHistory_.front().t > 0.8f) calHistory_.erase(calHistory_.begin());
                // El sabit mi? (son 0.8 sn deki oynama govde boyunun %13 unden az)
                float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
                for (const auto& h : calHistory_) { minx = (std::min)(minx, h.rel.x); maxx = (std::max)(maxx, h.rel.x); miny = (std::min)(miny, h.rel.y); maxy = (std::max)(maxy, h.rel.y); }
                bool still = calHistory_.size() > 5 && (maxx - minx) < 0.13f && (maxy - miny) < 0.13f;
                // Yonu de kontrol et: sol-ust icin el omzun ustunde/yaninda olmali
                bool plausible = tl ? rel.y < 0.4f : rel.y > -0.6f;
                calTimer_ = (still && plausible) ? calTimer_ + dt : 0;
                if (calTimer_ > 0 && s_.hand == 0) calSideLocked_ = side;   // otomatikte olcumu baslatan eli kilitle
                // Elin nerede oldugunu gosteren nokta + ilerleme halkasi
                Vector2 dot = { std::clamp(0.5f + rel.x / 3.f, 0.03f, 0.97f) * VW, std::clamp(0.5f + rel.y / 2.4f, 0.05f, 0.95f) * VH };
                Ui::drawHandShape(dot, 1.f, ColorAlpha(WHITE, 0.85f), side == 0);
                if (calTimer_ > 0) DrawRing(dot, 40, 46, -90, -90 + 360 * std::clamp(calTimer_ / 1.2f, 0.f, 1.f), 48, WHITE);
                if (calTimer_ >= 1.2f) {
                    Vector2 avg{};
                    for (const auto& h : calHistory_) { avg.x += h.rel.x; avg.y += h.rel.y; }
                    avg.x /= calHistory_.size(); avg.y /= calHistory_.size();
                    if (tl) { calTL_ = avg; calStep_ = 2; }
                    else    { calBR_ = avg; finishCalibration(side); }
                    calTimer_ = 0; calHistory_.clear();
                }
            } else {
                calTimer_ = 0;
                line(330, T("Elini göremiyorum, kaldır.", "I can't see your hand, raise it."), { 255, 170, 90, 255 });
            }
        } else {
            // Sonuc
            bool good = calMsg_.empty();
            line(160, good ? T("Kalibrasyon tamam!", "Calibration complete!") : T("Tekrar deneyelim", "Let's try again"), good ? ok : Color{ 255, 140, 110, 255 }, 32);
            if (good) {
                line(206, T("İmleç artık senin hareket alanına göre ayarlı.", "The cursor is now tuned to your range of motion."));
                line(240, T("Elini kaldırıp köşelere ulaşabildiğini dene.", "Raise your hand and check that you can reach the corners."), { 160, 160, 170, 255 }, 22);
            } else {
                line(206, calMsg_);
            }
        }

        // Alt butonlar
        Rectangle back = { tx, 520, 230, 70 }, redo = { tx + 250, 520, 230, 70 }, def = { tx + 500, 520, 300, 70 };
        drawTile(f_, back, calStep_ == 3 ? accent_ : Color{ 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(back));
        textCenter(f_.regular, calStep_ == 3 ? T("Bitti", "Done") : T("Vazgeç", "Cancel"), back.x + back.width / 2, back.y + 20, 26, WHITE);
        if (ui_.select("cal_back", back)) { tab_ = Tab::Settings; go(Scene::Dashboard); }
        if (calStep_ == 3) {
            drawTile(f_, redo, { 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(redo));
            textCenter(f_.regular, T("Tekrar", "Again"), redo.x + redo.width / 2, redo.y + 20, 26, WHITE);
            if (ui_.select("cal_redo", redo)) startCalibration();
        }
        if (s_.calValid) {
            Rectangle d = calStep_ == 3 ? def : redo;
            drawTile(f_, d, { 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(d));
            textCenter(f_.regular, T("Varsayılana dön", "Use default"), d.x + d.width / 2, d.y + 20, 26, WHITE);
            if (ui_.select("cal_default", d)) {
                s_.calValid = false; s_.save();
                toast(T("Kalibrasyon kaldırıldı, varsayılan alan kullanılıyor", "Calibration removed, using the default area"));
                tab_ = Tab::Settings; go(Scene::Dashboard);
            }
        }
        statusBar();
    }

    void finishCalibration(int side) {
        // Olcum mantikli mi? Alan cok kucukse ya da koseler ters secildiyse tekrar istenir
        float w = calBR_.x - calTL_.x, h = calBR_.y - calTL_.y;
        calStep_ = 3;
        if (w < 0.45f || h < 0.35f) {
            calMsg_ = w < 0.45f ? T("Sol ve sağ nokta birbirine çok yakın, elini daha geniş aç.", "The left and right points are too close; move your hand further apart.") : T("Üst ve alt nokta birbirine çok yakın, elini daha çok yukarı-aşağı götür.", "The top and bottom points are too close; move your hand further up and down.");
            return;
        }
        calMsg_.clear();
        s_.calValid = true; s_.calSide = side;
        s_.calTLx = calTL_.x; s_.calTLy = calTL_.y; s_.calBRx = calBR_.x; s_.calBRy = calBR_.y;
        s_.save();
    }

    // ---------------- telefon kamerasi ----------------
    void phoneScreen() {
        background();
        textCenter(f_.light, T("Telefon kamerası", "Phone camera"), VW / 2.f, 40, 52, WHITE);
        const float x = 60;
        Color dim = { 170, 170, 180, 255 };

        text(f_.bold, T("Yol 1: Telefonu webcam gibi kullanan uygulama", "Option 1: An app that turns your phone into a webcam"), x, 140, 24, WHITE);
        text(f_.regular, T("Windows Telefon Bağlantısı, DroidCam ya da Iriun telefonunu bilgisayarda normal bir", "Windows Phone Link, DroidCam or Iriun make your phone show up as a regular webcam"), x, 176, 20, dim);
        text(f_.regular, T("webcam gibi gösterir. Kurduktan sonra Ayarlar → Kamera ile seçmen yeterli.", "on your PC. After installing, just pick it in Settings → Camera."), x, 202, 20, dim);

        text(f_.bold, T("Yol 2: Wi-Fi üzerinden (IP Webcam vb.)", "Option 2: Over Wi-Fi (IP Webcam, etc.)"), x, 262, 24, WHITE);
        text(f_.regular, T("Telefona IP Webcam (Android) gibi bir uygulama kur ve sunucuyu başlat. Ekranda yazan", "Install an app like IP Webcam (Android) and start its server. Type the address it"), x, 298, 20, dim);
        text(f_.regular, T("adresi aşağıya yaz (ör. 192.168.1.35:8080). Telefon ve bilgisayar aynı Wi-Fi'da olmalı.", "shows below (e.g. 192.168.1.35:8080). The phone and PC must be on the same Wi-Fi."), x, 324, 20, dim);

        // Adres alani (klavye ile yazilir, Ctrl+V yapistirir, Enter baglanir)
        Rectangle field = { x, 372, 640, 64 };
        bool fieldHover = ui_.hovered(field);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) phoneEditing_ = fieldHover;
        int ch;
        while ((ch = GetCharPressed()) > 0) {
            phoneEditing_ = true;
            if (ch >= 32 && ch < 127 && phoneInput_.size() < 120) phoneInput_ += static_cast<char>(ch);
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !phoneInput_.empty()) phoneInput_.pop_back();
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
            const char* clip = GetClipboardText();
            if (clip) for (const char* c = clip; *c && phoneInput_.size() < 120; ++c) if (*c >= 32 && *c < 127) phoneInput_ += *c;
        }
        DrawRectangleRec(field, { 40, 40, 46, 255 });
        DrawRectangleLinesEx(field, 2, phoneEditing_ ? accent_ : Color{ 90, 90, 100, 255 });
        std::string shown = phoneInput_.empty() && !phoneEditing_ ? "192.168.1.35:8080" : phoneInput_;
        text(f_.regular, shown, field.x + 18, field.y + 16, 30, phoneInput_.empty() ? Color{ 110, 110, 120, 255 } : WHITE);
        if (phoneEditing_ && std::fmod(time_, 1.f) < 0.55f) {
            float cx = field.x + 18 + textWidth(f_.regular, phoneInput_, 30) + 2;
            DrawRectangleRec({ cx, field.y + 14, 3, 36 }, WHITE);
        }

        Rectangle connect = { x + 660, 372, 200, 64 };
        drawTile(f_, connect, accent_, "", Icon::None, ui_.hovered(connect));
        textCenter(f_.regular, phoneConnecting_ ? T("Bağlanıyor…", "Connecting…") : T("Bağlan", "Connect"), connect.x + connect.width / 2, connect.y + 17, 26, WHITE);
        bool doConnect = ui_.select("phone_connect", connect) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        if (doConnect && !phoneConnecting_ && !phoneInput_.empty()) {
            tracker_.selectPhone(phoneInput_);
            phonePending_ = true;
            phoneEditing_ = false;
        }

        // Durum
        float sy = 460;
        if (phoneConnecting_) {
            text(f_.regular, T("Bağlanıyor: ", "Connecting: ") + MjpegLabel(phoneInput_), x, sy, 22, { 220, 220, 228, 255 });
        } else if (!phoneError_.empty()) {
            text(f_.regular, translateError(phoneError_), x, sy, 22, { 255, 130, 110, 255 });
            phonePending_ = false;
        } else if (phoneActive_) {
            text(f_.regular, T("Bağlandı: ", "Connected: ") + shortUrl(phoneUrlNow_) + "   ·   " + std::to_string(static_cast<int>(fps_)) + " FPS", x, sy, 22, { 120, 220, 120, 255 });
            if (phonePending_) {   // basarili baglantiyi kaydet: program acilinca yine telefona baglanir
                s_.phoneUrl = phoneUrlNow_; s_.usePhone = true; s_.save();
                phonePending_ = false;
                toast(T("Telefon kamerası bağlandı", "Phone camera connected"));
            }
        }

        // Canli onizleme: maskot hareket ediyorsa goruntu geliyor demektir
        Rectangle stage = { 960, 140, 260, 360 };
        DrawRectangleRec(stage, ColorAlpha(accent_, 0.25f));
        liveAvatar({ stage.x, stage.y + 30, stage.width, 280 }, 64);
        textCenter(f_.regular, body_.valid ? T("Seni görüyorum", "I can see you") : T("Seni göremiyorum", "I can't see you"), stage.x + stage.width / 2, stage.y + stage.height - 34, 20, body_.valid ? Color{ 120, 220, 120, 255 } : dim);

        Rectangle back = { x, 540, 230, 70 }, pc = { x + 250, 540, 360, 70 };
        drawTile(f_, back, { 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(back));
        textCenter(f_.regular, T("Geri", "Back"), back.x + back.width / 2, back.y + 20, 26, WHITE);
        if (ui_.select("phone_back", back)) { tab_ = Tab::Settings; go(Scene::Dashboard); }
        if (phoneActive_ || s_.usePhone) {
            drawTile(f_, pc, { 70, 70, 78, 255 }, "", Icon::None, ui_.hovered(pc));
            textCenter(f_.regular, T("Bilgisayar kamerasına dön", "Switch back to PC camera"), pc.x + pc.width / 2, pc.y + 20, 26, WHITE);
            if (ui_.select("phone_pc", pc)) nextCamera();
        }
        statusBar();
    }
    static std::string MjpegLabel(const std::string& in) { return in.empty() ? "-" : in; }

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
    // telefon kamerasi
    bool phoneActive_ = false, phoneConnecting_ = false, phonePending_ = false, phoneEditing_ = false;
    std::string phoneUrlNow_, phoneError_, phoneInput_;
    // kalibrasyon
    struct CalSample { float t; Vector2 rel; };
    int calStep_ = 0, calSideLocked_ = -1;
    float calTimer_ = 0;
    std::vector<CalSample> calHistory_;
    Vector2 calTL_{}, calBR_{};
    std::string calMsg_;
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
    if (FileExists("assets/kinext_icon.png")) {   // pencere ikonu (sol ust kose, Alt+Tab)
        Image icon = LoadImage("assets/kinext_icon.png");
        ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

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
    if (trackerOk && settings.usePhone && !settings.phoneUrl.empty()) tracker.selectPhone(settings.phoneUrl);   // son kullanilan telefon

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
