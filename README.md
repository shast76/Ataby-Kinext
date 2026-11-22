<p align="center">
  <img src="docs/images/kinext-logo.png" width="640" alt="Ataby Kinext logosu">
</p>

Kinect olmadan, sıradan bir webcam ile Kinect tarzı hareket oyunları.
Hedef: PC (Windows/Linux), Baldic OS, native Xbox (UWP / Dev Mode) ve Raspberry Pi 5 tabanlı Kinext konsolu.

> Kinext, Microsoft ile bağlantılı değildir. "Kinect" ve "Xbox", Microsoft Corporation'ın ticari markalarıdır.

<p align="center">
  <img src="docs/images/home.png" width="49%" alt="Kinext Dashboard ana sayfa">
  <img src="docs/images/avatar.png" width="49%" alt="Maskot seçimi">
  <img src="docs/images/balloon.png" width="49%" alt="Balon Patlat">
  <img src="docs/images/dodge.png" width="49%" alt="Engelden Kaç">
</p>

<sub>Ekran görüntüleri programın kendisinden alınmıştır; kamera yerine el sallayan sahte bir oyuncu verisi kullanılmıştır, gerçek kamera görüntüsü içermez.</sub>

## Neler var

**Kinext Core** (`kinext_core`): Webcam görüntüsü → MoveNet poz modeli (ONNX Runtime, Windows'ta DirectML ile GPU) → **Kinect v1 formatında 20 eklemli iskelet** → jest algılama.

- Kinect SDK 1.8'deki eklem adları ve sırası (`HipCenter` … `FootRight`)
- Kişiye odaklı kırpma: model her karede oyuncunun bulunduğu bölgeye yakınlaşır, bacak ve ayak takibi çok daha iyi olur
- Titreme önleyici yumuşatma (hızlı hareketlerde gecikme yapmaz)
- Jestler: sol/sağ/iki el yukarı, sağa/sola eğilme, zıplama, sağ eli hızla savurma
- Kamera seçimi: Windows'taki kamera adlarıyla sağ üstte butonlar, `C` tuşu ile sıradaki kamera

**Kinext Dashboard** (`kinext_dashboard.exe`, ana program): Xbox One (2013–2015) tarzı ana ekran.

- **Ana Sayfa · Oyunlar · Mağaza · Ayarlar** sekmeleri, Metro kutuları, seçilebilir vurgu rengi
- **El ile kontrol:** elini kaldır, yarı saydam el imlecini bir kutunun üstünde tut, halka dolunca seçilir (fare de çalışır)
- Kontrol eli seçimi (otomatik / sol / sağ), kamera seçimi, tam ekran, ayarları sıfırlama
- **Maskotlar:** oyunlarda kamera görüntüsü yerine seçtiğin maskot olarak görünürsün: **Robo**, **Pofi**, **Astro**
- Kamera ve poz takibi arka planda ayrı bir iş parçacığında çalışır, arayüz akıcı kalır
- Çizim: [raylib](https://www.raylib.com/)

**Oyunlar**

- 🎈 **Balon Patlat**: yükselen balonları ellerinle patlat (60 saniye, zamanla hızlanır)
- 🧱 **Engelden Kaç**: 3 şeritte düşen bloklardan sağa-sola kayarak kaç (3 can)
- Mağazada yakında: Bowling, Pozu Tut, Kaleci

**Diğer programlar** (geliştirme/test için)

| Program | Ne yapar |
|---|---|
| `kinext.exe` | İskelet test aracı: kamera görüntüsü üstünde eklemleri ve jestleri gösterir, `J` ile JSON çıktısı verir. |
| `kinext_games.exe` | İlk OpenCV oyun sürümü (kamera görüntüsü üstünde). |

## Gerekenler (Windows)

1. **Visual Studio 2022+**: "C++ ile masaüstü geliştirme" iş yükü; *C++ CMake araçları* ve *vcpkg paket yöneticisi* bileşenleri seçili olsun.
2. **ONNX Runtime (DirectML)**: nuget.org'dan **Microsoft.ML.OnnxRuntime.DirectML** paketini indir (`.nupkg` aslında bir zip dosyası).
   Uzantısını `.zip` yapıp içini `third_party/onnxruntime/` klasörüne çıkar.
3. **Model**: Hugging Face'teki `Xenova/movenet-singlepose-lightning` deposundan `onnx/model.onnx` dosyasını indir,
   `models/movenet_lightning.onnx` olarak kaydet.
   Daha isabetli takip için isteğe bağlı: `Xenova/movenet-singlepose-thunder` → `models/movenet_thunder.onnx` (varsa otomatik kullanılır).

OpenCV ve raylib'i elle kurmana gerek yok; `vcpkg.json` sayesinde Visual Studio ilk açılışta kendisi indirip derler (ilk sefer 15–40 dakika sürebilir).

## Derleme ve çalıştırma

1. Visual Studio → **Klasör aç** → bu klasörü seç
2. Yapılandırma olarak **Kinext - Windows x64** seçili olsun; CMake ve vcpkg işini bitirene kadar bekle
3. Başlangıç öğesi olarak **kinext_dashboard.exe**'yi seç
4. **Ctrl+Shift+B** ile derle, ▶ ile çalıştır

Kamerayı TV'nin üstüne ya da göğüs-baş hizasına koy, 2–3 metre geride, tüm vücudun görünecek şekilde dur.

### Tuşlar

| Tuş | kinext_dashboard | kinext |
|---|---|---|
| `ESC` | Oyundan çık / ana sayfaya dön | Çıkış |
| `F11` | Tam ekran | |
| `C` | | Sıradaki kamera |
| `M` | | Aynalamayı aç/kapa |
| `J` | | İskeleti JSON olarak yazdır |

Ayarlar ve rekorlar exe'nin yanındaki `kinext_settings.txt` dosyasında tutulur.

## Proje yapısı

```
src/
  skeleton.hpp      Kinect formatında iskelet + yumuşatma
  pose.hpp/.cpp     MoveNet ile poz tahmini (ONNX Runtime + DirectML), kişiye odaklı kırpma
  gestures.hpp      Jest algılama
  camera_list.*     Windows kamera adları (Media Foundation)
  camera.hpp        Kamera açma/değiştirme
  tracker.*         Kamera + poz takibini arka planda çalıştırır
  dash/             Dashboard (raylib)
    dashboard_main.cpp  Sahneler: ilk kurulum, sekmeler, oyunlar
    ui.hpp          Tema, yazı tipleri, el imleci ve seçim sistemi
    avatar.hpp      Maskotlar (Robo, Pofi, Astro)
    games.hpp       Oyun altyapısı, mağaza kataloğu, kapaklar
    game_balloon.hpp, game_dodge.hpp
    settings.hpp    Ayarlar ve rekorlar
  main.cpp          İskelet test aracı (kinext.exe)
  games_main.cpp, games/   İlk OpenCV oyunları (kinext_games.exe)
assets/             Beyaz logo
```

## Yol haritası

1. ✅ Webcam ile iskelet takibi + jestler
2. ✅ İlk küçük oyunlar (Balon Patlat, Engelden Kaç)
3. ✅ Xbox One tarzı dashboard, mağaza ve maskotlar
4. Daha fazla oyun (bowling, poz taklidi, kaleci…)
5. Gerçek Kinect desteği (SDK 1.8 / libfreenect)
6. Native Xbox sürümü (UWP, Dev Mode, MediaCapture + DirectML)
7. Oyun konsolu sürümü: Raspberry Pi 5 + webcam ile TV'ye takılan, kendi başına çalışan Kinext konsolu
