#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QFrame>
#include <QListWidget>
#include <QProgressBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QList>
#include <QElapsedTimer>
#include "qcustomplot.h"
#include "system.h"
#include "modeswitch.h"
#include "zmqsubscriber.h"
#include "zmqcommandpublisher.h"

class QGraphicsDropShadowEffect;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateClock();
    // Belirli bir süredir güncellenmeyen hedefleri "pasif" (soluk) göstermek
    // için clockTimer'a (1 sn) takılı periyodik kontrol.
    void hedefTazelikKontrolu();
    // "BAĞLI" göstergesi eskiden TEK YÖNLÜ idi -- handleZmqLine() ilk paket
    // geldiğinde bir kere yeşile çeviriyordu ama backend (streamer.py)
    // sonradan donsa/çökse bile hiç "BAĞLI DEĞİL"e dönmüyordu (sahada
    // kafa karıştırdı: GUI yeşil gösterirken backend arka planda donmuştu).
    // clockTimer'a (1 sn) takılı bu kontrol, son mesajdan bu yana
    // BAGLANTI_KOPMA_ESIK_MS'den uzun süre geçtiyse gerçek zamanlı olarak
    // "BAĞLI DEĞİL"e döndürür.
    void baglantiTazelikKontrolu();
    // ModeSwitch toggled(bool) sinyaline bağlı -- false=ED, true=ET.
    void modAktifDegisti(bool etModu);

    // --- Şartname madde 5.1/5.2 kapsamındaki eksik görevler için eklenen
    // kontroller. Donanım/algoritma tarafı henüz bağlı değil -- şimdilik
    // sadece arayüz durumunu değiştirip olay günlüğüne kaydediyorlar; gerçek
    // veri geldiğinde bu slotlar ilgili donanım çağrılarıyla genişletilecek.
    void dinlemeDurumuDegisti(bool aktif);
    void karistirmaBaslatDurdur(bool aktif);
    void aldatmaBaslatDurdur(bool aktif);
    void gnssAldatmaBaslatDurdur(bool aktif);

    // ZmqSubscriber::lineReceived sinyaline bagli -- gelen her satiri
    // parseLine()'a yonlendirmeden once ilk veri geldiginde baglanti
    // durumunu gunceller.
    void handleZmqLine(const QString &line);

