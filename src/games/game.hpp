// Kinext 1.0 - oyun altyapisi: sahne arayuzu, el ile secilen butonlar, cizim yardimcilari
#pragma once
#include "../skeleton.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <string>

enum class SceneResult { Continue, ToMenu, Restart };

class Game {
public:
    virtual ~Game() = default;
    virtual const char* name() const = 0;
    virtual void reset(cv::Size size) = 0;
    // sk: aynalanmis goruntuye gore iskelet (ekranda ne goruyorsan o)
    virtual SceneResult update(const Skeleton& sk, double dt, cv::Mat& canvas) = 0;
};

inline bool seen(const Skeleton& s, Joint j) { return s.valid && s[j].state != TrackState::NotTracked; }
inline cv::Point2f jp(const Skeleton& s, Joint j) { return { s[j].x, s[j].y }; }
inline cv::Point ip(cv::Point2f p) { return { cvRound(p.x), cvRound(p.y) }; }

// Goruntu aynalandiginda iskeleti de aynala
inline Skeleton mirrorSkeleton(const Skeleton& s, int width) {
    Skeleton m = s;
    for (auto& j : m.joints) j.x = static_cast<float>(width - 1) - j.x;
    return m;
}

inline void centerText(cv::Mat& img, const std::string& t, int y, double scale, const cv::Scalar& color,
                       int thick = 2, int font = cv::FONT_HERSHEY_DUPLEX) {
    int base = 0;
    cv::Size sz = cv::getTextSize(t, font, scale, thick, &base);
    cv::Point org((img.cols - sz.width) / 2, y);
    cv::putText(img, t, org + cv::Point(3, 3), font, scale, cv::Scalar(0, 0, 0), thick + 2, cv::LINE_AA);
    cv::putText(img, t, org, font, scale, color, thick, cv::LINE_AA);
}

inline void fillAlpha(cv::Mat& img, cv::Rect r, const cv::Scalar& color, double alpha) {
    r &= cv::Rect(0, 0, img.cols, img.rows);
    if (r.area() <= 0) return;
    cv::Mat roi = img(r);
    cv::Mat bg(roi.size(), roi.type(), color);
    cv::addWeighted(bg, alpha, roi, 1.0 - alpha, 0, roi);
}

// Oyuncunun silueti: soluk iskelet + parlak eller (Kinect'teki "golge" hissi)
inline void drawBody(cv::Mat& img, const Skeleton& sk) {
    if (!sk.valid) return;
    for (auto [a, b] : BONES) {
        if (!seen(sk, a) || !seen(sk, b)) continue;
        cv::line(img, ip(jp(sk, a)), ip(jp(sk, b)), cv::Scalar(200, 170, 120), 10, cv::LINE_AA);
    }
    if (seen(sk, Head)) cv::circle(img, ip(jp(sk, Head)), 28, cv::Scalar(200, 170, 120), cv::FILLED, cv::LINE_AA);
}

inline void drawHands(cv::Mat& img, const Skeleton& sk) {
    for (Joint j : { HandLeft, HandRight }) {
        if (!seen(sk, j)) continue;
        cv::circle(img, ip(jp(sk, j)), 22, cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
        cv::circle(img, ip(jp(sk, j)), 8, cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
    }
}

// Kinect tarzi buton: elini ustunde tut, halka dolunca secilir
struct HoverButton {
    cv::Rect rect;
    std::string label;
    cv::Scalar color{ 200, 120, 40 };
    double hold = 0;
    static constexpr double HOLD_TIME = 1.3;

    bool update(const Skeleton& sk, double dt, cv::Mat& img) {
        bool over = false;
        cv::Point2f hand;
        for (Joint j : { HandLeft, HandRight })
            if (seen(sk, j) && rect.contains(ip(jp(sk, j)))) { over = true; hand = jp(sk, j); }
        hold = over ? hold + dt : 0;

        fillAlpha(img, rect, over ? color * 1.3 : color, over ? 0.95 : 0.8);
        cv::rectangle(img, rect, cv::Scalar(255, 255, 255), over ? 3 : 1, cv::LINE_AA);
        // Yazi butona sigsin: yukseklige gore baslat, genislige sigmiyorsa kucult
        int base = 0;
        double scale = rect.height / 70.0;
        cv::Size sz = cv::getTextSize(label, cv::FONT_HERSHEY_DUPLEX, scale, 2, &base);
        double maxW = rect.width * 0.88;
        if (sz.width > maxW) {
            scale *= maxW / sz.width;
            sz = cv::getTextSize(label, cv::FONT_HERSHEY_DUPLEX, scale, 2, &base);
        }
        cv::putText(img, label, cv::Point(rect.x + (rect.width - sz.width) / 2, rect.y + (rect.height + sz.height) / 2),
                    cv::FONT_HERSHEY_DUPLEX, scale, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
        if (over) {
            double p = (std::min)(hold / HOLD_TIME, 1.0);
            cv::ellipse(img, ip(hand), cv::Size(34, 34), -90, 0, 360 * p, cv::Scalar(255, 255, 255), 6, cv::LINE_AA);
        }
        if (hold >= HOLD_TIME) { hold = 0; return true; }
        return false;
    }
};
