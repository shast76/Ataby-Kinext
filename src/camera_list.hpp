// Kinext 1.0 - bilgisayardaki kameralarin isimlerini listeler
// Windows: Media Foundation (OpenCV'nin CAP_MSMF arka ucuyla ayni sira)
#pragma once
#include <string>
#include <vector>

#include <utility>

// Bilgisayardaki kameralarin adlari (Windows: Media Foundation sirasi, OpenCV CAP_MSMF ile ayni)
// Uygulamasi camera_list.cpp'de: windows.h baska kutuphanelerle (raylib) cakismasin diye.
std::vector<std::string> listCameras();


// OpenCV'nin yazi tipi Turkce/ozel karakter cizemiyor: ASCII'ye cevir ve kisalt
inline std::string asciiLabel(const std::string& s, size_t maxLen = 30) {
    static const std::pair<const char*, char> tr[] = {
        {"\xC3\xA7",'c'},{"\xC3\x87",'C'},{"\xC4\x9F",'g'},{"\xC4\x9E",'G'},
        {"\xC4\xB1",'i'},{"\xC4\xB0",'I'},{"\xC3\xB6",'o'},{"\xC3\x96",'O'},
        {"\xC5\x9F",'s'},{"\xC5\x9E",'S'},{"\xC3\xBC",'u'},{"\xC3\x9C",'U'}};
    std::string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { out += static_cast<char>(c); ++i; continue; }
        bool done = false;
        for (auto& [seq, rep] : tr)
            if (s.compare(i, 2, seq) == 0) { out += rep; i += 2; done = true; break; }
        if (done) continue;
        out += '?';                                   // diger ozel karakterler
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
    }
    if (out.size() > maxLen) out = out.substr(0, maxLen - 3) + "...";
    return out;
}
