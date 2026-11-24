// Kinext 1.0 - arka planda calisan hareket takibi (kamera + poz modeli kendi is parcaciginda)
// Bu baslik OpenCV/ONNX icermez: raylib gibi kutuphanelerle ayni dosyada rahatca kullanilir.
#pragma once
#include "skeleton.hpp"
#include <memory>
#include <string>
#include <vector>

struct TrackerState {
    // Iskelet 0-1 arasi koordinatlarla ve AYNALANMIS: oyuncu sagina gidince x buyur (ayna gibi)
    Skeleton skeleton;
    bool running = false;
    float fps = 0;
    int camIndex = -1;          // -1: telefon kamerasi kullaniliyor
    bool phone = false;
    std::string phoneUrl;       // telefon kullaniliyorsa adresi
    std::string phoneError;     // son telefon baglanti hatasi
    bool phoneConnecting = false;
    std::string backend;
};

class Tracker {
public:
    Tracker();
    ~Tracker();

    // Modeli yukler, kamerayi acar ve takibi baslatir. Hata olursa false doner, error() aciklar.
    bool start(const std::string& modelPath, int preferredCamera = 0);
    void stop();

    TrackerState state() const;
    std::vector<std::string> cameraNames() const;
    void selectCamera(int index);   // is parcaciginda uygulanir
    void selectPhone(const std::string& address);   // Wi-Fi telefon kamerasi (IP Webcam vb.)
    std::string error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
