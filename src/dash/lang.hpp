// Kinext 1.0 - dil destegi (Turkce / English)
// Her arayuz yazisi T("Turkce", "English") ile yazilir; o an secili dile gore biri dondurulur.
#pragma once
#include <string>

enum Lang { LANG_TR = 0, LANG_EN = 1 };

inline int& currentLang() { static int lang = LANG_TR; return lang; }
inline void setLang(int l) { currentLang() = (l == LANG_EN) ? LANG_EN : LANG_TR; }
inline const char* T(const char* tr, const char* en) { return currentLang() == LANG_EN ? en : tr; }

// Kamera/telefon/model katmanindan gelen hata mesajlari (orada Turkce karaktersiz yazili) -> secili dil
inline std::string translateError(const std::string& e) {
    struct M { const char* key; const char* tr; const char* en; };
    static const M map[] = {
        { "Adres bos", "Adres boş", "Address is empty" },
        { "Ag baslatilamadi", "Ağ başlatılamadı", "Network could not be started" },
        { "Adres bulunamadi: ", "Adres bulunamadı: ", "Address not found: " },
        { "Soket acilamadi", "Bağlantı açılamadı", "Could not open a connection" },
        { "Baglanilamadi", "Bağlanılamadı (telefon ve bilgisayar aynı Wi-Fi'da mı?)", "Could not connect (are the phone and PC on the same Wi-Fi?)" },
        { "Istek gonderilemedi", "İstek gönderilemedi", "Request could not be sent" },
        { "Adres bulundu ama", "Adres bulundu ama görüntü yolu yanlış (genelde sonu /video olur)", "Address found but the video path is wrong (it usually ends with /video)" },
        { "Goruntu alinamadi", "Görüntü alınamadı", "No image received" },
        { "Baglanti koptu", "Bağlantı koptu", "Connection lost" },
        { "Telefon bağlantısı koptu: ", "Telefon bağlantısı koptu: ", "Phone connection lost: " },
        { "Kamera bulunamadi", "Kamera bulunamadı", "No camera found" },
        { "Model yuklenemedi: ", "Model yüklenemedi: ", "Model could not be loaded: " },
        // sesli komut
        { "Sesli komut sadece Windows'ta calisir", "Sesli komut sadece Windows'ta çalışır", "Voice commands only work on Windows" },
        { "Konusma tanima baslatilamadi", "Konuşma tanıma başlatılamadı", "Speech recognition could not start" },
        { "Konusma tanima motoru yok (Windows Ayarlar > Zaman ve dil > Konusma)", "Konuşma tanıma motoru yok (Windows Ayarlar > Zaman ve dil > Konuşma)", "No speech recognizer installed (Windows Settings > Time & language > Speech)" },
        { "Konusma tanima motoru acilamadi", "Konuşma tanıma motoru açılamadı", "Speech recognizer could not be opened" },
        { "Mikrofon bulunamadi", "Mikrofon bulunamadı", "No microphone found" },
        { "Mikrofon acilamadi (Gizlilik > Mikrofon iznini kontrol et)", "Mikrofon açılamadı (Gizlilik > Mikrofon iznini kontrol et)", "Could not open the microphone (check Privacy > Microphone)" },
        { "Komut listesi olusturulamadi", "Komut listesi oluşturulamadı", "Could not build the command list" },
        { "Dinleme baslatilamadi", "Dinleme başlatılamadı", "Could not start listening" },
    };
    for (const auto& m : map) {
        std::string k = m.key;
        if (e.rfind(k, 0) != 0) continue;
        std::string rest = e.substr(k.size());
        bool fullMessage = k == "Baglanilamadi" || k == "Adres bulundu ama";   // anahtar sadece baslangic, gerisi sabit
        if (fullMessage) rest.clear();
        if (k == "Telefon bağlantısı koptu: ") rest = translateError(rest);
        return std::string(T(m.tr, m.en)) + rest;
    }
    return e;
}
