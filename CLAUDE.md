# EHARPP GUI — Proje Bağlamı

TEKNOFEST 2026 Elektronik Harp Yarışması için Qt6/C++ operatör arayüzü
("Ebabil Teknoloji Takımı - Elektronik Harp Yer Kontrol İstasyonu"). ED
(Elektronik Destek) ve ET (Elektronik Taarruz) modları arasında geçiş yapan
tek pencerelik bir uygulama.

## Backend AYRI BİR REPO'DA

Bu GUI'nin bağlandığı Python backend'ler (`streamer.py`, `pluto_ed_scanner.py`,
`et_control.py`, `mavlink_bridge.py`) burada DEĞİL, ayrı bir repoda:
**https://github.com/Habibey/Ebabil_EH_AI** — orada da bir `CLAUDE.md` var,
tüm mimari/durum bilgisi orada.

## Haberleşme (ZeroMQ, hepsi loopback varsayılan)

- 5555/5556: streamer.py (RTL-SDR) SYS/SPEC + AI
- 5557: GUI'nin PUB olarak bağladığı komut kanalı (JAM_START/STOP,
  SDR_VERISI_ISTEK, HEDEF_SEC, DINLE_BASLAT/DURDUR, TARAMA_DURDUR/DEVAM,
  BANT_AYARLA, ET/ALDATMA komutları) -- her iki backend de buna SUB olur.
- 5559: mavlink_bridge.py (UAV telemetri)
- 5560/5561: pluto_ed_scanner.py SYS/SPEC + AI

Backend başka bir makinede ise `EBABIL_JETSON_IP=<ip>` ortam değişkeniyle
GUI oraya bağlanır (bkz. `setupZmqConnections()` içindeki yorum).

## ŞU AN NE YAPILIYOR: Windows'tan Ubuntu'ya taşıma

Bu proje az önce (bugün) git'e alındı ve bu GitHub reposuna push edildi --
öncesinde HİÇ versiyon kontrolü yoktu. Kullanıcı şu an aynı zamanda backend'i
de Ubuntu'ya (bedirhan-EXCALIBUR-G870) taşıyor.

`CMakeLists.txt` Linux'ta derlenebilsin diye düzeltildi:
- vcpkg/CONFIG tabanlı ZeroMQ/cppzmq bulma SADECE `if(WIN32)` içinde.
- Linux'ta `pkg_check_modules` ile `libzmq3-dev` (apt) üzerinden bulunuyor.
- cppzmq (`zmq.hpp`) Ubuntu'da apt paketi olarak YOKTU -- GitHub'dan
  (zeromq/cppzmq v4.11.0) `/usr/local/include`'a manuel indirildi.
- **GUI Ubuntu'da bu düzeltmelerle BAŞARIYLA DERLENDİ** (`cmake .. && cmake
  --build .`).

## SIRADAKİ ADIM

Kullanıcı backend'i (streamer.py/et_control.py) Ubuntu'da ilk kez çalıştırıp
bu GUI'yi (`./build/EHARPP`) açarak uçtan uca (BAĞLI durumu, hedef tespiti,
DİNLE, karıştırma) doğrulayacak -- bu Ubuntu'daki İLK gerçek entegrasyon
testi olacak, henüz yapılmadı.

## Önemli notlar

- `qcustomplot.cpp` çok büyük (~30k satır) -- MinGW/Windows'ta
  `-Wa,-mbig-obj` gerekiyordu (CMakeLists.txt'te zaten var), Linux/GCC'de
  bu sorun muhtemelen yaşanmaz ama derleme çok uzun sürebilir.
- `build/` ve `build_zmq/` klasörleri `.gitignore`'da -- Ubuntu'da temiz bir
  `build/` klasörüyle baştan derlemek gerekiyor (Windows'un build klasörünü
  taşımaya çalışma, MinGW'ye özel önbellek/yol içeriyor).
