// Kinext 1.0 - Balon Patlat: asagidan yukselen balonlari ellerinle patlat (60 saniye)
#pragma once
#include "game.hpp"
#include <vector>

class BalloonGame : public Game {
public:
    const char* name() const override { return "BALON PATLAT"; }

    void reset(cv::Size size) override {
        size_ = size;
        balloons_.clear(); parts_.clear();
        time_ = 0; spawn_ = 0.5; score_ = 0; missed_ = 0; over_ = false;
        int bw = size.width / 4, bh = size.height / 9;
        again_ = { cv::Rect(size.width / 2 - bw - 20, size.height * 6 / 10, bw, bh), "TEKRAR", cv::Scalar(60, 150, 60) };
        menu_  = { cv::Rect(size.width / 2 + 20,      size.height * 6 / 10, bw, bh), "MENU",   cv::Scalar(160, 90, 40) };
    }

    SceneResult update(const Skeleton& sk, double dt, cv::Mat& img) override {
        const float h = static_cast<float>(size_.height), w = static_cast<float>(size_.width);

        if (!over_ && sk.valid) {
            time_ += dt;
            spawn_ -= dt;
            double progress = time_ / DURATION;   // zamanla hizlanir
            if (spawn_ <= 0) {
                spawnBalloon(progress);
                spawn_ = (std::max)(0.35, 1.0 - progress * 0.6);
            }
            for (auto& b : balloons_) {
                b.pos.y -= b.speed * static_cast<float>(dt);
                b.pos.x += std::sin(static_cast<float>(time_) * 2.f + b.wobble) * 25.f * static_cast<float>(dt);
            }
            // Patlatma: herhangi bir el balona degdi mi?
            for (auto it = balloons_.begin(); it != balloons_.end();) {
                bool popped = false;
                for (Joint j : { HandLeft, HandRight, WristLeft, WristRight })
                    if (seen(sk, j) && cv::norm(jp(sk, j) - it->pos) < it->r + 18.f) popped = true;
                if (popped) { ++score_; burst(it->pos, it->color); it = balloons_.erase(it); }
                else if (it->pos.y + it->r * 2.f < 0) { ++missed_; it = balloons_.erase(it); }
                else ++it;
            }
            if (time_ >= DURATION) over_ = true;
        }

        // Parcaciklar
        for (auto& p : parts_) { p.pos += p.vel * static_cast<float>(dt); p.vel.y += 600.f * static_cast<float>(dt); p.life -= static_cast<float>(dt); }
        parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.life <= 0; }), parts_.end());

        // Cizim
        for (const auto& b : balloons_) {
            cv::Point c = ip(b.pos);
            int r = cvRound(b.r);
            cv::line(img, c + cv::Point(0, r), c + cv::Point(cvRound(std::sin(b.wobble) * 8), r * 2), cv::Scalar(220, 220, 220), 2, cv::LINE_AA);
            cv::ellipse(img, c, cv::Size(r, cvRound(r * 1.15)), 0, 0, 360, b.color, cv::FILLED, cv::LINE_AA);
            cv::circle(img, c + cv::Point(-r / 3, -r / 2), r / 5, cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
        }
        for (const auto& p : parts_)
            cv::circle(img, ip(p.pos), (std::max)(1, cvRound(6 * p.life)), p.color, cv::FILLED, cv::LINE_AA);
        drawHands(img, sk);

        // Ust bilgi
        fillAlpha(img, cv::Rect(0, 0, img.cols, 56), cv::Scalar(20, 20, 20), 0.7);
        cv::putText(img, "PUAN " + std::to_string(score_), { 20, 40 }, cv::FONT_HERSHEY_DUPLEX, 1.2, cv::Scalar(80, 220, 255), 2, cv::LINE_AA);
        int left = (std::max)(0, static_cast<int>(std::ceil(DURATION - time_)));
        centerText(img, std::to_string(left), 42, 1.3, left <= 10 ? cv::Scalar(80, 80, 255) : cv::Scalar(255, 255, 255));
        cv::putText(img, "KACAN " + std::to_string(missed_), { img.cols - 200, 40 }, cv::FONT_HERSHEY_DUPLEX, 1.0, cv::Scalar(200, 200, 200), 2, cv::LINE_AA);

        if (!sk.valid && !over_) centerText(img, "KAMERAYA GEC", static_cast<int>(h / 2), 1.6, cv::Scalar(80, 160, 255));

        if (over_) {
            fillAlpha(img, cv::Rect(0, 0, img.cols, img.rows), cv::Scalar(0, 0, 0), 0.55);
            centerText(img, "SURE DOLDU!", static_cast<int>(h * 0.32), 2.0, cv::Scalar(255, 255, 255), 3);
            centerText(img, "PUAN: " + std::to_string(score_), static_cast<int>(h * 0.47), 1.8, cv::Scalar(80, 220, 255), 3);
            if (again_.update(sk, dt, img)) return SceneResult::Restart;
            if (menu_.update(sk, dt, img)) return SceneResult::ToMenu;
            drawHands(img, sk);
        }
        (void)w;
        return SceneResult::Continue;
    }

private:
    struct Balloon { cv::Point2f pos; float r, speed, wobble; cv::Scalar color; };
    struct Particle { cv::Point2f pos, vel; float life; cv::Scalar color; };
    static constexpr double DURATION = 60.0;

    void spawnBalloon(double progress) {
        std::uniform_real_distribution<float> u(0.f, 1.f);
        const float h = static_cast<float>(size_.height), w = static_cast<float>(size_.width);
        Balloon b;
        b.r = h * (0.055f + 0.03f * u(rng_));
        b.pos = { b.r + u(rng_) * (w - 2 * b.r), h + b.r };
        b.speed = h * (0.16f + 0.12f * u(rng_)) * (1.f + static_cast<float>(progress));
        b.wobble = u(rng_) * 6.28f;
        static const cv::Scalar colors[] = { {60, 60, 230}, {230, 140, 40}, {60, 200, 80}, {40, 200, 240}, {200, 80, 200}, {255, 160, 80} };
        b.color = colors[rng_() % 6];
        balloons_.push_back(b);
    }
    void burst(cv::Point2f at, const cv::Scalar& color) {
        std::uniform_real_distribution<float> a(0.f, 6.283f), s(150.f, 450.f);
        for (int i = 0; i < 16; ++i) {
            float ang = a(rng_), sp = s(rng_);
            parts_.push_back({ at, { std::cos(ang) * sp, std::sin(ang) * sp }, 0.6f, color });
        }
    }

    cv::Size size_;
    std::vector<Balloon> balloons_;
    std::vector<Particle> parts_;
    std::mt19937 rng_{ std::random_device{}() };
    double time_ = 0, spawn_ = 0;
    int score_ = 0, missed_ = 0;
    bool over_ = false;
    HoverButton again_, menu_;
};
