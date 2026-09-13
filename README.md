# EHARPP - ZeroMQ Entegrasyonu

Bu proje artık 915 MHz seri port yerine ZeroMQ (loopback) üzerinden
`src/streamer.py` (radar/spektrum simülasyonu) ve `src/predict.py`
(RF modülasyon sınıflandırma modeli) ile konuşuyor. Bağlantı derlendi,
çalıştırıldı ve canlı veriyle uçtan uca doğrulandı (2026-09-01).

## Port şeması (hepsi 127.0.0.1 / loopback)

| Port | Yön                        | İçerik                                              |
|------|----------------------------|------------------------------------------------------|
| 5555 | streamer.py -> arayüz      | `SYS,...` ve `SPEC,...` satırları                    |
| 5556 | predict.py -> arayüz       | `AI,...` satırları                                   |
| 5557 | arayüz -> streamer/predict | `JAM_START\|f MHz`, `JAM_STOP`, `SDR_VERISI_ISTEK`   |

**Güvenlik notu:** Üç soket de sadece `127.0.0.1`'e bağlanıyor/bind ediyor --
dış ağa hiç açılmıyor, bu da tek makine üzerindeki bu trafik için yeterli
izolasyonu sağlıyor. Arayüz ve model ayrı makinelerde çalışacaksa (ağ
üzerinden), ZeroMQ'nun CURVE (public-key) şifrelemesi eklenmeli -- şu anki
haliyle bu senaryo için güvenli DEĞİL.

## Kurulum (bu makinede doğrulanmış adımlar)

### 1. vcpkg + zeromq/cppzmq (MinGW)

```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
# MinGW derleyicisi PATH'te olmalı (vcpkg'nin derleyiciyi bulabilmesi için):
set PATH=C:\Qt\Tools\mingw1310_64\bin;%PATH%
.\vcpkg.exe install zeromq cppzmq --triplet x64-mingw-dynamic
```

### 2. CMake configure + build

```
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build_zmq ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe" ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" ^
  -DCMAKE_TOOLCHAIN_FILE="<vcpkg-kök>/scripts/buildsystems/vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

"C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe" -C build_zmq -j4
```

`libzmq.dll` build klasörüne otomatik kopyalanıyor (vcpkg'nin CMake
entegrasyonu sayesinde) -- ayrıca bir DLL taşıma işlemi gerekmiyor.

`EHARPP.exe`'yi çalıştırmak için Qt'nin runtime DLL'lerinin (Qt6Widgets,
Qt6Core, Qt6Gui, Qt6PrintSupport, `platforms/qwindows.dll`) bulunabilmesi
için `C:\Qt\6.11.0\mingw_64\bin`'in PATH'te olması gerekiyor (Qt Creator'dan
çalıştırırken bunu kit otomatik ayarlıyor; komut satırından çalıştırırken
elle eklemek gerekir).

### 3. Bilinen derleyici sorunu: "too many sections"

`qcustomplot.cpp` (~30.000 satır) Debug modda MinGW/COFF'un section
sınırını aşıp `file too big` hatası veriyordu -- `CMakeLists.txt`'e
eklenen `-Wa,-mbig-obj` bayrağıyla çözüldü (bkz. `MINGW` bloğu).

## Qt Creator ile açma

Qt Creator'da projeyi açtığında CMake ayarlarına (Projects > Build >
CMake) yukarıdaki `-DCMAKE_TOOLCHAIN_FILE` ve `-DVCPKG_TARGET_TRIPLET`
değişkenlerini eklemen yeterli; geri kalanı (derleyici, Qt prefix) zaten
kit tarafından ayarlı.

## Çalıştırma

```
python src/streamer.py
python src/predict.py
```

ardından `EHARPP.exe`'yi başlat. `SYS`/`SPEC` paketleri hiçbir buton
gerekmeden otomatik akar (bağlantı durumu "● BAĞLI" olur, HEDEF-1 kartı
belirir). AI sınıflandırması için "DİNLE" butonuna basmak gerekiyor --
bu, `predict.py`'ye saniyede bir `SDR_VERISI_ISTEK` gönderilmesini başlatır.

## Bilinen eksikler / sonraki adımlar

- `UAV,...` satırları henüz hiçbir Python betiği tarafından gönderilmiyor
  -- İHA telemetri paneli bu paket gelene kadar boş kalır.
- Hedef konumu (lat/lon) şu an `nan` gönderiliyor (`streamer.py`) -- gerçek
  konum kestirimi eklenene kadar arayüzde "nan" görünür.
- `predict.py`'deki I/Q hâlâ tamamen simüle (rastgele) -- gerçek SDR/IQ
  kaynağı bağlanınca `iq_to_v5_features()` girişini o veriyle beslemek
  yeterli, model pipeline'ı (7 özellik + normalizasyon) zaten hazır.
