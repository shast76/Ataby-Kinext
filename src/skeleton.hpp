// Kinext 1.0 - Kinect v1 formatinda iskelet (20 eklem)
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <sstream>

// MoveNet / COCO 17 nokta sirasi
enum Coco {
    NOSE, L_EYE, R_EYE, L_EAR, R_EAR,
    L_SHOULDER, R_SHOULDER, L_ELBOW, R_ELBOW, L_WRIST, R_WRIST,
    L_HIP, R_HIP, L_KNEE, R_KNEE, L_ANKLE, R_ANKLE, COCO_COUNT
};

struct Keypoint { float x = 0, y = 0, score = 0; };   // piksel koordinati
using Keypoints = std::array<Keypoint, COCO_COUNT>;

// Kinect for Windows SDK 1.8 NUI_SKELETON_POSITION_INDEX ile ayni sira
enum Joint {
    HipCenter, Spine, ShoulderCenter, Head,
    ShoulderLeft, ElbowLeft, WristLeft, HandLeft,
    ShoulderRight, ElbowRight, WristRight, HandRight,
    HipLeft, KneeLeft, AnkleLeft, FootLeft,
    HipRight, KneeRight, AnkleRight, FootRight,
    JOINT_COUNT
};

inline const char* JOINT_NAMES[JOINT_COUNT] = {
    "HipCenter","Spine","ShoulderCenter","Head",
    "ShoulderLeft","ElbowLeft","WristLeft","HandLeft",
    "ShoulderRight","ElbowRight","WristRight","HandRight",
    "HipLeft","KneeLeft","AnkleLeft","FootLeft",
    "HipRight","KneeRight","AnkleRight","FootRight"
};

// Kinect'teki NUI_SKELETON_POSITION_TRACKING_STATE karsiligi
enum class TrackState { NotTracked, Inferred, Tracked };

struct JointPos { float x = 0, y = 0; TrackState state = TrackState::NotTracked; };

struct Skeleton {
    std::array<JointPos, JOINT_COUNT> joints;
    bool valid = false;

    const JointPos& operator[](Joint j) const { return joints[j]; }