private:
    // false=ED (açık mavi/lacivert), true=ET (koyu/turkuaz) -- tüm arayüzü
    // (central QSS, header bar, glow, şelale/spektrum/minimap) yeniden
    // renklendirir. Constructor sonunda başlangıç teması için de çağrılır.
    void temaUygula(bool etModu);

    // --- Panel kurucu fonksiyonlar ---
    QWidget* buildHeaderBar();
    QWidget* buildCenterPanel();
    QWidget* buildPositionPanel();
    QWidget* buildSignalPanel();
    // --- ET (Elektronik Taarruz) panelleri ---
    // İçerik (karıştırma gücü, bant seçimi vb.) henüz netleşmedi; şimdilik
    // ED'de tespit edilen hedefleri mirror'layan bir tablo + placeholder
    // gösterge kutuları. hedefGuncelle() ikisini birden günceller.
    QWidget* buildETHedefPanel();
    QWidget* buildETGucPanel();
    // GNSS servis toggle satırı (ör. "GPS" başlığı + L1/L2/L5 butonları).
    // Oluşturulan butonlar gnssServisButonlari listesine eklenir.
    QWidget* buildServisSatiri(const QString &baslik, const QStringList &servisler);
    // Bir hedef için dikey "alan: değer" kartı -- dar panelde çok sayıda
    // sütunun sığmama sorununu önlemek için QTableWidget yerine kullanılıyor.
    // alanAdlari sırasıyla değer etiketlerine karşılık gelir (degerLabellariOut
    // aynı sırayla doldurulur).
    QFrame* buildHedefKarti(const QString &id, const QColor &renk, const QStringList &alanAdlari,
                             QLabel **baslikLabelOut, QList<QLabel*> *degerLabellariOut);
    QCustomPlot* buildWaterfallPlot();
    // Şelalenin üstüne gqrx/SDR++ tarzı "Taban/Tepe (dB)" kontrast kontrolleri
    // + Oto-Kontrast onay kutusunu içeren küçük bir kontrol çubuğu.
    QWidget* buildWaterfallKontrastCubugu();
    QCustomPlot* buildSpectrumPlot();
    QCustomPlot* buildMiniMap();
    // sabitKoyu=true: kutu ED/ET temasından bağımsız, her zaman koyu/pastel
    // turkuaz kalır (orta panel/İHA telemetrisi için kullanılıyor).
    QFrame* createInfoBox(const QString &title, const QString &value, QLabel **valueRef = nullptr, bool sabitKoyu = false);
    // temaTakipEtsin=false: glow rengi temaUygula() tarafından güncellenmez.
    void applyGlow(QWidget *widget, bool temaTakipEtsin = true);
    void addLogEntry(const QString &text);
    void updateUavRotation();
    // Orijinal İHA şeması şeffaf/tek renkli bir çizim -- temaya göre farklı
    // renkte göstermek için alfa kanalını koruyarak yeniden renklendirir
    // (CompositionMode_SourceIn). ED'de koyu bir görsel açık zeminde
    // kaybolduğu için gerekli.
    QPixmap tintliPixmap(const QPixmap &kaynak, const QColor &renk) const;
    void setConnectionStatus(bool connected);
    // Seri porttan ZeroMQ'ya gecildi: streamer.py (5555: SYS/SPEC, 5556: AI)
    // ve mavlink_bridge.py (5559: UAV) SUB ile dinlenir, komutlar
    // (JAM_START/STOP, SDR_VERISI_ISTEK) 5557'de bind edilen PUB soketiyle
    // gonderilir. Tumu loopback (127.0.0.1) -- dis
    // aga hic acilmiyor, bu da tek makine ici bu trafik icin yeterli izolasyon.
    void setupZmqConnections();
    void parseLine(const QString &line);

    // --- Dinamik hedef yönetimi ---
    // Sabit 3 slot yerine, tespit edildikçe (id ilk kez görüldüğünde)
    // otomatik olarak yeni bir tablo satırı + mini harita noktası açan
    // model. id, kaynak tarafından (ör. bant adı) belirlenir ve GUI için
    // sadece bir anahtardır; aynı id tekrar geldiğinde var olan girdi
    // güncellenir, yenisi oluşturulmaz.
    struct HedefGirdisi {
        EWSystem veri;
        // ED kartı: {Durum, Frekans, Bant Gen., Güç, Analog/Sayısal,
        // Modülasyon, Enlem, Boylam, İrtifa} sırasıyla edDegerLabellari.
        QFrame *edKarti = nullptr;
        QLabel *edBaslikLabel = nullptr;
        QList<QLabel*> edDegerLabellari;
        // Operatör bu hedefi ET adayı yapmak için basar (bkz. hedefGuncelle
        // içindeki onay lambda'sı) -- onaylanmadan etKarti oluşturulmaz.
        QPushButton *onaylaButonu = nullptr;
        bool onaylandi = false;
        // ET kartı: {Frekans, Sinyal Gücü, Karıştırma Gücü, Durum} --
        // SADECE onaylandi=true olunca oluşturulur (bkz. hedefGuncelle).
        QFrame *etKarti = nullptr;
        QLabel *etBaslikLabel = nullptr;
        QList<QLabel*> etDegerLabellari;
        QCPGraph *haritaNoktasi = nullptr;
        qint64 sonGorulmeMs = 0;
        QColor renk;
    };
    // Onaylı ET kartlarından birine tıklanınca çağrılır -- etSeciliHedefId'yi
    // günceller (karıştır/aldat komutları bu hedefin frekansını kullanır),
    // önceki/yeni ET kartının kenarlık vurgusunu değiştirir.
    void etHedefSec(const QString &id);
    // ED kartlarından birine tıklanınca çağrılır -- backend'e (streamer.py
    // veya pluto_ed_scanner.py, id önekine göre) "artık en son bulunana değil
    // BUNA kilitlen" komutunu gönderir (bkz. HEDEF_SEC|/PLUTO_ED_HEDEF_SEC|,
    // src/streamer.py ve src/pluto_ed_scanner.py'deki "hedef <id>" komutunun
    // GUI eşdeğeri). Aynı karta tekrar tıklamak seçimi kaldırıp otomatik
    // (en son bulunan hedef) moduna döner.
    void edHedefSec(const QString &id);
    // ED ve ET kartlarındaki tıklamaları yakalamak için (installEventFilter
    // ile kartın kendisine takılıyor, property("etHedefId") taşıyan kartlar
    // etHedefSec'i tetikler).
    bool eventFilter(QObject *watched, QEvent *event) override;
    // freqMhz/powerDbm/bandwidthKHz: parametre çıkarımı algoritmasından (SYS paketi).
    // sapmaMhz/gurultuTabaniDb/snrDb/sureklilik: KTR Tablo 8'in geri kalan
    // parametreleri -- streamer.py/pluto_ed_scanner.py'nin OS-CFAR'ı zaten
    // hesaplıyordu, sadece dışarı hiç aktarılmıyordu (bkz. sdr_common.py).
    void hedefGuncelle(const QString &id, bool tespitEdildi, double lat, double lon, double alt,
                        double freqMhz, double powerDbm, double bandwidthKHz,
                        double sapmaMhz, double gurultuTabaniDb, double snrDb, const QString &sureklilik);
    // analogSayisal/modulasyonTuru: ayrı entegre edilecek yapay zeka
    // sınıflandırma modülünden (AI paketi) -- SYS'ten bağımsız, hedef daha
    // önce SYS ile oluşturulmuş olmalı.
    void hedefYapayZekaGuncelle(const QString &id, const QString &analogSayisal, const QString &modulasyonTuru);
    // yontemKodu, protokolde sabit tutulan bir koddur ("IHA_GENLIK" |
    // "YER_YAGI") -- görüntülenecek Türkçe metne çevirir. Bilinmeyen bir kod
    // gelirse (henüz eklenmemiş bir yöntem) olduğu gibi gösterilir.
    QString dfYontemMetni(const QString &yontemKodu);
    // Yön bulma + konum kestirimi sonucu (DF paketi) -- hem global "YÖN VE
    // KONUM BULMA" panelini (her zaman en son DF sonucu) hem ilgili hedefin
    // kartındaki Enlem/Boylam + harita noktasını günceller. lat/lon NaN
    // gelirse (henüz konum kestirimi yoksa, sadece açı varsa) konum alanları
    // değiştirilmez, önceki değer korunur.
    void dfSonucuGuncelle(const QString &hedefId, const QString &yontemKodu,
                           double aciDeg, double rmsDerece, double lat, double lon);
    QColor sonrakiRenk();
    // Şu an "tespit edildi" durumundaki tüm hedeflerin gerçek frekanslarını
    // tek bir şelale satırına ve anlık spektrum çizgisine birleştirip çizer.
    // ED/ET arasında ortak, moddan bağımsız.
    // Taban artık sabit -110 değil, spektrumZeminGucleri'nden (bkz. specGuncelle)
    // okunuyor -- böylece taranan frekanslardaki gerçek zemin/sinyal seviyesi
    // sürekli görünür, hedef tespiti sadece üstüne bindirilen bir katman olur.
    void spektrumVeSelaleGuncelle();
    // ebabil_sdr'dan gelen "SPEC" paketiyle (tam spektrum snapshot, tespit
    // eşiğinden bağımsız) çağrılır -- o paketin kapsadığı gerçek frekans
    // aralığındaki spektrumZeminGucleri hücrelerini günceller, sonra
    // spektrumVeSelaleGuncelle() ile waterfall/çizgiyi yeniden çizer. Bu
    // sayede waterfall, gqrx'teki gibi tarama ilerledikçe sürekli akar --
    // sadece eşiği geçen tespitlerde değil.
    void specGuncelle(double merkezMhz, double fsMhz, const QVector<double> &ornekler);
    // Görünüm penceresini (gorunumMerkezMhz/gorunumSpanMhz) istenen merkez/
    // genişliğe göre gerekiyorsa yeniden kurar (colorMap + eksen aralıkları +
    // geçmişi sıfırlar). Küçük dalgalanmalarda (span'ın %15'inden az kayma)
    // yeniden kurmaz -- her SPEC paketinde geçmişi silmemek için.
    void odaklanGerekirse(double merkezMhz, double spanMhz);

    QMap<QString, HedefGirdisi> hedefler;
    // Onaylı ET adayları arasından operatörün ET kartına tıklayarak seçtiği
    // TEK hedefin id'si -- karıştır/aldat komutları bu hedefin frekansını
    // kullanır. Boşsa henüz hiçbir hedef seçilmedi.
    QString etSeciliHedefId;
    // Operatörün ED kartına tıklayarak seçtiği hedefin id'si -- backend'in
    // (streamer.py/pluto_ed_scanner.py) izleme/dwell modunun "en son bulunan"
    // yerine BUNU takip etmesi için gönderilir (bkz. edHedefSec). Sadece
    // GÖRSEL vurgu ve komut gönderimi içindir; asıl "hangi hedefe kilitli"
    // durumu backend'de tutulur, burası sadece son tıklamayı hatırlar.
    QString edSeciliHedefId;
    // "ÇOKLU" karıştırma profili seçiliyken ET kartlarına tıklanarak
    // biriktirilen hedef listesi (en fazla 3, bkz. etHedefSec). TEKLİ/BARAJ
    // modunda kullanılmaz -- o modda etSeciliHedefId (tekil) geçerli.
    QStringList etCokluSeciliHedefler;
    // TEKLİ/BARAJ karıştırma için hiç hedef kartı seçilmediğinde kullanılan
    // manuel frekans -- RX/ED tarafı yokken (ör. sadece TX Pluto ile bench
    // testi) operatörün kart bekletmeden doğrudan frekans girip yayın
    // yapabilmesi için (bkz. karistirmaBaslatDurdur).
    class QDoubleSpinBox *etManuelFrekansSpin = nullptr;
    // Yeni kartlar, sondaki addStretch()'ten önce insertWidget ile eklenir.
    QVBoxLayout *hedefKartlariLayout;
    // Hiç hedef yokken kart alanının boş/bozuk görünmemesi için placeholder --
    // ilk hedef eklenince gizlenir (bkz. hedefGuncelle).
    QLabel *bosHedefMesaji;
    QLabel *aktifHedefSayisiLabel;
    QElapsedTimer calismaSuresi;
    int sonrakiRenkIndex = 0;

    // --- ED: Sinyal İzleme/Dinleme (madde 5.1.3) ---
    QPushButton *dinleButonu;
    QLabel *demodulasyonDurumuLabel;

    // --- ED: Tarama Bandı -- operatör arayüzden yeni bir bant girip
    // backend'e (streamer.py/pluto_ed_scanner.py, BANT_AYARLA| komutu)
    // süreci yeniden başlatmadan bildirir (madde 5.1.1: hakem bant açıklarsa).
    class QDoubleSpinBox *bantBaslangicSpin = nullptr;
    class QDoubleSpinBox *bantBitisSpin = nullptr;
    QPushButton *bantUygulaButonu = nullptr;
    // Sürekli tarama (her SPEC paketinde retune+capture+repaint) zayıf
    // sistemlerde arayüzü zorluyor -- operatör izlemeye ara vermek istediğinde
    // backend'e TARAMA_DURDUR/TARAMA_DEVAM gönderen aç/kapa düğmesi. DİNLE
    // aktifse etkilemiyor (bkz. streamer.py/pluto_ed_scanner.py main döngüsü).
    QPushButton *taramaDurdurButonu = nullptr;

    // --- ED: Yön ve Konum Bulma (madde 5.1.4 + 5.1.5) -- şimdilik
    // placeholder gösterge, gerçek DF/konum verisi geldiğinde ilgili paket
    // işleyicisi güncelleyecek.
    QLabel *yonAcisiLabel;
    QLabel *yonYontemiLabel;
    QLabel *rmsHataLabel;
    QLabel *konumEnlemLabel;
    QLabel *konumBoylamLabel;

    // --- Tema (ED=açık mavi/lacivert, ET=koyu/turkuaz) ---
    QWidget *central;
    QWidget *headerBar;
    QLabel *teamLabel;
    // applyGlow() ile oluşturulan tüm efektler burada tutulur ki
    // temaUygula() glow rengini sonradan canlı güncelleyebilsin.
    QList<QGraphicsDropShadowEffect*> glowEfektleri;

    // --- Header bar ---
    QLabel *clockLabel;
    QLabel *connectionStatusLabel;
    ModeSwitch *modeSwitch;
    QTimer *clockTimer;
    // Adi degisti (eskiden serialConnected): artik ZMQ uzerinden ilk veri
    // alindiginda true olur, baglanti durumu etiketini kontrol eder.
    bool mZmqConnected = false;
    // En son HERHANGİ bir ZMQ satırının (SYS/SPEC/AI) alındığı an --
    // baglantiTazelikKontrolu() bunu kullanıp backend gerçekten hâlâ veri
    // gönderiyor mu diye bakar (bkz. calismaSuresi -- aynı QElapsedTimer,
    // hedefTazelikKontrolu'nde kullanılanla tutarlı olsun diye).
    qint64 sonZmqMesajMs = 0;
    static const int BAGLANTI_KOPMA_ESIK_MS = 3000;

    QListWidget *eventLog;

    // --- ED/ET panel geçişi ---
    // index 0 = ED, index 1 = ET. Orta panel (İHA) sabit, sadece bu ikisi değişir.
    QStackedWidget *solPanelStack;
    QStackedWidget *sagPanelStack;

    // --- ET (Elektronik Taarruz) widget'ları -- placeholder içerik ---
    QVBoxLayout *etHedefKartlariLayout;
    QLabel *etBosHedefMesaji;
    QLabel *etAktifBantLabel;
    QLabel *etCikisGucuLabel;
    // Çıkış gücü kaydırıcı: -20..+30 dBm, 1 dBm adım -- et_kontrol'e
    // "SET_POWER <değer>" komutu olarak gönderilir (bkz. buildETGucPanel).
    QSlider *etGucSlider = nullptr;
    QSpinBox *etGucSpinBox = nullptr;
    // Şu an aktif karıştırma görev kodu ("SUREKLI_KARISTIRMA" |
    // "ARA_BAKISLI_KARISTIRMA") -- DURDUR komutunda hangi görevin
    // durdurulacağını et_kontrol'e bildirmek için BASLAT anında kaydedilir.
    QString etAktifKaristirmaGorevi;

    // --- ET: Sürekli Karıştırma (5.2.1) + Arabakışlı Karıştırma (5.2.2) ---
    QPushButton *tekliButonu;
    QPushButton *cokluButonu;
    QPushButton *barajButonu;
    QButtonGroup *karistirmaTipiGrubu;
    QPushButton *karistirmaBaslatButonu;
    QPushButton *surekliButonu;
    QPushButton *arabakisliButonu;
    QButtonGroup *karistirmaModuGrubu;

    // --- ET: Analog Telsiz Aldatma (5.2.3) ---
    QPushButton *aldatmaBaslatButonu;
    QLabel *aldatmaDurumLabel;
    // Mesaj kaynağı seçimi: KAYIT-TEKRAR (data/aldatma_sesleri/'den bir .wav)
    // veya PİPER TTS (girilen metni yerel/çevrimdışı Türkçe TTS ile seslendirir).
    // Seçim, ALDATMAYI BAŞLAT'a basılınca et_control.py'ye ALDATMA_KAYNAK|
    // komutuyla iletilir (bkz. aldatmaBaslatDurdur).
    QPushButton *aldatmaKayitTekrarButonu;
    QPushButton *aldatmaPiperTtsButonu;
    QButtonGroup *aldatmaKaynakGrubu;
    QComboBox *aldatmaDosyaSecimi;   // KAYIT-TEKRAR seçiliyken görünür
    QLineEdit *aldatmaMetinKutusu;   // PİPER TTS seçiliyken görünür

    // --- ET: GNSS Aldatma (5.2.4) ---
    QList<QPushButton*> gnssServisButonlari;
    QPushButton *gnssBaslatButonu;

    // --- İHA telemetri widget'ları ---
    QLabel *speedLabel;
    QLabel *headingLabel;
    QLabel *altitudeLabel;
    QLabel *batteryLabel;
    QLabel *pitchLabel;
    QLabel *rollLabel;
    QLabel *uavLabel;
    QPixmap uavPixmapOriginal;
    // temaUygula() tarafından güncellenir: ED'de turkuaz, ET'de kırmızı.
    QColor uavTintRengi = QColor("#00ffcc");

    // --- Şelale grafiği + spektrum çizgisi (alt satır, ED/ET ortak) ---
    QCustomPlot *waterfallPlot;
    QCPColorMap *colorMap;
    QCustomPlot *spektrumPlot;
    QCPGraph *spektrumGrafik;
    QVector<double> spektrumFrekanslar; // her bin'in merkez frekansı (MHz), o anki görünüm penceresine göre
    // Sürekli güncellenen "gerçek zemin/spektrum" -- SPEC paketleriyle
    // taranan dilimler doldukça güncellenir, hiç taranmamış bin'ler
    // başlangıç değeri -110'da kalır. spektrumVeSelaleGuncelle()'nin taban
    // katmanı budur (bkz. specGuncelle).
    QVector<double> spektrumZeminGucleri;
    // Şelale/spektrum artık sabit 100-2600 MHz aralığını göstermiyor --
    // gqrx tarzı, o an aktif hedefin (veya hedef yoksa taramanın bulunduğu
    // bandın) etrafında dar/yakınlaştırılmış bir pencere gösteriyor. Bu ikisi
    // o pencerenin merkezini/genişliğini (MHz) tutar; 0 ise henüz kurulmadı.
    double gorunumMerkezMhz = 0.0;
    double gorunumSpanMhz = 0.0;
    // Spektrum ÇİZGİSİ için Exponential Moving Average (EMA/"video averaging")
    // -- gqrx/SDR++'taki gibi çizgi anlık titremek yerine yumuşak akar.
    // Şelale hücreleri (colorMap) bundan ETKİLENMEZ, ham/anlık değeri gösterir.
    QVector<double> spektrumEma;
    static constexpr double SPEKTRUM_EMA_ALPHA = 0.35; // 0=hiç değişmez, 1=EMA kapalı (ham veri)
    // gqrx/SDR++ tarzı kontrast kontrolleri (bkz. buildWaterfallKontrastCubugu).
    // Oto işaretliyse her satırın kendi min/max'ına göre otomatik ayarlanır
    // (bkz. spektrumVeSelaleGuncelle); değilse kullanıcının Taban/Tepe (dB)
    // spin box değerleri doğrudan colorMap dataRange olarak kullanılır.
    class QDoubleSpinBox *waterfallTabanSpin = nullptr;
    class QDoubleSpinBox *waterfallTepeSpin = nullptr;
    class QCheckBox *waterfallOtoKontrastCheck = nullptr;

    // --- Mini harita ---
    QCustomPlot *miniMapPlot;
    QCPGraph *uavMapPoint;

    // --- ZeroMQ ---
    ZmqSubscriber *mZmqSubscriber = nullptr;
    ZmqCommandPublisher *mZmqPublisher = nullptr;
    // DINLE aktifken predict.py'den periyodik AI siniflandirmasi istemek
    // icin (SDR_VERISI_ISTEK).
    QTimer *mAiRequestTimer = nullptr;

    // --- Veri ---
    UAVTelemetry uavData;
    int waterfallRow = 0;
    // replot() CPU açısından pahalı (waterfall + spektrum çizgisi ikisi
    // birden) -- ebabil_sdr saniyede onlarca SPEC paketi gönderebiliyor,
    // her pakette replot() çağırmak sistem yükü altında arayüzü tıkayıp
    // "donmuş" hissi veriyordu. Veri (colorMap hücresi, EMA, waterfallRow)
    // HER pakette güncellenmeye devam eder -- sadece PAHALI repaint çağrısı
    // aşağıdaki REPLOT_MIN_ARALIK_MS'den sık yapılmaz (bkz. spektrumVeSelaleGuncelle).
    qint64 sonReplotMs = 0;
    static constexpr qint64 REPLOT_MIN_ARALIK_MS = 40; // ~25 fps üst sınır

    static const int WATERFALL_HISTORY = 100;
    // ebabil_sdr'ın SdrIslemci::downsample64() ile gönderdiği nokta sayısıyla
    // (bkz. TelemetriGonderici::gonderSpec) birebir eşleşiyor -- her SPEC
    // örneği kendi görünüm penceresinde tam olarak bir sütuna denk gelir,
    // ek bir toplulaştırma/genişletme yapılmaz.
    static const int FREQ_BINS = 64;
    // Bu süreden (ms) uzun süredir güncellenmeyen hedef "pasif" (soluk)
    // gösterilir -- veri hâlâ ekranda durur, sadece görünüşü değişir.
    static const int HEDEF_PASIF_ESIK_MS = 8000;
};

#endif // MAINWINDOW_H
