// Kinext 1.0 - sesli komutlar (Windows SAPI 5, uygulama ici tanima motoru)
#include "voice.hpp"

#ifndef _WIN32

void Voice::start(int) { setError("Sesli komut sadece Windows'ta calisir"); }
void Voice::stop() {}
void Voice::run(int) {}

#else

#include <windows.h>
#include <sapi.h>
#include <iterator>
#include <vector>

namespace {

std::string utf8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

template <class T> void release(T*& p) { if (p) { p->Release(); p = nullptr; } }

// Kategorideki (tanima motorlari, mikrofonlar) ilk uygun nesne
ISpObjectToken* findToken(const wchar_t* category, const wchar_t* required) {
    ISpObjectTokenCategory* cat = nullptr;
    IEnumSpObjectTokens* en = nullptr;
    ISpObjectToken* tok = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, __uuidof(ISpObjectTokenCategory), reinterpret_cast<void**>(&cat)))
        && SUCCEEDED(cat->SetId(category, FALSE))
        && SUCCEEDED(cat->EnumTokens(required, nullptr, &en))) {
        ULONG count = 0;
        if (SUCCEEDED(en->GetCount(&count)) && count > 0) en->Item(0, &tok);
    }
    release(en); release(cat);
    return tok;
}

// Windows'ta varsayilan secili mikrofon
ISpObjectToken* defaultAudioIn() {
    ISpObjectTokenCategory* cat = nullptr;
    ISpObjectToken* tok = nullptr;
    WCHAR* id = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, __uuidof(ISpObjectTokenCategory), reinterpret_cast<void**>(&cat)))
        && SUCCEEDED(cat->SetId(SPCAT_AUDIOIN, FALSE))
        && SUCCEEDED(cat->GetDefaultTokenId(&id)) && id
        && SUCCEEDED(CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, __uuidof(ISpObjectToken), reinterpret_cast<void**>(&tok)))) {
        if (FAILED(tok->SetId(nullptr, id, FALSE))) release(tok);
    }
    if (id) CoTaskMemFree(id);
    release(cat);
    return tok;
}

std::string tokenName(ISpObjectToken* tok) {
    WCHAR* desc = nullptr;
    std::string s;
    if (tok && SUCCEEDED(tok->GetStringValue(nullptr, &desc)) && desc) { s = utf8(desc); CoTaskMemFree(desc); }
    return s;
}

void clearEvent(SPEVENT& e) {
    if (e.elParamType == SPET_LPARAM_IS_OBJECT && e.lParam) reinterpret_cast<IUnknown*>(e.lParam)->Release();
    else if ((e.elParamType == SPET_LPARAM_IS_POINTER || e.elParamType == SPET_LPARAM_IS_STRING) && e.lParam)
        CoTaskMemFree(reinterpret_cast<void*>(e.lParam));
    e.lParam = 0;
}

struct Phrase { const wchar_t* text; VoiceCmd cmd; };

// "Kinext" bilinmeyen bir kelime oldugu icin motor onu harflerden okur; "Kinect" de kabul edilsin.
const Phrase EN_PHRASES[] = {
    { L"home", VoiceCmd::Home }, { L"go home", VoiceCmd::Home },
    { L"games", VoiceCmd::Games }, { L"my games", VoiceCmd::Games },
    { L"store", VoiceCmd::Store }, { L"settings", VoiceCmd::Settings },
    { L"back", VoiceCmd::Back }, { L"stop", VoiceCmd::Back }, { L"quit game", VoiceCmd::Back },
    { L"play again", VoiceCmd::Again }, { L"again", VoiceCmd::Again },
    { L"play balloon pop", VoiceCmd::PlayBalloon }, { L"play balloons", VoiceCmd::PlayBalloon },
    { L"play dodge", VoiceCmd::PlayDodge },
    { L"play bowling", VoiceCmd::PlayBowling },
    { L"play goalkeeper", VoiceCmd::PlayKeeper }, { L"play keeper", VoiceCmd::PlayKeeper },
    { L"full screen", VoiceCmd::Fullscreen }, { L"calibrate", VoiceCmd::Calibrate },
};
const Phrase TR_PHRASES[] = {
    { L"ana sayfa", VoiceCmd::Home }, { L"eve dön", VoiceCmd::Home },
    { L"oyunlar", VoiceCmd::Games }, { L"mağaza", VoiceCmd::Store }, { L"ayarlar", VoiceCmd::Settings },
    { L"geri", VoiceCmd::Back }, { L"dur", VoiceCmd::Back }, { L"oyundan çık", VoiceCmd::Back },
    { L"tekrar", VoiceCmd::Again }, { L"tekrar oyna", VoiceCmd::Again },
    { L"balon patlat", VoiceCmd::PlayBalloon }, { L"balon oyna", VoiceCmd::PlayBalloon },
    { L"engelden kaç", VoiceCmd::PlayDodge },
    { L"bowling oyna", VoiceCmd::PlayBowling }, { L"bowling", VoiceCmd::PlayBowling },
    { L"kaleci oyna", VoiceCmd::PlayKeeper }, { L"kaleci", VoiceCmd::PlayKeeper },
    { L"tam ekran", VoiceCmd::Fullscreen }, { L"kalibrasyon", VoiceCmd::Calibrate },
};
const wchar_t* WAKE[] = { L"kinext", L"kinect" };

} // namespace

void Voice::start(int lang) {
    stop();
    stop_ = false; starting_ = true;
    setError(""); setStatus("");
    th_ = std::thread(&Voice::run, this, lang);
}