    // Govde boyu (omuz merkezi - kalca merkezi): mesafeden bagimsiz olcek
    float torso() const {
        const auto& a = joints[ShoulderCenter]; const auto& b = joints[HipCenter];
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    // Oyunlara / WebSocket'e gonderilecek JSON (koordinatlar 0-1 arasi)
    std::string toJson(float w, float h) const {
        std::ostringstream s;
        s.precision(4);
        s << "{\"valid\":" << (valid ? "true" : "false") << ",\"joints\":{";
        for (int i = 0; i < JOINT_COUNT; ++i) {
            const auto& j = joints[i];
            s << (i ? "," : "") << '"' << JOINT_NAMES[i] << "\":[" << j.x / w << ',' << j.y / h << ','
              << static_cast<int>(j.state) << ']';
        }
        s << "}}";
        return s.str();
    }
};

// 17 COCO noktasindan 20 Kinect eklemi uretir
inline Skeleton toKinect(const Keypoints& k, float minScore = 0.3f) {
    Skeleton sk;
    // Bacak noktalari kamerada daha dusuk guvenle gelir, onlar icin esik biraz daha dusuk
    auto ok = [&](Coco c) { return k[c].score >= (c >= L_KNEE ? minScore * 0.5f : minScore); };
    auto set = [&](Joint j, float x, float y, TrackState st) { sk.joints[j] = { x, y, st }; };
    auto copy = [&](Joint j, Coco c) {
        set(j, k[c].x, k[c].y, ok(c) ? TrackState::Tracked : TrackState::NotTracked);
    };
    auto mid = [&](Joint j, Coco a, Coco b) {
        set(j, (k[a].x + k[b].x) / 2, (k[a].y + k[b].y) / 2,
            ok(a) && ok(b) ? TrackState::Inferred : TrackState::NotTracked);
    };
    // Bir segmenti uzatarak tahmin (bilekten ele, bilekten ayaga)
    auto extend = [&](Joint j, Coco from, Coco to, float f) {
        set(j, k[to].x + (k[to].x - k[from].x) * f, k[to].y + (k[to].y - k[from].y) * f,
            ok(from) && ok(to) ? TrackState::Inferred : TrackState::NotTracked);
    };

    copy(Head, NOSE);
    mid(ShoulderCenter, L_SHOULDER, R_SHOULDER);
    mid(HipCenter, L_HIP, R_HIP);
    {
        const auto& s = sk.joints[ShoulderCenter]; const auto& h = sk.joints[HipCenter];
        set(Spine, h.x + (s.x - h.x) * 0.35f, h.y + (s.y - h.y) * 0.35f,
            s.state != TrackState::NotTracked && h.state != TrackState::NotTracked
                ? TrackState::Inferred : TrackState::NotTracked);
    }
    copy(ShoulderLeft, L_SHOULDER);  copy(ElbowLeft, L_ELBOW);  copy(WristLeft, L_WRIST);
    copy(ShoulderRight, R_SHOULDER); copy(ElbowRight, R_ELBOW); copy(WristRight, R_WRIST);
    extend(HandLeft, L_ELBOW, L_WRIST, 0.25f);
    extend(HandRight, R_ELBOW, R_WRIST, 0.25f);
    copy(HipLeft, L_HIP);  copy(KneeLeft, L_KNEE);  copy(AnkleLeft, L_ANKLE);
    copy(HipRight, R_HIP); copy(KneeRight, R_KNEE); copy(AnkleRight, R_ANKLE);
    // Ayak: bilegin hemen alti (kaval kemigini uzatmak bacak bukulunce ayagi yanlis yere atiyordu)
    auto foot = [&](Joint j, Coco knee, Coco ankle) {
        float shin = std::hypot(k[ankle].x - k[knee].x, k[ankle].y - k[knee].y);
        set(j, k[ankle].x, k[ankle].y + shin * 0.12f,
            ok(ankle) ? TrackState::Inferred : TrackState::NotTracked);
    };
    foot(FootLeft, L_KNEE, L_ANKLE);
    foot(FootRight, R_KNEE, R_ANKLE);

    sk.valid = ok(L_SHOULDER) && ok(R_SHOULDER) && ok(L_HIP) && ok(R_HIP);
    return sk;
}

// Kinect'in cizdigi kemikler
inline const std::array<std::pair<Joint, Joint>, 19> BONES = {{
    {HipCenter, Spine}, {Spine, ShoulderCenter}, {ShoulderCenter, Head},
    {ShoulderCenter, ShoulderLeft}, {ShoulderLeft, ElbowLeft}, {ElbowLeft, WristLeft}, {WristLeft, HandLeft},
    {ShoulderCenter, ShoulderRight}, {ShoulderRight, ElbowRight}, {ElbowRight, WristRight}, {WristRight, HandRight},
    {HipCenter, HipLeft}, {HipLeft, KneeLeft}, {KneeLeft, AnkleLeft}, {AnkleLeft, FootLeft},
    {HipCenter, HipRight}, {HipRight, KneeRight}, {KneeRight, AnkleRight}, {AnkleRight, FootRight},
}};

// Titreme onleyici: yeni kareyi oncekiyle harmanlar. Hizli hareketlerde daha az yumusatir
// ki atis/savurma gecikmesin (basit "One Euro" mantigi).
inline Skeleton smoothSkeleton(const Skeleton& prev, const Skeleton& cur) {
    if (!prev.valid || !cur.valid) return cur;
    Skeleton out = cur;
    float scale = std::max(cur.torso(), 1.f);
    for (int i = 0; i < JOINT_COUNT; ++i) {
        const auto& p = prev.joints[i]; const auto& c = cur.joints[i];
        if (p.state == TrackState::NotTracked || c.state == TrackState::NotTracked) continue;
        float move = std::hypot(c.x - p.x, c.y - p.y) / scale;   // govde boyuna gore hareket
        float a = std::clamp(0.35f + move * 4.f, 0.35f, 1.f);    // yavas: 0.35, hizli: 1.0
        out.joints[i].x = p.x + (c.x - p.x) * a;
        out.joints[i].y = p.y + (c.y - p.y) * a;
    }
    return out;
}

// --- One Euro filtresi (Casiez ve ark., 2012) ---
// Yavas harekette guclu yumusatir (titreme yok), hizli harekette az yumusatir (gecikme yok).
struct OneEuro {
    float x = 0, dx = 0;
    bool init = false;
    static float alpha(float cutoff, float dt) {
        float tau = 1.f / (2.f * 3.14159265f * cutoff);
        return 1.f / (1.f + tau / dt);
    }
    float filter(float v, float dt, float minCutoff, float beta) {
        if (!init) { init = true; x = v; dx = 0; return v; }
        float d = (v - x) / dt;
        dx += alpha(1.f, dt) * (d - dx);
        float a = alpha(minCutoff + beta * std::fabs(dx), dt);
        x += a * (v - x);
        return x;
    }
};

// Iskeleti kareden kareye kararli tutar:
//  - her eklem One Euro ile filtrelenir (bacaklar daha guclu, cunku daha az hareket eder ve daha cok titrer)
//  - bir eklem kisa sureligine kaybolursa (0,5 sn) son konumu tutulur: ayak "zipzip" yapmaz
//  - dusuk guvenli bir eklemin tek karede asiri sicramasi yok sayilir
class SkeletonFilter {
public:
    void reset() { *this = SkeletonFilter(); }

