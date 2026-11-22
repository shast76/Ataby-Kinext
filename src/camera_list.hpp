// Kinext 1.0 - bilgisayardaki kameralarin isimlerini listeler
// Windows: Media Foundation (OpenCV'nin CAP_MSMF arka ucuyla ayni sira)
#pragma once
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>

inline std::vector<std::string> listCameras() {
    std::vector<std::string> names;
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(MFStartup(MF_VERSION))) { if (SUCCEEDED(hrCo)) CoUninitialize(); return names; }

    IMFAttributes* attr = nullptr;
    if (SUCCEEDED(MFCreateAttributes(&attr, 1)) &&
        SUCCEEDED(attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
        IMFActivate** devices = nullptr;
        UINT32 count = 0;
        if (SUCCEEDED(MFEnumDeviceSources(attr, &devices, &count))) {
            for (UINT32 i = 0; i < count; ++i) {
                WCHAR* wname = nullptr;
                UINT32 len = 0;
                std::string name = "Kamera " + std::to_string(i);
                if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &wname, &len))) {
                    int n = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
                    if (n > 1) {
                        name.assign(static_cast<size_t>(n - 1), '\0');
                        WideCharToMultiByte(CP_UTF8, 0, wname, -1, name.data(), n, nullptr, nullptr);
                    }
                    CoTaskMemFree(wname);
                }
                names.push_back(name);
                devices[i]->Release();
            }
            CoTaskMemFree(devices);
        }
    }
    if (attr) attr->Release();
    MFShutdown();
    if (SUCCEEDED(hrCo)) CoUninitialize();
    return names;
}
#else
inline std::vector<std::string> listCameras() { return {}; }
#endif

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
