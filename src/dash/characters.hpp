// Kinext - kendi karakterin: PNG parcalarindan olusan maskotlar
//
// Bir karakter = bir klasor. Icinde su resimler bulunur (hepsi istege bagli, eksik parca cizilmez):
//   head.png  torso.png  upper_arm.png  lower_arm.png  hand.png  upper_leg.png  lower_leg.png  foot.png
//   head_back.png, torso_back.png  : 3D oyunlarda arkadan gorunus (yoksa on yuz kullanilir)
//   *_left.png                     : sol taraf icin ayri cizim (yoksa sag taraf aynalanir)
//   character.txt                  : name=Adi   description=Kisa aciklama
// Kural: her parca dik durur; ustten %10 ve alttan %10 cizgileri eklem noktalaridir
// (ornegin ust kol: ust cizgi omuz, alt cizgi dirsek). Sablon: assets/character_template
//
// Klasorler: <Kinext>/characters ve %APPDATA%\Kinext\characters. Yeni klasor eklenince
// otomatik yuklenir; bir klasoru Kinext penceresine surukleyip birakmak da yeterli.
#pragma once
#include "r3d.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <vector>

struct CustomCharacter {
    std::string name, desc, dir;
    std::map<std::string, Texture2D> tex;   // "head", "upper_arm_left" ...
    const Texture2D* get(const std::string& part, bool left = false) const {
        if (left) { auto it = tex.find(part + "_left"); if (it != tex.end()) return &it->second; }
        auto it = tex.find(part);
        return it == tex.end() ? nullptr : &it->second;
    }
};

class CharacterLibrary {
public:
    static CharacterLibrary& get() { static CharacterLibrary lib; return lib; }

    const std::vector<CustomCharacter>& list() const { return chars_; }
    int count() const { return static_cast<int>(chars_.size()); }

    static std::filesystem::path userDir() {
        namespace fs = std::filesystem;
#ifdef _WIN32
        const char* base = std::getenv("APPDATA");
        fs::path p = base ? fs::path(base) / "Kinext" / "characters" : fs::path("characters");
#else
        const char* home = std::getenv("HOME");
        fs::path p = home ? fs::path(home) / ".config" / "kinext" / "characters" : fs::path("characters");
#endif
        std::error_code ec;
        fs::create_directories(p, ec);
        return p;
    }

    // Klasorleri tara; yeni ya da degisen karakter varsa yeniden yukle. true: liste degisti
    bool refresh() {
        namespace fs = std::filesystem;
        std::map<std::string, long long> found;   // klasor -> en yeni degisiklik zamani
        for (const fs::path& root : { fs::path("assets/characters"), fs::path("characters"), userDir() }) {
            std::error_code ec;
            if (!fs::is_directory(root, ec)) continue;
            for (const auto& e : fs::directory_iterator(root, ec)) {
                if (!e.is_directory(ec)) continue;
                long long stamp = 0; bool any = false;
                for (const auto& f : fs::directory_iterator(e.path(), ec)) {
                    std::string ext = f.path().extension().string();
                    if (ext != ".png" && ext != ".PNG" && ext != ".txt") continue;
                    any = any || ext != ".txt";
                    stamp = (std::max)(stamp, static_cast<long long>(f.last_write_time(ec).time_since_epoch().count()));
                }
                if (any) found[fs::absolute(e.path(), ec).string()] = stamp;
            }
        }
        if (found == stamps_) return false;
        stamps_ = found;
        unloadAll();
        for (const auto& [dir, s] : found) load(dir);
        std::sort(chars_.begin(), chars_.end(), [](const CustomCharacter& a, const CustomCharacter& b) { return a.name < b.name; });
        return true;
    }

    // Surukle-birak: klasoru (ya da icindeki bir PNG'yi) kullanici karakter klasorune kopyala
    std::string import(const std::string& dropped) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path src = dropped;
        if (!fs::is_directory(src, ec)) src = src.parent_path();
        bool hasPart = false;
        for (const char* p : PARTS) if (fs::exists(src / (std::string(p) + ".png"), ec)) hasPart = true;
        if (!hasPart) return {};
        fs::path dst = userDir() / src.filename();
        if (fs::absolute(src, ec) == fs::absolute(dst, ec)) return src.filename().string();
        fs::create_directories(dst, ec);
        for (const auto& f : fs::directory_iterator(src, ec)) {
            std::string ext = f.path().extension().string();
            if (ext == ".png" || ext == ".PNG" || ext == ".txt") fs::copy_file(f.path(), dst / f.path().filename(), fs::copy_options::overwrite_existing, ec);
        }
        return src.filename().string();
    }

    int indexOf(const std::string& name) const {
        for (int i = 0; i < count(); ++i) if (chars_[i].name == name) return i;
        return -1;
    }

    static constexpr const char* PARTS[] = { "head", "torso", "upper_arm", "lower_arm", "hand", "upper_leg", "lower_leg", "foot", "head_back", "torso_back" };

