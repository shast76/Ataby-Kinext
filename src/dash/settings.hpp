// Kinext 1.0 Dashboard - ayarlar ve skorlar (basit metin dosyasi: kinext_settings.txt)
#pragma once
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>

struct Settings {
    bool setupDone = false;
    int accent = 0;
    int avatar = 0;
    int camera = 0;
    int hand = 0;                        // 0 otomatik, 1 sol, 2 sag
    std::string name;
    std::string lastGame = "balloon";
    std::map<std::string, int> best;     // oyun id -> en yuksek skor

    static constexpr const char* FILE_NAME = "kinext_settings.txt";

    void load() {
        const char* user = std::getenv("USERNAME");
        if (!user) user = std::getenv("USER");
        name = user ? user : "Oyuncu";
        std::ifstream in(FILE_NAME);
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
                else if (k == "name" && !v.empty()) name = v;
                else if (k == "last") lastGame = v;
                else if (k.rfind("best_", 0) == 0) best[k.substr(5)] = std::stoi(v);
            } catch (...) {}
        }
    }

    void save() const {
        std::ofstream out(FILE_NAME);
        out << "setup=" << (setupDone ? 1 : 0) << "\n"
            << "accent=" << accent << "\n"
            << "avatar=" << avatar << "\n"
            << "camera=" << camera << "\n"
            << "hand=" << hand << "\n"
            << "name=" << name << "\n"
            << "last=" << lastGame << "\n";
        for (const auto& [k, v] : best) out << "best_" << k << "=" << v << "\n";
    }

    // Fabrika ayarlarina don. Skor kayitlari silinmez, sifirlanir:
    // oyunlar bu degerlere isaretci tutuyor, harita yeniden olusturulmamali.
    void reset() {
        setupDone = false;
        accent = 0; avatar = 0; camera = 0; hand = 0;
        lastGame = "balloon";
        for (auto& [k, v] : best) v = 0;
        const char* user = std::getenv("USERNAME");
        if (!user) user = std::getenv("USER");
        name = user ? user : "Oyuncu";
        save();
    }

    int total() const { int t = 0; for (const auto& [k, v] : best) t += v; return t; }
};
