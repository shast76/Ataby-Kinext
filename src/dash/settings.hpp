// Kinext 1.0 Dashboard - ayarlar ve skorlar (basit metin dosyasi: kinext_settings.txt)
#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

struct Settings {
    bool setupDone = false;
    int accent = 0;
    int avatar = 0;
    int camera = 0;
    int hand = 0;                        // 0 otomatik, 1 sol, 2 sag
    int lang = 0;                        // 0 Turkce, 1 English
    // Kalibrasyon: kontrol elinin rahatca ulastigi sol-ust ve sag-alt nokta,
    // o elin omzuna gore ve govde boyu biriminde. side: 0 sol el, 1 sag el ile olculdu.
    bool calValid = false;
    static constexpr int CAL_VERSION = 2;   // 2: bilek noktasiyla olculur
    int calVersion = 0;
    int calSide = 1;
    float calTLx = 0, calTLy = 0, calBRx = 0, calBRy = 0;
    // Telefon kamerasi (IP Webcam vb.): bos degilse kamera yerine bu adres kullanilir
    std::string phoneUrl;
    bool usePhone = false;
    std::string name;
    std::string lastGame = "balloon";
    std::map<std::string, int> best;     // oyun id -> en yuksek skor

    static constexpr const char* FILE_NAME = "kinext_settings.txt";

    // Ayar dosyasinin yeri: kurulumdan sonra program "Program Files" altinda olur ve oraya yazilamaz.
    // Bu yuzden kullanicinin kendi klasoru kullanilir: Windows'ta %APPDATA%\Kinext, Linux'ta ~/.config/kinext.
    // Eski surumlerden kalan, exe'nin yanindaki dosya varsa bir kez oradan tasinir.
    static std::filesystem::path path() {
        namespace fs = std::filesystem;
        static fs::path p = [] {
            fs::path dir;
            if (const char* a = std::getenv("APPDATA")) dir = fs::path(a) / "Kinext";
            else if (const char* x = std::getenv("XDG_CONFIG_HOME")) dir = fs::path(x) / "kinext";
            else if (const char* h = std::getenv("HOME")) dir = fs::path(h) / ".config" / "kinext";
            std::error_code ec;
            if (dir.empty() || (!fs::create_directories(dir, ec) && !fs::is_directory(dir, ec))) return fs::path(FILE_NAME);
            fs::path target = dir / FILE_NAME;
            if (!fs::exists(target, ec) && fs::exists(FILE_NAME, ec)) fs::copy_file(FILE_NAME, target, ec);   // eski ayarlari tasi
            return target;
        }();
        return p;
    }

    void load() {
        const char* user = std::getenv("USERNAME");
        if (!user) user = std::getenv("USER");
        name = user ? user : "Oyuncu";
        std::ifstream in(path());
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            try {
                if (k == "setup") setupDone = v == "1";
                else if (k == "accent") accent = std::stoi(v);
                else if (k == "avatar") avatar = std::stoi(v);
                else if (k == "camera") camera = std::stoi(v);
                else if (k == "hand") hand = std::stoi(v);
                else if (k == "lang") lang = std::stoi(v);
                else if (k == "cal") calValid = v == "1";
                else if (k == "cal_v") calVersion = std::stoi(v);
                else if (k == "cal_side") calSide = std::stoi(v);
                else if (k == "cal_tlx") calTLx = std::stof(v);
                else if (k == "cal_tly") calTLy = std::stof(v);
                else if (k == "cal_brx") calBRx = std::stof(v);
                else if (k == "cal_bry") calBRy = std::stof(v);
                else if (k == "phone_url") phoneUrl = v;
                else if (k == "use_phone") usePhone = v == "1";
                else if (k == "name" && !v.empty()) name = v;
                else if (k == "last") lastGame = v;
                else if (k.rfind("best_", 0) == 0) best[k.substr(5)] = std::stoi(v);
            } catch (...) {}
        }
        if (calValid && calVersion != CAL_VERSION) calValid = false;   // eski surum kalibrasyon: yeniden yapilmali
    }

    void save() const {
        std::ofstream out(path());
        out << "setup=" << (setupDone ? 1 : 0) << "\n"
            << "accent=" << accent << "\n"
            << "avatar=" << avatar << "\n"
            << "camera=" << camera << "\n"
            << "hand=" << hand << "\n"
            << "lang=" << lang << "\n"
            << "cal=" << (calValid ? 1 : 0) << "\n"
            << "cal_v=" << CAL_VERSION << "\n"
            << "cal_side=" << calSide << "\n"
            << "cal_tlx=" << calTLx << "\n" << "cal_tly=" << calTLy << "\n"
            << "cal_brx=" << calBRx << "\n" << "cal_bry=" << calBRy << "\n"
            << "phone_url=" << phoneUrl << "\n"
            << "use_phone=" << (usePhone ? 1 : 0) << "\n"
            << "name=" << name << "\n"
            << "last=" << lastGame << "\n";
        for (const auto& [k, v] : best) out << "best_" << k << "=" << v << "\n";
    }

    // Fabrika ayarlarina don. Skor kayitlari silinmez, sifirlanir:
    // oyunlar bu degerlere isaretci tutuyor, harita yeniden olusturulmamali.
    void reset() {
        setupDone = false;
        accent = 0; avatar = 0; camera = 0; hand = 0;
        calValid = false; calSide = 1; calTLx = calTLy = calBRx = calBRy = 0;
        phoneUrl.clear(); usePhone = false;
        lastGame = "balloon";
        for (auto& [k, v] : best) v = 0;
        const char* user = std::getenv("USERNAME");
        if (!user) user = std::getenv("USER");
        name = user ? user : "Oyuncu";
        save();
    }

    int total() const { int t = 0; for (const auto& [k, v] : best) t += v; return t; }
};
