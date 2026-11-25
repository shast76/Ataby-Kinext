// Kinext 1.0 - sesli komutlar
// Windows'un kendi konusma tanima motoru (SAPI) ile tamamen cevrimdisi calisir; internet gerekmez.
// Serbest konusma degil, kisa bir komut listesi dinlenir ("Kinext, play bowling" gibi): bu yuzden
// hizli ve hatasizdir. Ses hicbir yere gonderilmez ve kaydedilmez.
#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

enum class VoiceCmd {
    None, Home, Games, Store, Settings, Back, Again,
    PlayBalloon, PlayDodge, PlayBowling, PlayKeeper, Fullscreen, Calibrate,
};

struct VoiceEvent { VoiceCmd cmd = VoiceCmd::None; std::string heard; };

class Voice {
public:
    ~Voice() { stop(); }

    // lang: 0 Turkce, 1 English. Turkce tanima motoru yoksa Ingilizce komutlar kullanilir.
    void start(int lang);
    void stop();

    bool running() const { return running_; }
    bool starting() const { return starting_; }
    bool englishOnly() const { return english_; }   // Turkce secili ama Turkce motor yok
    std::string status() const { std::lock_guard<std::mutex> l(m_); return status_; }
    std::string error() const { std::lock_guard<std::mutex> l(m_); return error_; }

    // Siradaki komut (yoksa cmd == None)
    VoiceEvent poll() {
        std::lock_guard<std::mutex> l(m_);
        if (queue_.empty()) return {};
        VoiceEvent e = queue_.front(); queue_.pop_front(); return e;
    }

private:
    void run(int lang);
    void push(VoiceCmd c, const std::string& heard) { std::lock_guard<std::mutex> l(m_); queue_.push_back({ c, heard }); }
    void setError(const std::string& e) { std::lock_guard<std::mutex> l(m_); error_ = e; }
    void setStatus(const std::string& s) { std::lock_guard<std::mutex> l(m_); status_ = s; }

    std::thread th_;
    std::atomic<bool> stop_{ false }, running_{ false }, starting_{ false }, english_{ false };
    mutable std::mutex m_;
    std::deque<VoiceEvent> queue_;
    std::string status_, error_;
};
