// Kinext 1.0 - Engelden Kac: 3 seritte dusen bloklardan saga-sola kayarak kac (3 can)
#pragma once
#include "game.hpp"
#include <vector>

class DodgeGame : public Game {
public:
    const char* name() const override { return "ENGELDEN KAC"; }

    void reset(cv::Size size) override {
        size_ = size;
        blocks_.clear();
        time_ = 0; spawn_ = 1.0; lives_ = 3; dodged_ = 0; over_ = false; flash_ = 0;
        int bw = size.width / 4, bh = size.height / 9;
        again_ = { cv::Rect(size.width / 2 - bw - 20, size.height * 6 / 10, bw, bh), "TEKRAR", cv::Scalar(60, 150, 60) };
        menu_  = { cv::Rect(size.width / 2 + 20,      size.height * 6 / 10, bw, bh), "MENU",   cv::Scalar(160, 90, 40) };
    }

    SceneResult update(const Skeleton& sk, double dt, cv::Mat& img) override {
        const float h = static_cast<float>(size_.height), w = static_cast<float>(size_.width);
        const float laneW = w / LANES;
        const float playerTop = h * 0.74f, playerBottom = h * 0.92f;

        // Oyuncunun seridi: kalca merkezinin yatay konumu
        if (sk.valid && seen(sk, HipCenter))
            lane_ = std::clamp(static_cast<int>(sk[HipCenter].x / laneW), 0, LANES - 1);

        if (!over_ && sk.valid) {
            time_ += dt;
            float speed = h * (0.45f + 0.025f * static_cast<float>(time_));   // zamanla hizlanir
            spawn_ -= dt;
            if (spawn_ <= 0) {
                spawnRow();
                spawn_ = (std::max)(0.55, 1.4 - time_ * 0.02);
            }
            for (auto& b : blocks_) {
                b.y += speed * static_cast<float>(dt);
                bool overlap = b.y + b.h > playerTop && b.y < playerBottom;
                if (overlap && !b.done && b.lane == lane_) { b.done = true; b.hit = true; --lives_; flash_ = 0.4; }
                if (b.y > playerBottom && !b.done) { b.done = true; ++dodged_; }
            }
            blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(), [&](const Block& b) { return b.y > h; }), blocks_.end());
            if (lives_ <= 0) over_ = true;
        }
        flash_ = (std::max)(0.0, flash_ - dt);

        // Seritler
        for (int i = 1; i < LANES; ++i)
            cv::line(img, cv::Point(cvRound(laneW * i), 60), cv::Point(cvRound(laneW * i), img.rows), cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
        fillAlpha(img, cv::Rect(cvRound(laneW * lane_), 56, cvRound(laneW), img.rows), cv::Scalar(80, 200, 80), 0.12);

        // Bloklar
        for (const auto& b : blocks_) {
            cv::Rect r(cvRound(laneW * b.lane + laneW * 0.1f), cvRound(b.y), cvRound(laneW * 0.8f), cvRound(b.h));
            fillAlpha(img, r, b.hit ? cv::Scalar(120, 120, 120) : cv::Scalar(50, 50, 220), 0.9);
            cv::rectangle(img, r, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
        }
        // Oyuncu isareti
        cv::Rect pr(cvRound(laneW * lane_ + laneW * 0.25f), cvRound(playerTop), cvRound(laneW * 0.5f), cvRound(playerBottom - playerTop));
        fillAlpha(img, pr, cv::Scalar(80, 220, 80), 0.5);
        cv::rectangle(img, pr, cv::Scalar(150, 255, 150), 3, cv::LINE_AA);

        if (flash_ > 0) fillAlpha(img, cv::Rect(0, 0, img.cols, img.rows), cv::Scalar(0, 0, 255), flash_ * 0.8);

        // Ust bilgi
        fillAlpha(img, cv::Rect(0, 0, img.cols, 56), cv::Scalar(20, 20, 20), 0.7);
        cv::putText(img, "PUAN " + std::to_string(score()), { 20, 40 }, cv::FONT_HERSHEY_DUPLEX, 1.2, cv::Scalar(80, 220, 255), 2, cv::LINE_AA);
        std::string hearts;
        for (int i = 0; i < 3; ++i) hearts += i < lives_ ? "O " : "- ";
        cv::putText(img, "CAN " + hearts, { img.cols - 260, 40 }, cv::FONT_HERSHEY_DUPLEX, 1.1, cv::Scalar(80, 80, 255), 2, cv::LINE_AA);

        if (!sk.valid && !over_) centerText(img, "KAMERAYA GEC", static_cast<int>(h / 2), 1.6, cv::Scalar(80, 160, 255));
        else if (time_ < 3 && !over_) centerText(img, "SAGA SOLA KAYARAK KAC!", static_cast<int>(h * 0.4), 1.4, cv::Scalar(255, 255, 255));

        if (over_) {
            fillAlpha(img, cv::Rect(0, 0, img.cols, img.rows), cv::Scalar(0, 0, 0), 0.55);
            centerText(img, "OYUN BITTI!", static_cast<int>(h * 0.32), 2.0, cv::Scalar(255, 255, 255), 3);
            centerText(img, "PUAN: " + std::to_string(score()), static_cast<int>(h * 0.47), 1.8, cv::Scalar(80, 220, 255), 3);
            if (again_.update(sk, dt, img)) return SceneResult::Restart;
            if (menu_.update(sk, dt, img)) return SceneResult::ToMenu;
        }
        drawHands(img, sk);
        return SceneResult::Continue;
    }

private:
    struct Block { int lane; float y, h; bool done = false, hit = false; };
    static constexpr int LANES = 3;

    int score() const { return dodged_ * 10 + static_cast<int>(time_); }

    void spawnRow() {
        const float h = static_cast<float>(size_.height);
        int a = static_cast<int>(rng_() % LANES);
        blocks_.push_back({ a, -h * 0.12f, h * 0.12f });
        // 20 saniyeden sonra bazen iki serit birden (biri hep acik kalir)
        if (time_ > 20 && rng_() % 3 == 0) {
            int b = (a + 1 + static_cast<int>(rng_() % (LANES - 1))) % LANES;
            blocks_.push_back({ b, -h * 0.12f, h * 0.12f });
        }
    }

    cv::Size size_;
    std::vector<Block> blocks_;
    std::mt19937 rng_{ std::random_device{}() };
    double time_ = 0, spawn_ = 0, flash_ = 0;
    int lives_ = 3, dodged_ = 0, lane_ = 1;
    bool over_ = false;
    HoverButton again_, menu_;
};
