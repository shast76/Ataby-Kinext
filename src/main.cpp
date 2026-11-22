// Kinext 1.0 Core - webcam ile Kinect tarzi iskelet takibi
// Kullanim: kinext [model.onnx] [kamera_no] [--cpu] [--json]
//   ESC / Q : cikis   M : aynalama   J : JSON ciktisi   C : sonraki kamera
//   Sag ustteki butonlara tiklayarak da kamera secilir
#include "camera_list.hpp"
#include "gestures.hpp"
#include "pose.hpp"
#include "skeleton.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <cctype>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

static void drawSkeleton(cv::Mat& img, const Skeleton& sk) {
    auto pt = [](const JointPos& j) { return cv::Point(cvRound(j.x), cvRound(j.y)); };
    for (auto [a, b] : BONES) {
        const auto& ja = sk[a]; const auto& jb = sk[b];
        if (ja.state == TrackState::NotTracked || jb.state == TrackState::NotTracked) continue;
        bool inferred = ja.state == TrackState::Inferred || jb.state == TrackState::Inferred;
        // Kinect renkleri: takip edilen yesil, tahmin edilen gri
        cv::line(img, pt(ja), pt(jb), inferred ? cv::Scalar(160, 160, 160) : cv::Scalar(60, 220, 60), 4, cv::LINE_AA);
    }
    for (const auto& j : sk.joints) {
        if (j.state == TrackState::NotTracked) continue;
        cv::circle(img, pt(j), 6,
                   j.state == TrackState::Tracked ? cv::Scalar(30, 140, 255) : cv::Scalar(200, 200, 200),
                   cv::FILLED, cv::LINE_AA);
    }
}

struct ClickState { int x = -1, y = -1; };

static void onMouse(int event, int x, int y, int, void* data) {
    if (event == cv::EVENT_LBUTTONDOWN) { auto* c = static_cast<ClickState*>(data); c->x = x; c->y = y; }
}

// Sag ustte kamera butonlari; tiklanan butonun kamera numarasini dondurur (yoksa -1)
static int drawCameraButtons(cv::Mat& img, const std::vector<std::string>& names, int active, ClickState& click) {
    const int w = 320, h = 34, gap = 6, margin = 10, top = 54;
    int clicked = -1;
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        cv::Rect r(img.cols - w - margin, top + i * (h + gap), w, h);
        bool isActive = (i == active);
        cv::Mat roi = img(r & cv::Rect(0, 0, img.cols, img.rows));
        cv::Mat bg(roi.size(), roi.type(), isActive ? cv::Scalar(60, 160, 60) : cv::Scalar(30, 30, 30));
        cv::addWeighted(bg, 0.8, roi, 0.2, 0, roi);   // yari saydam buton
        cv::rectangle(img, r, isActive ? cv::Scalar(120, 255, 120) : cv::Scalar(200, 200, 200), 1, cv::LINE_AA);
        cv::putText(img, (isActive ? "> " : "  ") + asciiLabel(names[i]), { r.x + 10, r.y + 23 },
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        if (click.x >= 0 && r.contains(cv::Point(click.x, click.y))) clicked = i;
    }
    click = {};
    return clicked;
}

