// Kinext 1.0 - kamera yonetimi (acma, degistirme, isimli butonlar)
#pragma once
#include "camera_list.hpp"
#include "mjpeg.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class CameraManager {
public:
    CameraManager() {
        names_ = listCameras();
        if (names_.empty()) for (int i = 0; i < 4; ++i) names_.push_back("Kamera " + std::to_string(i));
    }

    // Telefon kamerasi (Wi-Fi uzerinden MJPEG). Basarili olursa normal kamera kapatilir.
    bool openPhone(const std::string& address) {
        auto p = std::make_unique<MjpegStream>();
        if (!p->open(address)) { phoneError_ = p->error(); return false; }
        cam_.release();
        phone_ = std::move(p);
        phoneError_.clear();
        std::cout << "Telefon kamerasi acildi: " << phone_->url() << "\n";
        return true;
    }
    void closePhone() { phone_.reset(); }
    bool usingPhone() const { return phone_ != nullptr; }
    const std::string& phoneError() const { return phoneError_; }
    std::string phoneUrl() const { return phone_ ? phone_->url() : std::string(); }

    // Kamerayi acar; acilamazsa onceki kameraya geri doner
    bool open(int index) {
        phone_.reset();
        int prev = index_;
        cam_.release();   // bazi kameralar/USB portlari iki akisi ayni anda kaldiramaz
        if (tryOpen(index)) { index_ = index; std::cout << "Kamera acildi: " << names_[index] << "\n"; return true; }
        std::cerr << "Kamera acilamadi: " << index << "\n";
        if (prev >= 0 && prev != index) tryOpen(prev);
        return false;
    }
    bool openFirst(int preferred = 0) {
        if (preferred < count() && open(preferred)) return true;
        for (int i = 0; i < count(); ++i) if (i != preferred && open(i)) return true;
        return false;
    }
    bool next() {
        for (int s = 1; s <= count(); ++s) if (open(((index_ < 0 ? 0 : index_) + s) % count())) return true;
        return false;
    }
    bool read(cv::Mat& f) {
        if (phone_) {
            if (phone_->read(f)) return true;
            phoneError_ = phone_->error();
            return false;
        }
        return cam_.read(f) && !f.empty();
    }

    int index() const { return phone_ ? -1 : index_; }
    int count() const { return static_cast<int>(names_.size()); }
    const std::string& name(int i) const { return names_[i]; }

    // Sag ustte isimli butonlar; tiklanan kamera numarasini dondurur (yoksa -1)
    int drawButtons(cv::Mat& img, int clickX, int clickY, int top = 54) const {
        const int w = 320, h = 34, gap = 6, margin = 10;
        int clicked = -1;
        for (int i = 0; i < count(); ++i) {
            cv::Rect r(img.cols - w - margin, top + i * (h + gap), w, h);
            r &= cv::Rect(0, 0, img.cols, img.rows);
            if (r.area() <= 0) continue;
            bool active = (i == index_);
            cv::Mat roi = img(r);
            cv::Mat bg(roi.size(), roi.type(), active ? cv::Scalar(60, 160, 60) : cv::Scalar(30, 30, 30));
            cv::addWeighted(bg, 0.8, roi, 0.2, 0, roi);
            cv::rectangle(img, r, active ? cv::Scalar(120, 255, 120) : cv::Scalar(200, 200, 200), 1, cv::LINE_AA);
            cv::putText(img, (active ? "> " : "  ") + asciiLabel(names_[i]), cv::Point(r.x + 10, r.y + 23),
                        cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            if (clickX >= 0 && r.contains(cv::Point(clickX, clickY))) clicked = i;
        }
        return clicked;
    }

private:
    bool tryOpenApi(int index, int api) {
        cv::VideoCapture c;
        if (!c.open(index, api)) return false;
        c.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        c.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        cv::Mat test;
        for (int i = 0; i < 5; ++i) if (c.read(test) && !test.empty()) break;   // ilk kareler bos olabilir
        if (test.empty()) return false;
        cam_ = std::move(c);
        return true;
    }
    bool tryOpen(int index) {
#ifdef _WIN32
        return tryOpenApi(index, cv::CAP_MSMF) || tryOpenApi(index, cv::CAP_DSHOW);
#else
        return tryOpenApi(index, cv::CAP_ANY);
#endif
    }

    cv::VideoCapture cam_;
    std::unique_ptr<MjpegStream> phone_;
    std::string phoneError_;
    std::vector<std::string> names_;
    int index_ = -1;
};