private:
    ~CharacterLibrary() = default;
    void unloadAll() {
        if (IsWindowReady()) for (auto& c : chars_) for (auto& [k, t] : c.tex) UnloadTexture(t);
        chars_.clear();
    }
    void load(const std::string& dir) {
        namespace fs = std::filesystem;
        CustomCharacter c;
        c.dir = dir;
        c.name = fs::path(dir).filename().string();
        std::ifstream in(fs::path(dir) / "character.txt");
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            if (k == "name" && !v.empty()) c.name = v.substr(0, 24);
            else if (k == "description") c.desc = v.substr(0, 40);
        }
        for (const char* p : PARTS)
            for (const std::string& suffix : { std::string(), std::string("_left") }) {
                fs::path f = fs::path(dir) / (std::string(p) + suffix + ".png");
                std::error_code ec;
                if (!fs::exists(f, ec)) continue;
                Texture2D t = LoadTexture(f.string().c_str());
                if (t.id == 0) continue;
                GenTextureMipmaps(&t);
                SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
                c.tex[std::string(p) + suffix] = t;
            }
        if (!c.tex.empty()) chars_.push_back(std::move(c));
    }

    std::vector<CustomCharacter> chars_;
    std::map<std::string, long long> stamps_;
};

// ---------------- cizim ----------------
// Bir parcayi a (ust pivot, %10) ile b (alt pivot, %90) arasina yerlestirir
inline void drawPart2D(const Texture2D* t, Vector2 a, Vector2 b, bool flip, float alpha) {
    if (!t || t->id == 0) return;
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.f) return;
    float H = len / 0.8f, W = t->width * H / t->height;
    Rectangle src = { 0, 0, flip ? -static_cast<float>(t->width) : static_cast<float>(t->width), static_cast<float>(t->height) };
    DrawTexturePro(*t, src, { (a.x + b.x) / 2, (a.y + b.y) / 2, W, H }, { W / 2, H / 2 },
                   std::atan2(dy, dx) * RAD2DEG - 90.f, ColorAlpha(WHITE, alpha));
}

// Parca yerlesimi: iskeletten her parcanin iki pivot noktasini cikarir (2D ve 3D ortak)
template <class V, class GetJ, class Add, class Sub, class Mul, class Norm>
void customPartPlan(GetJ J, Add add, Sub sub, Mul mul, Norm norm, float t, V up,
                    std::vector<std::tuple<const char*, bool, V, V>>& out) {
    V sc = J(ShoulderCenter), hc = J(HipCenter);
    V neck = add(sc, mul(up, t * 0.08f));
    for (int side = 0; side < 2; ++side) {   // bacaklar (arkada)
        bool left = side == 0;
        Joint hi = left ? HipLeft : HipRight, kn = left ? KneeLeft : KneeRight, an = left ? AnkleLeft : AnkleRight;
        out.push_back({ "upper_leg", left, J(hi), J(kn) });
        out.push_back({ "lower_leg", left, J(kn), J(an) });
        V legDir = norm(sub(J(an), J(kn)));
        out.push_back({ "foot", left, J(an), add(J(an), mul(legDir, t * 0.3f)) });
    }
    out.push_back({ "torso", false, add(sc, mul(up, t * 0.02f)), hc });
    out.push_back({ "head", false, add(neck, mul(up, t * 0.85f)), neck });
    for (int side = 0; side < 2; ++side) {   // kollar (onde)
        bool left = side == 0;
        Joint sh = left ? ShoulderLeft : ShoulderRight, el = left ? ElbowLeft : ElbowRight, wr = left ? WristLeft : WristRight;
        out.push_back({ "upper_arm", left, J(sh), J(el) });
        out.push_back({ "lower_arm", left, J(el), J(wr) });
        V armDir = norm(sub(J(wr), J(el)));
        out.push_back({ "hand", left, J(wr), add(J(wr), mul(armDir, t * 0.36f)) });
    }
}