static std::string mirrorName(std::string e, bool mirrored) {
    // Aynalanmis goruntude "goruntu sagi" oyuncunun kendi sagidir
    auto rep = [&](const std::string& from, const std::string& to) {
        if (auto p = e.find(from); p != std::string::npos) e.replace(p, from.size(), to);
    };
    if (mirrored) { rep("GORUNTU_SAGI", "SOLA"); rep("GORUNTU_SOLU", "SAGA");
                    rep("GORUNTU_SAGA", "SOLA"); rep("GORUNTU_SOLA", "SAGA"); }
    else          { rep("GORUNTU_SAGI", "SAGA"); rep("GORUNTU_SOLU", "SOLA");
                    rep("GORUNTU_SAGA", "SAGA"); rep("GORUNTU_SOLA", "SOLA"); }
    return e;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Media Foundation'in donanim donusumleri bazi kameralarda acilisi saniyelerce dondurur
    _putenv_s("OPENCV_VIDEOIO_MSMF_ENABLE_HW_TRANSFORMS", "0");
#endif
    // Daha isabetli Thunder modeli klasordeyse onu, yoksa hizli Lightning'i kullan
    std::string modelPath = std::filesystem::exists("models/movenet_thunder.onnx")
        ? "models/movenet_thunder.onnx" : "models/movenet_lightning.onnx";
    int camIndex = 0;
    bool useGpu = true, printJson = false, mirror = true;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cpu") useGpu = false;
        else if (a == "--json") printJson = true;
        else if (!a.empty() && std::isdigit(static_cast<unsigned char>(a[0]))) camIndex = std::stoi(a);
        else modelPath = a;
    }

    try {
        PoseEstimator pose(modelPath, useGpu);
        GestureDetector gestures;

        std::vector<std::string> camNames = listCameras();
        if (camNames.empty()) for (int i = 0; i < 6; ++i) camNames.push_back("Kamera " + std::to_string(i));
        for (size_t i = 0; i < camNames.size(); ++i) std::cout << "[" << i << "] " << camNames[i] << "\n";
        const int camCount = static_cast<int>(camNames.size());

        cv::VideoCapture cam;
        // Tek bir arka uçla kamerayi acip ilk kareyi okumayi dener
        auto tryOpen = [&](int index, int api) -> bool {
            cv::VideoCapture c;
            if (!c.open(index, api))
                return false;
            c.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
            c.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
            cv::Mat test;
            for (int i = 0; i < 5; ++i)                       // bazi kameralar ilk kareleri bos verir
                if (c.read(test) && !test.empty()) break;
            if (test.empty()) return false;
            cam = std::move(c);
            return true;
        };
        auto openCam = [&](int index) {
            int previous = camIndex;
            // Once eski kamerayi birak: bazi kameralar/USB portlari ayni anda iki akisi kaldiramaz
            cam.release();
#ifdef _WIN32
            bool ok = tryOpen(index, cv::CAP_MSMF) || tryOpen(index, cv::CAP_DSHOW);
#else
            bool ok = tryOpen(index, cv::CAP_ANY);
#endif
            if (ok) {
                camIndex = index;
                std::cout << "Kamera " << index << " acildi\n";
                return true;
            }
            std::cerr << "Kamera " << index << " acilamadi\n";
            if (previous != index) {                          // eskisine geri don
#ifdef _WIN32
                tryOpen(previous, cv::CAP_MSMF) || tryOpen(previous, cv::CAP_DSHOW);
#else
                tryOpen(previous, cv::CAP_ANY);
#endif
            }
            return false;
        };
        // Sonraki calisan kamerayi bul (sona gelince basa doner)
        auto nextCam = [&]() {
            for (int step = 1; step <= camCount; ++step)
                if (openCam((camIndex + step) % camCount)) return true;
            return false;
        };
        if (!openCam(camIndex) && !nextCam()) { std::cerr << "Hic kamera bulunamadi\n"; return 1; }
        Skeleton prevSk;

        const std::string win = "Kinext 1.0";
        cv::namedWindow(win, cv::WINDOW_NORMAL);
        ClickState click;
        cv::setMouseCallback(win, onMouse, &click);

        std::string lastEvent;
        auto lastEventTime = std::chrono::steady_clock::now();
        auto prev = std::chrono::steady_clock::now();
        double fps = 0;

        cv::Mat frame;
        while (true) {
            if (!cam.read(frame) || frame.empty()) {
                std::cerr << "Kamera kesildi, baska kamera araniyor...\n";
                if (!nextCam()) break;
                continue;
            }
            Keypoints kp{};
            Skeleton sk;
            if (pose.estimate(frame, kp)) sk = smoothSkeleton(prevSk, toKinect(kp));
            prevSk = sk;

            for (const auto& e : gestures.update(sk)) {
                lastEvent = mirrorName(e, mirror);
                lastEventTime = std::chrono::steady_clock::now();
                std::cout << "JEST: " << lastEvent << std::endl;
            }
            if (printJson) std::cout << sk.toJson(static_cast<float>(frame.cols), static_cast<float>(frame.rows)) << "\n";

            drawSkeleton(frame, sk);
            if (mirror) cv::flip(frame, frame, 1);

            // FPS
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - prev).count();
            prev = now;
            if (dt > 0) fps = fps * 0.9 + (1.0 / dt) * 0.1;

            cv::rectangle(frame, cv::Rect(0, 0, frame.cols, 44), cv::Scalar(20, 20, 20), cv::FILLED);
            cv::putText(frame, "KINEXT 1.0  |  " + pose.backend() + "  |  " + std::to_string(static_cast<int>(fps)) + " FPS  |  KAMERA " + std::to_string(camIndex) +
                                (sk.valid ? "  |  OYUNCU ALGILANDI" : "  |  KAMERAYA GECIN"),
                        { 12, 30 }, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        sk.valid ? cv::Scalar(60, 220, 60) : cv::Scalar(80, 160, 255), 2, cv::LINE_AA);

            if (!lastEvent.empty() &&
                std::chrono::duration<double>(now - lastEventTime).count() < 1.0) {
                cv::putText(frame, lastEvent, { 24, frame.rows - 40 }, cv::FONT_HERSHEY_DUPLEX, 1.6,
                            cv::Scalar(0, 200, 255), 3, cv::LINE_AA);
            }

            int chosen = drawCameraButtons(frame, camNames, camIndex, click);
            if (chosen >= 0 && chosen != camIndex) {
                if (openCam(chosen)) { prevSk = Skeleton{}; gestures = GestureDetector{}; }
                else std::cerr << "Kamera acilamadi: " << camNames[chosen] << "\n";
            }

            cv::imshow(win, frame);
            int key = cv::waitKey(1) & 0xFF;
            if (key == 27 || key == 'q') break;
            if (key == 'm') mirror = !mirror;
            if (key == 'j') printJson = !printJson;
            if (key == 'c') { nextCam(); prevSk = Skeleton{}; gestures = GestureDetector{}; }
        }
    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
