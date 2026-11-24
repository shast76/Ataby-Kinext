// Kinext 1.0 - telefon kamerasi: Wi-Fi uzerinden MJPEG akisi okuyucu
// IP Webcam (Android) ve benzeri uygulamalar goruntuyu http://<ip>:<port>/video adresinden
// "multipart/x-mixed-replace" (art arda JPEG kareleri) olarak yayinlar. Bu sinif o akisi okur.
#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

class MjpegStream {
public:
    MjpegStream() = default;
    ~MjpegStream();
    MjpegStream(const MjpegStream&) = delete;
    MjpegStream& operator=(const MjpegStream&) = delete;

    // "192.168.1.35", "192.168.1.35:8080" ya da tam adres ("http://192.168.1.35:8080/video") kabul eder
    bool open(const std::string& address, int timeoutMs = 4000);
    void close();
    bool isOpen() const { return sock_ >= 0; }
    // Siradaki tam JPEG karesini okur ve cozer (bloklayan cagri)
    bool read(cv::Mat& frame);

    // Kullanicinin yazdigi adresi tam URL'ye cevirir (ekranda gostermek icin de kullanilir)
    static std::string normalize(const std::string& address);
    const std::string& url() const { return url_; }
    const std::string& error() const { return error_; }

private:
    bool fill();   // sokete gelen veriyi tampona ekler
    void drain();  // bekleyen tum veriyi al (gecikme birikmesin)

    long long sock_ = -1;
    std::string url_, error_;
    std::vector<unsigned char> buf_;
};
