#include "mjpeg.hpp"
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
static bool netInit() {
    static bool ok = [] { WSADATA d; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }();
    return ok;
}
static void closeSock(long long s) { closesocket(static_cast<SOCKET>(s)); }
#else
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using socklen_type = socklen_t;
static bool netInit() { return true; }
static void closeSock(long long s) { ::close(static_cast<int>(s)); }
#endif

namespace {
struct Url { std::string host, port = "8080", path = "/video"; };

Url parse(const std::string& full) {
    Url u;
    std::string s = full;
    if (s.rfind("http://", 0) == 0) s = s.substr(7);
    auto slash = s.find('/');
    std::string hostport = slash == std::string::npos ? s : s.substr(0, slash);
    if (slash != std::string::npos) u.path = s.substr(slash);
    auto colon = hostport.find(':');
    u.host = colon == std::string::npos ? hostport : hostport.substr(0, colon);
    if (colon != std::string::npos) u.port = hostport.substr(colon + 1);
    return u;
}
}

std::string MjpegStream::normalize(const std::string& address) {
    std::string a = address;
    a.erase(std::remove_if(a.begin(), a.end(), [](unsigned char c) { return c == ' ' || c == '\t'; }), a.end());
    if (a.empty()) return a;
    if (a.rfind("https://", 0) == 0) a = "http://" + a.substr(8);   // uygulamalar yerel agda http kullanir
    if (a.rfind("http://", 0) != 0) a = "http://" + a;
    Url u = parse(a);
    bool hasPath = a.find('/', 7) != std::string::npos;
    return "http://" + u.host + ":" + u.port + (hasPath ? u.path : "/video");
}

MjpegStream::~MjpegStream() { close(); }

void MjpegStream::close() {
    if (sock_ >= 0) closeSock(sock_);
    sock_ = -1;
    buf_.clear();
}

bool MjpegStream::open(const std::string& address, int timeoutMs) {
    close();
    error_.clear();
    url_ = normalize(address);
    if (url_.empty()) { error_ = "Adres bos"; return false; }
    if (!netInit()) { error_ = "Ag baslatilamadi"; return false; }
    Url u = parse(url_);

    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(u.host.c_str(), u.port.c_str(), &hints, &res) != 0 || !res) { error_ = "Adres bulunamadi: " + u.host; return false; }

    auto s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
#ifdef _WIN32
    if (s == INVALID_SOCKET) { freeaddrinfo(res); error_ = "Soket acilamadi"; return false; }
    DWORD tv = static_cast<DWORD>(timeoutMs);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    if (s < 0) { freeaddrinfo(res); error_ = "Soket acilamadi"; return false; }
    timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    int rc = connect(s, res->ai_addr, static_cast<socklen_type>(res->ai_addrlen));
    freeaddrinfo(res);
    sock_ = static_cast<long long>(s);
    if (rc != 0) { close(); error_ = "Baglanilamadi (telefon ve bilgisayar ayni Wi-Fi'da mi?)"; return false; }

    std::string req = "GET " + u.path + " HTTP/1.1\r\nHost: " + u.host + ":" + u.port +
                      "\r\nUser-Agent: Kinext/1.1\r\nConnection: keep-alive\r\n\r\n";
    if (send(s, req.c_str(), static_cast<int>(req.size()), 0) <= 0) { close(); error_ = "Istek gonderilemedi"; return false; }

    // Ilk kare gelene kadar bekle: adres dogru mu, gercekten goruntu akisi mi?
    cv::Mat test;
    if (!read(test)) {
        std::string head(buf_.begin(), buf_.begin() + (std::min)(buf_.size(), static_cast<size_t>(64)));
        close();
        error_ = head.rfind("HTTP/1.1 404", 0) == 0 || head.rfind("HTTP/1.0 404", 0) == 0
                     ? "Adres bulundu ama goruntu yolu yanlis (genelde sonu /video olur)"
                     : "Goruntu alinamadi";
        return false;
    }
    return true;
}

bool MjpegStream::fill() {
    unsigned char tmp[65536];
#ifdef _WIN32
    int n = recv(static_cast<SOCKET>(sock_), reinterpret_cast<char*>(tmp), sizeof(tmp), 0);
#else
    int n = static_cast<int>(recv(static_cast<int>(sock_), tmp, sizeof(tmp), 0));
#endif
    if (n <= 0) return false;
    buf_.insert(buf_.end(), tmp, tmp + n);
    // Tampon cok buyurse (bozuk akis) eski veriyi at
    if (buf_.size() > 8 * 1024 * 1024) buf_.erase(buf_.begin(), buf_.end() - 1024 * 1024);
    return true;
}

// Sokette bekleyen tum veriyi al: telefon bizden hizli gonderiyorsa eski kareler birikmesin
void MjpegStream::drain() {
    for (int i = 0; i < 64; ++i) {
#ifdef _WIN32
        u_long avail = 0;
        if (ioctlsocket(static_cast<SOCKET>(sock_), FIONREAD, &avail) != 0 || avail == 0) return;
#else
        int avail = 0;
        if (ioctl(static_cast<int>(sock_), FIONREAD, &avail) != 0 || avail <= 0) return;
#endif
        if (!fill()) return;
    }
}

bool MjpegStream::read(cv::Mat& frame) {
    if (sock_ < 0) return false;
    drain();
    // JPEG karesi FF D8 ile baslar, FF D9 ile biter. Aradaki HTTP sinir/basliklarini onemsemiyoruz.
    static const unsigned char SOI[] = { 0xFF, 0xD8 }, EOI[] = { 0xFF, 0xD9 };
    for (int guard = 0; guard < 4000; ++guard) {
        // Tampondaki tam kareleri bul; birden fazlaysa en yenisini al (gecikme birikmesin)
        auto pos = buf_.begin();
        auto bestSoi = buf_.end(), bestEoi = buf_.end();
        while (true) {
            auto soi = std::search(pos, buf_.end(), SOI, SOI + 2);
            if (soi == buf_.end()) break;
            auto eoi = std::search(soi + 2, buf_.end(), EOI, EOI + 2);
            if (eoi == buf_.end()) break;
            bestSoi = soi; bestEoi = eoi;
            pos = eoi + 2;
        }
        if (bestSoi != buf_.end()) {
            std::vector<unsigned char> jpg(bestSoi, bestEoi + 2);
            buf_.erase(buf_.begin(), bestEoi + 2);
            cv::Mat img = cv::imdecode(jpg, cv::IMREAD_COLOR);
            if (!img.empty()) { frame = img; return true; }
            continue;
        }
        if (!fill()) { error_ = "Baglanti koptu"; return false; }
    }
    return false;
}
