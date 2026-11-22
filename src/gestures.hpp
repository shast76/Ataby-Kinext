// Kinext 1.0 - basit jest algilama
// Tum esikler govde boyuna (torso) gore: kameraya yakin/uzak durmak sonucu degistirmez.
#pragma once
#include "skeleton.hpp"
#include <chrono>
#include <deque>
#include <string>
#include <vector>

class GestureDetector {
public:
    // Her karede cagrilir, o karede olusan olaylari dondurur
    std::vector<std::string> update(const Skeleton& sk) {
        std::vector<std::string> events;
        if (!sk.valid) { baselineReady_ = false; history_.clear(); return events; }

        const double now = seconds();
        const float t = sk.torso();
        if (t < 1.f) return events;

        const auto& head = sk[Head];
        const auto& hip = sk[HipCenter];
        const auto& sc = sk[ShoulderCenter];

        // 1) El kaldirma: bilek basin ustunde.
        //    Bas kadrajda degilse (kameraya yakin durunca) omuzlarin biraz ustu referans alinir.
        float handLine = tracked(head) ? head.y : sc.y - t * 0.35f;
        bool leftUp  = tracked(sk[WristLeft])  && sk[WristLeft].y  < handLine;
        bool rightUp = tracked(sk[WristRight]) && sk[WristRight].y < handLine;
        if (leftUp && rightUp) fire(events, "IKI_EL_YUKARI", now);
        else if (leftUp)       fire(events, "SOL_EL_YUKARI", now);
        else if (rightUp)      fire(events, "SAG_EL_YUKARI", now);

        // 2) Egilme: omuz merkezi kalca merkezinin saginda/solunda
        //    (goruntu aynalanmadan once: x buyukse kameraya gore sag)
        float lean = (sc.x - hip.x) / t;
        if (lean > 0.25f)       fire(events, "EGILME_GORUNTU_SAGI", now);
        else if (lean < -0.25f) fire(events, "EGILME_GORUNTU_SOLU", now);

        // 3) Ziplama: kalca, duragan seviyesinin belirgin ustune cikti
        if (!baselineReady_) { baselineY_ = hip.y; baselineReady_ = true; }
        float rise = (baselineY_ - hip.y) / t;   // y asagi dogru artar
        if (rise > 0.18f) fire(events, "ZIPLAMA", now);
        else if (std::abs(rise) < 0.08f) baselineY_ = baselineY_ * 0.95f + hip.y * 0.05f; // yavas guncelle

        // 4) Sag el hizli yana savurma (bowling/tenis icin temel)
        history_.push_back({ now, sk[WristRight].x / t, sk[WristRight].y / t });
        while (!history_.empty() && now - history_.front().time > 0.25) history_.pop_front();
        // Kadrajdan cikarken yanlis savurma olmasin: iki omuz da net gorunmeli
        bool bodyInFrame = sk[ShoulderLeft].state == TrackState::Tracked && sk[ShoulderRight].state == TrackState::Tracked;
        if (!bodyInFrame) history_.clear();
        if (bodyInFrame && history_.size() >= 3 && tracked(sk[WristRight])) {
            const auto& a = history_.front(); const auto& b = history_.back();
            double dt = b.time - a.time;
            if (dt > 0.05) {
                double vx = (b.x - a.x) / dt;   // govde/saniye
                if (vx > 5.0)       fire(events, "SAVURMA_GORUNTU_SAGA", now);
                else if (vx < -5.0) fire(events, "SAVURMA_GORUNTU_SOLA", now);
            }
        }
        return events;
    }

private:
    struct Sample { double time; float x, y; };

    static double seconds() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
    static bool tracked(const JointPos& j) { return j.state != TrackState::NotTracked; }

    // Ayni olay surekli tetiklenmesin diye bekleme suresi
    void fire(std::vector<std::string>& ev, const std::string& name, double now) {
        for (auto& c : cooldowns_) {
            if (c.first == name) {
                if (now - c.second < 0.6) return;
                c.second = now; ev.push_back(name); return;
            }
        }
        cooldowns_.push_back({ name, now });
        ev.push_back(name);
    }

    std::vector<std::pair<std::string, double>> cooldowns_;
    std::deque<Sample> history_;
    float baselineY_ = 0;
    bool baselineReady_ = false;
};
