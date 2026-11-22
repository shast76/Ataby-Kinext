// Kinext 1.0 - kamera adlari (Windows: Media Foundation)
#include "camera_list.hpp"

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>

std::vector<std::string> listCameras() {
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
std::vector<std::string> listCameras() { return {}; }
#endif