void Voice::stop() {
    stop_ = true;
    if (th_.joinable()) th_.join();
    running_ = false; starting_ = false;
    std::lock_guard<std::mutex> l(m_);
    queue_.clear();
}

void Voice::run(int lang) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ISpRecognizer* rec = nullptr;
    ISpRecoContext* ctx = nullptr;
    ISpRecoGrammar* gram = nullptr;
    ISpObjectToken* engine = nullptr;
    ISpObjectToken* mic = nullptr;
    bool turkish = false;

    auto fail = [&](const char* msg) { setError(msg); };

    do {
        if (FAILED(CoCreateInstance(CLSID_SpInprocRecognizer, nullptr, CLSCTX_ALL, __uuidof(ISpRecognizer), reinterpret_cast<void**>(&rec)))) {
            fail("Konusma tanima baslatilamadi"); break;
        }
        // Tanima motoru: once secili dil, yoksa Ingilizce (ABD, Ingiltere, sonra herhangi biri)
        if (lang == 0) { engine = findToken(SPCAT_RECOGNIZERS, L"Language=41F"); turkish = engine != nullptr; }
        if (!engine) engine = findToken(SPCAT_RECOGNIZERS, L"Language=409");
        if (!engine) engine = findToken(SPCAT_RECOGNIZERS, L"Language=809");
        if (!engine) engine = findToken(SPCAT_RECOGNIZERS, nullptr);
        if (!engine) { fail("Konusma tanima motoru yok (Windows Ayarlar > Zaman ve dil > Konusma)"); break; }
        if (FAILED(rec->SetRecognizer(engine))) { fail("Konusma tanima motoru acilamadi"); break; }

        mic = defaultAudioIn();
        if (!mic) { fail("Mikrofon bulunamadi"); break; }
        if (FAILED(rec->SetInput(mic, TRUE))) { fail("Mikrofon acilamadi (Gizlilik > Mikrofon iznini kontrol et)"); break; }

        if (FAILED(rec->CreateRecoContext(&ctx))) { fail("Konusma tanima baslatilamadi"); break; }
        ctx->SetNotifyWin32Event();
        ctx->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION));

        // Komut dil bilgisi: "<kinext> <komut>" (tek kural, her ifade bir yol, komut kodu ozellikte)
        if (FAILED(ctx->CreateGrammar(1, &gram))) { fail("Komut listesi olusturulamadi"); break; }
        SPSTATEHANDLE rule = nullptr;
        if (FAILED(gram->GetRule(L"command", 0, SPRAF_TopLevel | SPRAF_Active, TRUE, &rule))) { fail("Komut listesi olusturulamadi"); break; }
        const Phrase* list = turkish ? TR_PHRASES : EN_PHRASES;
        size_t count = turkish ? std::size(TR_PHRASES) : std::size(EN_PHRASES);
        bool ok = true;
        for (const wchar_t* wake : WAKE)
            for (size_t i = 0; i < count && ok; ++i) {
                std::wstring text = std::wstring(wake) + L" " + list[i].text;
                SPPROPERTYINFO prop{};
                prop.pszName = L"cmd";
                prop.ulId = static_cast<ULONG>(list[i].cmd);
                prop.vValue.vt = VT_EMPTY;
                ok = SUCCEEDED(gram->AddWordTransition(rule, nullptr, text.c_str(), L" ", SPWT_LEXICAL, 1.0f, &prop));
            }
        if (!ok || FAILED(gram->Commit(0))) { fail("Komut listesi olusturulamadi"); break; }
        if (FAILED(gram->SetRuleState(nullptr, nullptr, SPRS_ACTIVE)) || FAILED(rec->SetRecoState(SPRST_ACTIVE))) {
            fail("Dinleme baslatilamadi"); break;
        }

        english_ = (lang == 0 && !turkish);
        setStatus(tokenName(engine));
        running_ = true; starting_ = false;

        HANDLE ev = ctx->GetNotifyEventHandle();
        while (!stop_) {
            if (WaitForSingleObject(ev, 150) != WAIT_OBJECT_0) continue;
            SPEVENT e{};
            ULONG fetched = 0;
            while (!stop_ && ctx->GetEvents(1, &e, &fetched) == S_OK && fetched == 1) {
                if (e.eEventId == SPEI_RECOGNITION && e.elParamType == SPET_LPARAM_IS_OBJECT) {
                    ISpRecoResult* res = reinterpret_cast<ISpRecoResult*>(e.lParam);
                    SPPHRASE* ph = nullptr;
                    if (SUCCEEDED(res->GetPhrase(&ph)) && ph) {
                        // Dusuk guvenli eslesmeler (ortam sesi, TV) yok sayilir
                        if (ph->pProperties && ph->Rule.Confidence != SP_LOW_CONFIDENCE) {
                            WCHAR* text = nullptr;
                            std::string heard;
                            if (SUCCEEDED(res->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &text, nullptr)) && text) {
                                heard = utf8(text); CoTaskMemFree(text);
                            }
                            push(static_cast<VoiceCmd>(ph->pProperties->ulId), heard);
                        }
                        CoTaskMemFree(ph);
                    }
                }
                clearEvent(e);
            }
        }
        rec->SetRecoState(SPRST_INACTIVE);
    } while (false);

    running_ = false; starting_ = false;
    release(gram); release(ctx); release(mic); release(engine); release(rec);
    if (SUCCEEDED(co)) CoUninitialize();
}

#endif
