// Kinext 1.0 - Oyunlar: menu + Balon Patlat + Engelden Kac
//   ESC : oyundan menuye / menuden cikis    C : sonraki kamera    Fare: sag ustte kamera secimi (menude)
#include "camera.hpp"
#include "games/balloon.hpp"
#include "games/dodge.hpp"
#include "games/game.hpp"
#include "pose.hpp"
#include <opencv2/highgui.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

struct ClickState { int x = -1, y = -1; };
static void onMouse(int event, int x, int y, int, void* data) {
    if (event == cv::EVENT_LBUTTONDOWN) { auto* c = static_cast<ClickState*>(data); c->x = x; c->y = y; }
}

int main() {
#ifdef _WIN32
    _putenv_s("OPENCV_VIDEOIO_MSMF_ENABLE_HW_TRANSFORMS", "0");
#endif
    try {
        std::string model = std::filesystem::exists("models/movenet_thunder.onnx")
            ? "models/movenet_thunder.onnx" : "models/movenet_lightning.onnx";
        PoseEstimator pose(model, true);
        CameraManager cams;
        if (!cams.openFirst(0)) { std::cerr << "Hic kamera bulunamadi\n"; return 1; }

        std::vector<std::unique_ptr<Game>> games;
        games.push_back(std::make_unique<BalloonGame>());
        games.push_back(std::make_unique<DodgeGame>());

        const std::string win = "Kinext 1.0 - Oyunlar";
        cv::namedWindow(win, cv::WINDOW_NORMAL);
        ClickState click;
        cv::setMouseCallback(win, onMouse, &click);

        int current = -1;                       // -1: menu
        std::vector<HoverButton> menuButtons;
        HoverButton exitButton;
        cv::Size layoutFor;
        Skeleton prevSk;
        auto last = std::chrono::steady_clock::now();
        cv::Mat frame;

        while (true) {
            if (!cams.read(frame)) {
                std::cerr << "Kamera kesildi, baska kamera araniyor...\n";
                if (!cams.next()) break;
                continue;
            }
            auto now = std::chrono::steady_clock::now();
            double dt = (std::min)(std::chrono::duration<double>(now - last).count(), 0.1);
            last = now;

            Keypoints kp{};
            Skeleton sk;
            if (pose.estimate(frame, kp)) sk = smoothSkeleton(prevSk, toKinect(kp));
            prevSk = sk;

            // Ayna gibi goster: oyuncu sagina gidince ekranda da saga gitsin
            cv::Mat view;
            cv::flip(frame, view, 1);
            Skeleton msk = mirrorSkeleton(sk, view.cols);
            view *= 0.45;                        // kamera goruntusunu karart, oyun one ciksin
            drawBody(view, msk);

            // Menu duzeni (pencere boyutu degisirse yeniden kur)
            if (layoutFor != view.size()) {
                layoutFor = view.size();
                menuButtons.clear();
                int bw = view.cols * 3 / 10, bh = view.rows / 5, gap = view.cols / 20;
                int total = static_cast<int>(games.size()) * bw + (static_cast<int>(games.size()) - 1) * gap;
                for (size_t i = 0; i < games.size(); ++i)
                    menuButtons.push_back({ cv::Rect((view.cols - total) / 2 + static_cast<int>(i) * (bw + gap), view.rows * 2 / 5, bw, bh),
                                            games[i]->name(), i == 0 ? cv::Scalar(180, 90, 200) : cv::Scalar(60, 90, 200) });
                exitButton = { cv::Rect(view.cols / 2 - bw / 4, view.rows * 3 / 4, bw / 2, bh / 2), "CIKIS", cv::Scalar(80, 80, 80) };
            }

            bool quit = false;
            if (current < 0) {
                centerText(view, "KINEXT 1.0", view.rows / 6, 2.2, cv::Scalar(255, 255, 255), 3);
                centerText(view, "Elini bir oyunun ustunde tut", view.rows / 6 + 50, 0.9, cv::Scalar(200, 200, 200), 1);
                for (size_t i = 0; i < menuButtons.size(); ++i)
                    if (menuButtons[i].update(msk, dt, view)) { current = static_cast<int>(i); games[i]->reset(view.size()); }
                if (exitButton.update(msk, dt, view)) quit = true;
                if (!msk.valid) centerText(view, "KAMERAYA GEC", view.rows - 30, 1.0, cv::Scalar(80, 160, 255));
                drawHands(view, msk);
                int chosen = cams.drawButtons(view, click.x, click.y, 10);
                if (chosen >= 0 && chosen != cams.index() && cams.open(chosen)) prevSk = Skeleton{};
            } else {
                switch (games[current]->update(msk, dt, view)) {
                case SceneResult::Restart: games[current]->reset(view.size()); break;
                case SceneResult::ToMenu:  current = -1; break;
                default: break;
                }
            }
            click = {};

            cv::imshow(win, view);
            int key = cv::waitKey(1) & 0xFF;
            if (key == 27) { if (current >= 0) current = -1; else quit = true; }
            if (key == 'c') { cams.next(); prevSk = Skeleton{}; }
            if (quit) break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