    Skeleton apply(const Skeleton& in, float dt) {
        dt = std::clamp(dt, 1.f / 120.f, 0.2f);
        if (!in.valid) {
            lostAll_ += dt;
            if (lostAll_ < 0.15f && last_.valid) return last_;  // tek tuk kare kaybinda goruntuyu koru
            reset();
            return in;
        }
        lostAll_ = 0;
        float t = (std::max)(in.torso(), 1.f);
        torso_ = torso_ <= 0 ? t : torso_ + (t - torso_) * 0.05f;   // olcek de yavasca degissin
        const float k = torso_;

        Skeleton out = in;
        for (int i = 0; i < JOINT_COUNT; ++i) {
            const JointPos& j = in.joints[i];
            bool legs = i >= HipLeft;   // HipLeft..FootRight
            if (j.state == TrackState::NotTracked) {
                lost_[i] += dt;
                if (last_.valid && last_.joints[i].state != TrackState::NotTracked && lost_[i] < 0.25f)
                    out.joints[i] = { last_.joints[i].x, last_.joints[i].y, TrackState::Inferred };
                continue;
            }
            // Bacaklarda dusuk guvenli (tahmini) eklem tek karede govde boyunun %60'indan fazla ziplarsa: kotu olcum.
            // Kollarda yapilmaz: hizli savurmalar (bowling, balon) gercek harekettir.
            if (legs && j.state == TrackState::Inferred && last_.valid && last_.joints[i].state != TrackState::NotTracked && lost_[i] == 0) {
                float jump = std::hypot(j.x - last_.joints[i].x, j.y - last_.joints[i].y) / k;
                if (jump > 0.6f) { out.joints[i] = last_.joints[i]; continue; }
            }
            lost_[i] = 0;
            // Govde birimine gore filtrele: kameraya yakin/uzak durmak ayari degistirmesin
            float minCut = legs ? 1.0f : 1.4f, beta = legs ? 0.8f : 0.6f;
            out.joints[i].x = fx_[i].filter(j.x / k, dt, minCut, beta) * k;
            out.joints[i].y = fy_[i].filter(j.y / k, dt, minCut, beta) * k;
        }
        last_ = out;
        return out;
    }

private:
    std::array<OneEuro, JOINT_COUNT> fx_{}, fy_{};
    std::array<float, JOINT_COUNT> lost_{};
    Skeleton last_;
    float torso_ = 0, lostAll_ = 0;
};
