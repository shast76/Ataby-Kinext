#include "tracker.hpp"
#include "camera.hpp"
#include "pose.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

struct Tracker::Impl {
    std::unique_ptr<PoseEstimator> pose;
    CameraManager cams;
    std::thread worker;
    std::atomic<bool> run{ false };
    std::atomic<int> requestedCamera{ -1 };
    std::string requestedPhone;          // mutex ile korunur
    bool phoneRequested = false;
    mutable std::mutex mutex;
    TrackerState shared;
    std::string error;

    void loop() {
        SkeletonFilter filter;
        cv::Mat frame;
        auto last = std::chrono::steady_clock::now();
        float fps = 0;
        while (run) {
            int req = requestedCamera.exchange(-1);
            if (req >= 0) { cams.open(req); filter.reset(); }

            std::string phoneAddr;
            bool phoneReq = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (phoneRequested) { phoneReq = true; phoneAddr = requestedPhone; phoneRequested = false; shared.phoneConnecting = true; }
            }
            if (phoneReq) {
                bool ok = cams.openPhone(phoneAddr);   // birkac saniye surebilir
                filter.reset();
                std::lock_guard<std::mutex> lock(mutex);
                shared.phoneConnecting = false;
                shared.phoneError = ok ? "" : cams.phoneError();
            }

            if (!cams.read(frame)) {
                if (cams.usingPhone()) {
                    // Telefon baglantisi koptu: bir kez yeniden dene, olmazsa bilgisayar kamerasina don
                    std::string url = cams.phoneUrl(), err = cams.phoneError();
                    if (!cams.openPhone(url)) {
                        cams.closePhone();
                        cams.openFirst(0);
                        std::lock_guard<std::mutex> lock(mutex);
                        shared.phoneError = "Telefon bağlantısı koptu: " + err;
                    }
                    continue;
                }
                if (!cams.next()) std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            if (dt > 0) fps = fps * 0.9f + (1.f / dt) * 0.1f;

            Keypoints kp{};
            Skeleton raw;
            try {
                if (pose->estimate(frame, kp)) raw = toKinect(kp);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(mutex);
                error = e.what();
            }
            Skeleton sk = filter.apply(raw, dt);

            // 0-1 araligina cevir ve aynala
            Skeleton n = sk;
            const float w = static_cast<float>(frame.cols), h = static_cast<float>(frame.rows);
            for (auto& j : n.joints) { j.x = 1.f - j.x / w; j.y = j.y / h; }

            std::lock_guard<std::mutex> lock(mutex);
            shared.skeleton = n;
            shared.running = true;
            shared.fps = fps;
            shared.camIndex = cams.index();
            shared.phone = cams.usingPhone();
            shared.phoneUrl = cams.phoneUrl();
        }
    }
};

Tracker::Tracker() : impl_(std::make_unique<Impl>()) {}
Tracker::~Tracker() { stop(); }

bool Tracker::start(const std::string& modelPath, int preferredCamera) {
#ifdef _WIN32
    // Media Foundation'in donanim donusumleri bazi kameralarda acilisi saniyelerce dondurur
    _putenv_s("OPENCV_VIDEOIO_MSMF_ENABLE_HW_TRANSFORMS", "0");
#endif
    try {
        impl_->pose = std::make_unique<PoseEstimator>(modelPath, true);
    } catch (const std::exception& e) {
        impl_->error = std::string("Model yuklenemedi: ") + e.what();
        return false;
    }
    if (!impl_->cams.openFirst(preferredCamera)) {
        impl_->error = "Kamera bulunamadi";
        return false;
    }
    impl_->shared.backend = impl_->pose->backend();
    impl_->run = true;
    impl_->worker = std::thread([this] { impl_->loop(); });
    return true;
}

void Tracker::stop() {
    if (!impl_) return;
    impl_->run = false;
    if (impl_->worker.joinable()) impl_->worker.join();
}

TrackerState Tracker::state() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->shared;
}

std::vector<std::string> Tracker::cameraNames() const {
    std::vector<std::string> names;
    // CameraManager adlari olusturuldugunda bir kez alir, sonra degismez
    for (int i = 0; i < impl_->cams.count(); ++i) names.push_back(impl_->cams.name(i));
    return names;
}

void Tracker::selectCamera(int index) { impl_->requestedCamera = index; }

void Tracker::selectPhone(const std::string& address) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->requestedPhone = address;
    impl_->phoneRequested = true;
    impl_->shared.phoneError.clear();
    impl_->shared.phoneConnecting = true;
}

std::string Tracker::error() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->error;
}
