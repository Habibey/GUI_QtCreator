#include "mainwindow.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QDateTime>
#include <QTime>
#include <QGraphicsDropShadowEffect>
#include <QTransform>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QDir>

// ================= CONSTRUCTOR =================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Ebabil Teknoloji Takımı - Elektronik Harp Yer Kontrol İstasyonu");
    resize(1600, 950);

    calismaSuresi.start();

    central = new QWidget(this);
    setCentralWidget(central);
    // Central QSS, header bar, glow ve grafik renkleri constructor'ın
    // sonunda temaUygula(false) ile (başlangıç modu ED) tek seferde kurulur.

    // --- Dış dikey layout: Header + Ana grid ---
    QVBoxLayout *outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    outerLayout->addWidget(buildHeaderBar());
    connect(modeSwitch, &QAbstractButton::toggled, this, &MainWindow::modAktifDegisti);

    QWidget *bodyWidget = new QWidget();
    QGridLayout *mainGrid = new QGridLayout(bodyWidget);
    mainGrid->setSpacing(10);
    mainGrid->setContentsMargins(10, 10, 10, 10);

    // --- Sol/sağ paneller: ED (index 0) ve ET (index 1) arasında ModeSwitch
    // ile geçilir. Orta panel (İHA) her iki modda da aynı kalır.
    solPanelStack = new QStackedWidget();
    solPanelStack->addWidget(buildPositionPanel()); // 0: ED
    solPanelStack->addWidget(buildETHedefPanel());  // 1: ET

    sagPanelStack = new QStackedWidget();
    sagPanelStack->addWidget(buildSignalPanel());   // 0: ED
    sagPanelStack->addWidget(buildETGucPanel());    // 1: ET

    mainGrid->addWidget(solPanelStack,        0, 0, 1, 1);
    mainGrid->addWidget(buildCenterPanel(),   0, 1, 1, 2);
    mainGrid->addWidget(sagPanelStack,        0, 3, 1, 1);

    // --- Alt satır: şelale (sol) + anlık spektrum çizgisi (sağ), ED/ET ortak ---
    QWidget *altSatir = new QWidget();
    QHBoxLayout *altLayout = new QHBoxLayout(altSatir);
    altLayout->setContentsMargins(0, 0, 0, 0);
    altLayout->setSpacing(10);

    waterfallPlot = buildWaterfallPlot();
    spektrumPlot = buildSpectrumPlot();

    QWidget *waterfallSutunu = new QWidget();
    QVBoxLayout *waterfallSutunLayout = new QVBoxLayout(waterfallSutunu);
    waterfallSutunLayout->setContentsMargins(0, 0, 0, 0);
    waterfallSutunLayout->setSpacing(4);
    waterfallSutunLayout->addWidget(buildWaterfallKontrastCubugu());
    waterfallSutunLayout->addWidget(waterfallPlot, 1);

    altLayout->addWidget(waterfallSutunu, 3);
    altLayout->addWidget(spektrumPlot, 2);

    mainGrid->addWidget(altSatir, 1, 0, 1, 4);

    mainGrid->setRowStretch(0, 2);
    mainGrid->setRowStretch(1, 1);

    outerLayout->addWidget(bodyWidget);

    // --- Saat + hedef tazelik zamanlayıcı ---
    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::hedefTazelikKontrolu);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::baglantiTazelikKontrolu);
    clockTimer->start(1000);
    updateClock();

    // Başlangıç modu ED (ModeSwitch varsayılan olarak işaretsiz/false) --
    // central QSS, header bar, glow ve grafik renklerini burada tek seferde kuruyoruz.
    temaUygula(false);

    setupZmqConnections();
}

MainWindow::~MainWindow()
{
    if (mZmqSubscriber)
        mZmqSubscriber->stop(); // thread'i silmeden once durdurmak zorunlu (QThread yikicisi calisirken assert atar)
}

// ================= ÜST DURUM ÇUBUĞU =================
QWidget* MainWindow::buildHeaderBar()
{
    headerBar = new QWidget();
    headerBar->setFixedHeight(45);
    // Renk/kenarlık temaUygula() tarafından kurulur.

    QHBoxLayout *layout = new QHBoxLayout(headerBar);
    layout->setContentsMargins(15, 0, 15, 0);

    teamLabel = new QLabel("EBABİL TEKNOLOJİ TAKIMI");

    // ED/ET geçiş anahtarı -- bağlanması constructor'da yapılır
    // (connect(modeSwitch, ..., &MainWindow::modAktifDegisti)).
    modeSwitch = new ModeSwitch();
    modeSwitch->setToolTip("ED / ET modu değiştir");

    clockLabel = new QLabel();

    connectionStatusLabel = new QLabel("● BAĞLI DEĞİL");
    connectionStatusLabel->setStyleSheet("color: #ff4444; font-weight: bold; font-size: 13px;");

    layout->addWidget(teamLabel);
    layout->addStretch();
    layout->addWidget(modeSwitch);
    layout->addStretch();
    layout->addWidget(clockLabel);
    layout->addStretch();
    layout->addWidget(connectionStatusLabel);

    return headerBar;
}

// ================= ED/ET MOD GEÇİŞİ =================
void MainWindow::modAktifDegisti(bool etModu)
{
    solPanelStack->setCurrentIndex(etModu ? 1 : 0);
    sagPanelStack->setCurrentIndex(etModu ? 1 : 0);
    temaUygula(etModu);
    addLogEntry(etModu ? "ET (Elektronik Taarruz) moduna geçildi"
                        : "ED (Elektronik Destek) moduna geçildi");
}

// ================= SİNYAL İZLEME/DİNLEME (madde 5.1.3) =================
void MainWindow::dinlemeDurumuDegisti(bool aktif)
{
    if (aktif && edSeciliHedefId.isEmpty()) {
        addLogEntry("[ED] Dinlemek için önce bir hedef kartına tıklayın.");
        dinleButonu->blockSignals(true);
        dinleButonu->setChecked(false);
        dinleButonu->blockSignals(false);
        return;
    }

    dinleButonu->setText(aktif ? "DURDUR" : "DİNLE");
    addLogEntry(aktif ? "Sinyal dinleme başlatıldı" : "Sinyal dinleme durduruldu");

    // Gerçek AM/FM demodülasyonu artık streamer.py'de (bkz. DinlemeOturumu) --
    // DINLE_BASLAT/DINLE_DURDUR komutlarıyla tetiklenir. "Demodülasyon: AKTİF"
    // yazısı burada iyimser/yerel; gerçek teyit DURUM,DINLEME_AKTIF paketiyle
    // gelir (bkz. parseLine).
    if (aktif) {
        if (mZmqPublisher)
            mZmqPublisher->sendLine(QString("DINLE_BASLAT|%1").arg(edSeciliHedefId));
        demodulasyonDurumuLabel->setText("Demodülasyon: başlatılıyor...");
        if (mAiRequestTimer)
            mAiRequestTimer->start();
    } else {
        if (mZmqPublisher)
            mZmqPublisher->sendLine("DINLE_DURDUR");
        demodulasyonDurumuLabel->setText("Demodülasyon: --");
        if (mAiRequestTimer)
            mAiRequestTimer->stop();
    }
}

// ================= SÜREKLİ/ARABAKIŞLI KARIŞTIRMA (madde 5.2.1/5.2.2) =================
// Komut formatı, takım arkadaşımızın et_kontrol (Desktop/ET/) sunucusuyla
// aynı: "ET,BASLAT,<görev_kodu>,<frekans_mhz>" / "ET,DURDUR,<görev_kodu>" --
// böylece et_kontrol'e (veya onun bir ZMQ köprüsüne) bağlandığımızda satır
// formatı değişmeden çalışır. Şu an mZmqPublisher üzerinden 5557'ye
// gönderiliyor, gerçek bir verici henüz dinlemiyor (bkz. streamer.py).
void MainWindow::karistirmaBaslatDurdur(bool aktif)
{
    const QString tip = (karistirmaTipiGrubu && karistirmaTipiGrubu->checkedButton())
                             ? karistirmaTipiGrubu->checkedButton()->text() : "TEKLİ";
    const bool coklu = (tip == "ÇOKLU");

    if (aktif) {
        if (coklu && etCokluSeciliHedefler.isEmpty()) {
            addLogEntry("[ET] Çoklu karıştırma için önce 1-3 hedef kartına tıklayın (ET'ye onaylanmış olmalı).");
            karistirmaBaslatButonu->blockSignals(true);
            karistirmaBaslatButonu->setChecked(false);
            karistirmaBaslatButonu->blockSignals(false);
            return;
        }
        // TEKLİ/BARAJ artık hedef kartı ZORUNLU değil -- kart seçiliyse onun
        // frekansı kullanılır, seçili değilse (ör. RX/ED yokken, sadece TX
        // Pluto ile bench testi) etManuelFrekansSpin'deki değer kullanılır.
    }

    karistirmaBaslatButonu->setText(aktif ? "KARIŞTIRMAYI DURDUR" : "KARIŞTIRMAYI BAŞLAT");
    if (aktif) {
        const QString mod = karistirmaModuGrubu->checkedButton() ? karistirmaModuGrubu->checkedButton()->text() : "-";
        etAktifKaristirmaGorevi = (mod == "ARABAKIŞLI") ? "ARA_BAKISLI_KARISTIRMA" : "SUREKLI_KARISTIRMA";

        // Tip -> et_control.py'nin beklediği ASCII kod (Türkçe karakter yok).
        QString tipKodu = "TEKLI";
        if (tip == "ÇOKLU") tipKodu = "COKLU";
        else if (tip == "BARAJ") tipKodu = "BARAJ";

        QString freqField;
        QString bantMetni;
        QString hedefMetni;
        if (coklu) {
            QStringList freqStrs;
            for (const QString &tid : etCokluSeciliHedefler) {
                if (!hedefler.contains(tid)) continue;
                freqStrs << QString::number(hedefler[tid].veri.frequency, 'f', 6);
            }
            freqField = freqStrs.join(";");
            bantMetni = QString("%1 hedef").arg(etCokluSeciliHedefler.size());
            hedefMetni = etCokluSeciliHedefler.join(", ");
        } else {
            double freqMhz;
            if (!etSeciliHedefId.isEmpty() && hedefler.contains(etSeciliHedefId)) {
                freqMhz = hedefler[etSeciliHedefId].veri.frequency;
                hedefMetni = etSeciliHedefId;
            } else {
                freqMhz = etManuelFrekansSpin->value();
                hedefMetni = QString("Manuel %1 MHz").arg(freqMhz, 0, 'f', 3);
            }
            freqField = QString::number(freqMhz, 'f', 6);
            bantMetni = QString::number(freqMhz, 'f', 3) + " MHz";
        }

        if (mZmqPublisher)
            mZmqPublisher->sendLine(QString("ET,BASLAT,%1,%2,%3").arg(etAktifKaristirmaGorevi, freqField, tipKodu));
        etAktifBantLabel->setText(bantMetni);
        addLogEntry(QString("Karıştırma başlatıldı: %1 (%2/%3, %4)").arg(hedefMetni, mod, tip, bantMetni));
    } else {
        if (mZmqPublisher)
            mZmqPublisher->sendLine(QString("ET,DURDUR,%1").arg(etAktifKaristirmaGorevi));
        etAktifBantLabel->setText("--");
        addLogEntry("Karıştırma durduruldu");
    }
}

// ================= ANALOG TELSİZ ALDATMA (madde 5.2.3) =================
void MainWindow::aldatmaBaslatDurdur(bool aktif)
{
    if (aktif && (etSeciliHedefId.isEmpty() || !hedefler.contains(etSeciliHedefId))) {
        addLogEntry("[ET] Aldatma için önce bir hedef kartına tıklayın (ET'ye onaylanmış olmalı).");
        aldatmaBaslatButonu->blockSignals(true);
        aldatmaBaslatButonu->setChecked(false);
        aldatmaBaslatButonu->blockSignals(false);
        return;
    }

    aldatmaBaslatButonu->setText(aktif ? "ALDATMAYI DURDUR" : "ALDATMAYI BAŞLAT");
    aldatmaDurumLabel->setText(aktif ? "Durum: AKTİF" : "Durum: --");
    if (aktif) {
        const double freqMhz = hedefler[etSeciliHedefId].veri.frequency;

        // Önce mesaj kaynağını et_control.py'ye bildir (BAŞLAT'tan HEMEN
        // önce -- handle_baslat çağrıldığında en son ayarlanan kaynağı okur).
        const bool piperSecili = aldatmaPiperTtsButonu->isChecked();
        QString kaynakOzet;
        if (piperSecili) {
            const QString metin = aldatmaMetinKutusu->text().trimmed();
            if (metin.isEmpty()) {
                addLogEntry("[ET] Piper TTS için önce seslendirilecek metni yazın.");
                aldatmaBaslatButonu->blockSignals(true);
                aldatmaBaslatButonu->setChecked(false);
                aldatmaBaslatButonu->blockSignals(false);
                return;
            }
            if (mZmqPublisher)
                mZmqPublisher->sendLine(QString("ALDATMA_KAYNAK|PIPER_TTS|%1").arg(metin));
            kaynakOzet = QString("Piper TTS: \"%1\"").arg(metin);
        } else {
            const QString dosya = aldatmaDosyaSecimi->isEnabled() ? aldatmaDosyaSecimi->currentText() : QString();
            if (dosya.isEmpty()) {
                addLogEntry("[ET] Kayıt-tekrar için data/aldatma_sesleri/ altında en az bir .wav olmalı "
                            "(yoksa sentetik uyarı tonu kullanılır).");
            }
            if (mZmqPublisher)
                mZmqPublisher->sendLine(QString("ALDATMA_KAYNAK|KAYIT_TEKRAR|%1").arg(dosya));
            kaynakOzet = dosya.isEmpty() ? "Kayıt-tekrar (dosya yok, sentetik ton)" : QString("Kayıt-tekrar: %1").arg(dosya);
        }

        // Protokol tekli/coklu/baraj icin 5 alana cikti (bkz. karistirmaBaslatDurdur) --
        // aldatmanin profil kavrami yok, hep TEKLI gonderiyoruz ki et_control.py'nin
        // ayristiricisi (tam 5 alan bekliyor) reddetmesin.
        if (mZmqPublisher)
            mZmqPublisher->sendLine(QString("ET,BASLAT,ANALOG_TELSIZ_ALDATMA,%1,TEKLI").arg(freqMhz, 0, 'f', 6));
        addLogEntry(QString("Analog telsiz aldatma başlatıldı: %1 (%2 MHz, %3)")
                        .arg(etSeciliHedefId, QString::number(freqMhz, 'f', 3) + " MHz", kaynakOzet));
    } else {
        if (mZmqPublisher)
            mZmqPublisher->sendLine("ET,DURDUR,ANALOG_TELSIZ_ALDATMA");
        addLogEntry("Analog telsiz aldatma durduruldu");
    }
}

// ================= GNSS ALDATMA (madde 5.2.4) =================
void MainWindow::gnssAldatmaBaslatDurdur(bool aktif)
{
    gnssBaslatButonu->setText(aktif ? "GNSS ALDATMAYI DURDUR" : "GNSS ALDATMAYI BAŞLAT");
    if (aktif) {
        QStringList secili;
        for (QPushButton *btn : gnssServisButonlari) {
            if (btn->isChecked()) {
                secili << btn->property("servisAdi").toString();
            }
        }
        addLogEntry(secili.isEmpty() ? "GNSS aldatma başlatıldı (servis seçilmedi)"
                                      : QString("GNSS aldatma başlatıldı: %1").arg(secili.join(", ")));
    } else {
        addLogEntry("GNSS aldatma durduruldu");
    }
}

// ================= TEMA =================
// ET: orijinal koyu/turkuaz tema. ED: açık mavi (#abedfc benzeri) zemin +
// lacivert yazı/kenarlıklar. Şelale, spektrum ve mini harita da (ED/ET
// arasında paylaşılan widget'lar olsalar da) moda göre yeniden boyanır.
void MainWindow::temaUygula(bool etModu)
{
    // Işık temalarıyla uğraşmak yerine köke dönüldü: ikisi de aynı orijinal
    // koyu tema (#0b0f19, dokunulmamış hali) -- tek fark aksan rengi.
    // ED = orijinal turkuaz (#00ffcc, hiç değiştirmediğimiz ilk hali),
    // ET = aynı temanın kırmızı varyantı.
    const QString anaArkaplan  = "#0b0f19";
    const QString anaMetin     = etModu ? "#ff4444" : "#00ffcc";
    const QString baslikRengi  = etModu ? "#e08a8a" : "#6fe0d0";
    const QString cerceveRengi = "#1e3a4a";
    const QString panelBg      = "rgba(10,14,25,255)";
    const QString infoBoxBg    = "rgba(15,20,35,220)";
    const QString headerBg     = "#12182a";
    const QColor  glowRengi    = etModu ? QColor("#ff4444") : QColor("#00ffcc");

    central->setStyleSheet(QString(
        "QWidget { background-color: %1; color: %2; font-family: 'Consolas'; }"
        "QLabel#infoTitle { color: %3; font-size: 10px; font-weight: bold; }"
        "QLabel#infoValue { color: %2; font-size: 15px; font-weight: bold; }"
        "QFrame#infoBox { background-color: %4; border: 1px solid %2; border-radius: 6px; }"
        "QFrame#panelFrame { background-color: %5; border: 1px solid %6; border-radius: 8px; }"
        "QListWidget { background-color: %1; color: %3; border: none; font-size: 10px; }"
        "QProgressBar { background: %1; border: 1px solid %6; border-radius: 3px; height: 8px; }"
        "QProgressBar::chunk { background-color: %2; border-radius: 3px; }"
        "QPushButton { background-color: %4; color: %2; border: 1px solid %2; border-radius: 4px; font-size: 11px; font-weight: bold; }"
        "QPushButton:checked { background-color: %2; color: %1; }"
        "QPushButton:hover { border: 1px solid %3; }"
        ).arg(anaArkaplan, anaMetin, baslikRengi, infoBoxBg, panelBg, cerceveRengi, headerBg));

    headerBar->setStyleSheet(QString("background-color: %1; border-bottom: 2px solid %2;").arg(headerBg, anaMetin));
    teamLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 15px;").arg(anaMetin));
    clockLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(baslikRengi));

    // "Aktif Hedef: N" sayacı objectName tabanlı QSS kapsamı dışında, ayrı
    // bir inline stil kullanıyor -- o da aksan rengini takip etmeli.
    aktifHedefSayisiLabel->setStyleSheet(QString("color:%1; font-weight:bold; font-size:16px;").arg(anaMetin));

    // Glow rengini güncelle -- mevcut tüm info kutularının parlaması canlı değişir.
    for (QGraphicsDropShadowEffect *glow : glowEfektleri) {
        glow->setColor(glowRengi);
    }

    // İHA şeması artık aksan rengini takip ediyor -- ED'de turkuaz, ET'de kırmızı.
    uavTintRengi = QColor(anaMetin);
    updateUavRotation();

    // Şelale + spektrum + mini harita -- ED/ET ortak widget'lar, moda göre yeniden renklendirilir.
    const QColor plotBg      = QColor("#0b0f19");
    const QColor plotEksen   = QColor("#1e3a4a");
    const QColor plotEtiket  = etModu ? QColor("#e08a8a") : QColor("#6fe0d0");
    const QColor cizgiRengi  = etModu ? QColor("#ff4444") : QColor("#00ffcc");

    for (QCustomPlot *plot : {waterfallPlot, spektrumPlot, miniMapPlot}) {
        plot->setBackground(plotBg);
        plot->xAxis->setLabelColor(plotEtiket);
        plot->xAxis->setTickLabelColor(plotEtiket);
        plot->xAxis->setBasePen(QPen(plotEksen));
        plot->xAxis->grid()->setPen(QPen(plotEksen, 1, Qt::DotLine));
        plot->yAxis->setLabelColor(plotEtiket);
        plot->yAxis->setTickLabelColor(plotEtiket);
        plot->yAxis->setBasePen(QPen(plotEksen));
        plot->yAxis->grid()->setPen(QPen(plotEksen, 1, Qt::DotLine));
        plot->replot();
    }
    spektrumGrafik->setPen(QPen(cizgiRengi, 1.5));
    spektrumPlot->replot();
}

void MainWindow::updateClock()
{
    clockLabel->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy   HH:mm:ss"));
}

void MainWindow::setConnectionStatus(bool connected)
{
    mZmqConnected = connected;
    if (connected) {
        connectionStatusLabel->setText("● BAĞLI");
        connectionStatusLabel->setStyleSheet("color: #00ff66; font-weight: bold; font-size: 13px;");
    } else {
        connectionStatusLabel->setText("● BAĞLI DEĞİL");
        connectionStatusLabel->setStyleSheet("color: #ff4444; font-weight: bold; font-size: 13px;");
    }
}

// ================= GLOW EFEKTİ =================
void MainWindow::applyGlow(QWidget *widget, bool temaTakipEtsin)
{
    QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect();
    glow->setColor(QColor("#4fd6c0"));
    glow->setBlurRadius(18);
    glow->setOffset(0, 0);
    widget->setGraphicsEffect(glow);
    if (temaTakipEtsin) {
        glowEfektleri.append(glow);
    }
}

// ================= YARDIMCI KUTU FONKSİYONU =================
QFrame* MainWindow::createInfoBox(const QString &title, const QString &value, QLabel **valueRef, bool sabitKoyu)
{
    QFrame *box = new QFrame();
    box->setObjectName("infoBox");
    box->setFixedSize(120, 55);

    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    QLabel *titleLbl = new QLabel(title);
    titleLbl->setObjectName("infoTitle");

    QLabel *valueLbl = new QLabel(value);
    valueLbl->setObjectName("infoValue");

    layout->addWidget(titleLbl);
    layout->addWidget(valueLbl);

    if (valueRef) {
        *valueRef = valueLbl;
    }

    if (sabitKoyu) {
        // Widget'ın kendi stylesheet'i, central'dan miras alınan (temaya
        // bağlı) renklerin önüne geçer -- bu kutu hep koyu/pastel turkuaz kalır.
        box->setStyleSheet("background-color: rgba(15,20,35,220); border: 1px solid #4fd6c0; border-radius: 6px;");
        titleLbl->setStyleSheet("color: #7fb0a8; font-size: 10px; font-weight: bold;");
        valueLbl->setStyleSheet("color: #4fd6c0; font-size: 15px; font-weight: bold;");
        applyGlow(box, false);
    } else {
        applyGlow(box);
    }
    return box;
}

// ================= HEDEF KARTI (yardımcı) =================
// Sütun bazlı tabloların yerine: her hedef için dikey "alan: değer" listesi.
// Panel dar olduğunda çok sayıda alan (Bant Genişliği, Analog/Sayısal,
// Modülasyon vb.) yatay sütunlara sığmıyordu -- bu kart yapısı alan sayısı
// arttıkça sadece boyca uzar, metin kesilmesi yaşanmaz.
QFrame* MainWindow::buildHedefKarti(const QString &id, const QColor &renk, const QStringList &alanAdlari,
                                     QLabel **baslikLabelOut, QList<QLabel*> *degerLabellariOut)
{
    QFrame *kart = new QFrame();
    kart->setObjectName("infoBox");

    QVBoxLayout *kartLayout = new QVBoxLayout(kart);
    kartLayout->setContentsMargins(8, 6, 8, 6);
    kartLayout->setSpacing(3);

    QLabel *baslik = new QLabel(id);
    baslik->setStyleSheet(QString("font-weight: bold; font-size: 12px; color: %1;").arg(renk.name()));
    kartLayout->addWidget(baslik);
    if (baslikLabelOut) {
        *baslikLabelOut = baslik;
    }

    for (const QString &alan : alanAdlari) {
        QHBoxLayout *satir = new QHBoxLayout();
        satir->setSpacing(6);

        QLabel *alanLbl = new QLabel(alan);
        alanLbl->setObjectName("infoTitle");

        QLabel *degerLbl = new QLabel("-");
        degerLbl->setObjectName("infoValue");
        degerLbl->setStyleSheet("font-size: 12px;"); // infoValue varsayılanı (15px) kart için büyük kalıyor
        degerLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        satir->addWidget(alanLbl);
        satir->addStretch();
        satir->addWidget(degerLbl);
        kartLayout->addLayout(satir);

        if (degerLabellariOut) {
            degerLabellariOut->append(degerLbl);
        }
    }

    applyGlow(kart);
    return kart;
}

// ================= OLAY GÜNLÜĞÜ =================
void MainWindow::addLogEntry(const QString &text)
{
    QString entry = QString("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), text);
    eventLog->addItem(entry);
    if (eventLog->count() > 50) {
        delete eventLog->takeItem(0);
    }
    eventLog->scrollToBottom();
}

// ================= ORTA PANEL: İHA + Aktif Hedef Sayacı + Telemetri =================
QWidget* MainWindow::buildCenterPanel()
{
    QFrame *panel = new QFrame();
    panel->setObjectName("panelFrame");

    QGridLayout *grid = new QGridLayout(panel);
    grid->setSpacing(15);

    uavLabel = new QLabel();
    uavLabel->setObjectName("uavLabel");
    uavLabel->setStyleSheet("background: transparent;"); // "görsel bulunamadı" yedek metni central QSS'den renk alır
    uavLabel->setAlignment(Qt::AlignCenter);

    uavPixmapOriginal = QPixmap(":/images/uav_schema.png");
    if (!uavPixmapOriginal.isNull()) {
        uavLabel->setPixmap(uavPixmapOriginal.scaled(400, 230, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        uavLabel->setText("[İHA görseli bulunamadı]");
    }

    // Üst sıra: kaç farklı sinyal/hedef şu an ekranda diye canlı sayaç.
    // Sabit sayıda "sistem" kutusu yerine bunu kullanıyoruz çünkü sahada
    // kaç farklı yayın olacağı önceden bilinmiyor -- her yeni tespit sol
    // paneldeki tabloya kendi satırını ekliyor (bkz. hedefGuncelle).
    aktifHedefSayisiLabel = new QLabel("Aktif Hedef: 0");
    // Renk temaUygula() tarafından kurulur (aksan rengini takip eder).
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->addStretch();
    topRow->addWidget(aktifHedefSayisiLabel);
    topRow->addStretch();
    grid->addLayout(topRow, 0, 0, 1, 4);

    grid->addWidget(createInfoBox("HIZ", "-- m/s", &speedLabel), 2, 0);
    grid->addWidget(createInfoBox("YÖN", "-- °", &headingLabel), 3, 0);

    grid->addWidget(uavLabel, 1, 1, 3, 2, Qt::AlignCenter);

    grid->addWidget(createInfoBox("İRTİFA", "-- m", &altitudeLabel), 2, 3);
    grid->addWidget(createInfoBox("BATARYA", "-- %", &batteryLabel), 3, 3);

    grid->addWidget(createInfoBox("PITCH", "-- °", &pitchLabel), 4, 1);
    grid->addWidget(createInfoBox("ROLL", "-- °", &rollLabel), 4, 2);

    return panel;
}

// Orijinal görselin alfa kanalını koruyarak tamamını tek bir renge boyar
// (ikon tintleme tekniği). Şema ED'de lacivert, ET'de pastel turkuaz görünür.
QPixmap MainWindow::tintliPixmap(const QPixmap &kaynak, const QColor &renk) const
{
    if (kaynak.isNull()) return kaynak;

    QPixmap sonuc(kaynak.size());
    sonuc.fill(Qt::transparent);

    QPainter p(&sonuc);
    p.drawPixmap(0, 0, kaynak);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(sonuc.rect(), renk);
    p.end();

    return sonuc;
}

void MainWindow::updateUavRotation()
{
    if (uavPixmapOriginal.isNull()) return;

    QPixmap tintli = tintliPixmap(uavPixmapOriginal, uavTintRengi);

    QTransform transform;
    transform.rotate(uavData.heading);
    QPixmap rotated = tintli.transformed(transform, Qt::SmoothTransformation);
    uavLabel->setPixmap(rotated.scaled(380, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// ================= SOL PANEL: Dinamik Hedef Tablosu + Mini Harita =================
QWidget* MainWindow::buildPositionPanel()
{
    QFrame *panel = new QFrame();
    panel->setObjectName("panelFrame");
    QVBoxLayout *layout = new QVBoxLayout(panel);

    QLabel *title = new QLabel("TESPİT EDİLEN HEDEFLER");
    title->setObjectName("infoTitle");
    title->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(title);

    // 0 kartla başlıyor; her yeni bant/hedef tespit edildiğinde
    // hedefGuncelle() yeni bir kart ekliyor. Sinyal sayısı öngörülemediği
    // için kaydırılabilir bir alan içinde. Sütun bazlı tablo yerine dikey
    // "alan: değer" kartı kullanılıyor -- alan sayısı arttıkça (Bant
    // Genişliği, Analog/Sayısal, Modülasyon vb.) dar panelde metinlerin
    // kesilmesi sorununu ortadan kaldırıyor (bkz. buildHedefKarti).
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    // Boşken devasa bir boşluk gibi durmasın diye küçük başlıyor, hedef
    // geldikçe kartlarla birlikte büyüyor (en fazla 260px, sonrası kayar).
    scroll->setMinimumHeight(50);
    scroll->setMaximumHeight(260);

    QWidget *kartAlani = new QWidget();
    hedefKartlariLayout = new QVBoxLayout(kartAlani);
    hedefKartlariLayout->setContentsMargins(0, 0, 0, 0);
    hedefKartlariLayout->setSpacing(6);

    bosHedefMesaji = new QLabel("Henüz hedef tespit edilmedi.");
    bosHedefMesaji->setObjectName("infoTitle");
    bosHedefMesaji->setAlignment(Qt::AlignCenter);
    hedefKartlariLayout->addWidget(bosHedefMesaji);

    hedefKartlariLayout->addStretch(); // yeni kartlar bunun önüne insertWidget ile eklenir
    scroll->setWidget(kartAlani);

    layout->addWidget(scroll);

    QLabel *mapTitle = new QLabel("KONUM HARİTASI");
    mapTitle->setObjectName("infoTitle");
    mapTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(mapTitle);

    miniMapPlot = buildMiniMap();
    layout->addWidget(miniMapPlot);

    return panel;
}

// ================= MİNİ 2D KONUM HARİTASI =================
QCustomPlot* MainWindow::buildMiniMap()
{
    QCustomPlot *plot = new QCustomPlot();
    plot->setBackground(QColor("#0b0f19"));
    plot->setMinimumHeight(200);

    plot->xAxis->setLabel("Boylam");
    plot->yAxis->setLabel("Enlem");
    plot->xAxis->setLabelColor(QColor("#6fe0d0"));
    plot->yAxis->setLabelColor(QColor("#6fe0d0"));
    plot->xAxis->setTickLabelColor(QColor("#6fe0d0"));
    plot->yAxis->setTickLabelColor(QColor("#6fe0d0"));
    plot->xAxis->setBasePen(QPen(QColor("#1e3a4a")));
    plot->yAxis->setBasePen(QPen(QColor("#1e3a4a")));
    plot->xAxis->grid()->setPen(QPen(QColor("#1e3a4a"), 1, Qt::DotLine));
    plot->yAxis->grid()->setPen(QPen(QColor("#1e3a4a"), 1, Qt::DotLine));

    // İHA noktası (turkuaz) -- sabit, her zaman var.
    uavMapPoint = plot->addGraph();
    uavMapPoint->setLineStyle(QCPGraph::lsNone);
    uavMapPoint->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangle, QColor("#00ffcc"), 12));

    // Hedef noktaları artık burada DEĞİL -- her yeni hedef ilk tespit
    // edildiğinde hedefGuncelle() içinde dinamik olarak ekleniyor.

    plot->xAxis->setRange(32.80, 32.90);
    plot->yAxis->setRange(39.88, 39.96);

    return plot;
}


// ================= SAĞ PANEL: Olay Günlüğü =================
// Panel üç eşit bölüme ayrılmış (her biri stretch=1): Olay Günlüğü / Sinyal
// İzleme / Yön ve Konum Bulma. Her bölüm kendi alt-layout'una sarılıyor ki
// stretch faktörü başlık+içerik birlikte 1/3'lük alanı paylaşsın.
QWidget* MainWindow::buildSignalPanel()
{
    QFrame *panel = new QFrame();
    panel->setObjectName("panelFrame");
    QVBoxLayout *layout = new QVBoxLayout(panel);

    // ================= 1/3: OLAY GÜNLÜĞÜ =================
    QWidget *gunlukBolumu = new QWidget();
    QVBoxLayout *gunlukLayout = new QVBoxLayout(gunlukBolumu);
    gunlukLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *title = new QLabel("OLAY GÜNLÜĞÜ");
    title->setObjectName("infoTitle");
    title->setStyleSheet("font-size: 13px; padding: 4px;");
    gunlukLayout->addWidget(title);

    // Sinyal başına sabit kutular kaldırıldı -- artık aynı bilgi (frekans,
    // güç, durum) soldaki dinamik hedef kartlarında; burada sadece
    // kronolojik olay akışı (jüri/operatör "ne zaman ne oldu" görsün diye).
    eventLog = new QListWidget();
    gunlukLayout->addWidget(eventLog);

    layout->addWidget(gunlukBolumu, 1);

    // ================= 1/3: SİNYAL İZLEME/DİNLEME (madde 5.1.3) =================
    // Donanım/algoritma tarafı henüz bağlı değil -- Dinle butonu şimdilik
    // sadece durumu değiştirip günlüğe yazıyor.
    QWidget *dinleBolumu = new QWidget();
    QVBoxLayout *dinleLayout = new QVBoxLayout(dinleBolumu);
    dinleLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *dinleTitle = new QLabel("SİNYAL İZLEME");
    dinleTitle->setObjectName("infoTitle");
    dinleTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    dinleLayout->addWidget(dinleTitle);

    // Diğer başlat/durdur butonlarıyla (ör. ALDATMAYI BAŞLAT) aynı
    // görünümde olması için tam genişlikte.
    dinleButonu = new QPushButton("DİNLE");
    dinleButonu->setCheckable(true);
    dinleButonu->setFixedHeight(28);
    connect(dinleButonu, &QAbstractButton::toggled, this, &MainWindow::dinlemeDurumuDegisti);
    dinleLayout->addWidget(dinleButonu);

    demodulasyonDurumuLabel = new QLabel("Demodülasyon: --");
    demodulasyonDurumuLabel->setObjectName("infoTitle");
    dinleLayout->addWidget(demodulasyonDurumuLabel);
    dinleLayout->addStretch();

    layout->addWidget(dinleBolumu, 1);

    // ================= TARAMA BANDI (madde 5.1.1) =================
    // Hakem bant açıklarsa (ya da operatör başka bir test frekansına
    // geçecekse) kod değiştirip süreci yeniden başlatmaya gerek kalmadan
    // BANT_AYARLA| komutuyla backend'e (streamer.py/pluto_ed_scanner.py,
    // hangisi çalışıyorsa) bildirilir.
    QWidget *bantBolumu = new QWidget();
    QVBoxLayout *bantLayout = new QVBoxLayout(bantBolumu);
    bantLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *bantTitle = new QLabel("TARAMA BANDI");
    bantTitle->setObjectName("infoTitle");
    bantTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    bantLayout->addWidget(bantTitle);

    QHBoxLayout *bantGirisRow = new QHBoxLayout();
    bantBaslangicSpin = new QDoubleSpinBox();
    bantBaslangicSpin->setRange(24.0, 6000.0);
    bantBaslangicSpin->setDecimals(3);
    bantBaslangicSpin->setSingleStep(0.5);
    bantBaslangicSpin->setValue(430.0);
    bantBaslangicSpin->setSuffix(" MHz");

    QLabel *bantAyracLabel = new QLabel("-");
    bantAyracLabel->setObjectName("infoTitle");

    bantBitisSpin = new QDoubleSpinBox();
    bantBitisSpin->setRange(24.0, 6000.0);
    bantBitisSpin->setDecimals(3);
    bantBitisSpin->setSingleStep(0.5);
    bantBitisSpin->setValue(440.0);
    bantBitisSpin->setSuffix(" MHz");

    bantGirisRow->addWidget(bantBaslangicSpin);
    bantGirisRow->addWidget(bantAyracLabel);
    bantGirisRow->addWidget(bantBitisSpin);
    bantLayout->addLayout(bantGirisRow);

    bantUygulaButonu = new QPushButton("BANDI UYGULA");
    bantUygulaButonu->setFixedHeight(26);
    connect(bantUygulaButonu, &QPushButton::clicked, this, [this]() {
        const double baslangic = bantBaslangicSpin->value();
        const double bitis = bantBitisSpin->value();
        if (baslangic >= bitis) {
            addLogEntry("[ED] Bant başlangıcı bitişten küçük olmalı.");
            return;
        }
        if (mZmqPublisher) {
            mZmqPublisher->sendLine(QString("BANT_AYARLA|%1|%2")
                                         .arg(baslangic, 0, 'f', 3)
                                         .arg(bitis, 0, 'f', 3));
        }
        addLogEntry(QString("Tarama bandı gönderildi: %1-%2 MHz")
                        .arg(baslangic, 0, 'f', 3).arg(bitis, 0, 'f', 3));
    });
    bantLayout->addWidget(bantUygulaButonu);

    QPushButton *bantVarsayilanButonu = new QPushButton("VARSAYILANA DÖN");
    bantVarsayilanButonu->setFixedHeight(24);
    connect(bantVarsayilanButonu, &QPushButton::clicked, this, [this]() {
        if (mZmqPublisher)
            mZmqPublisher->sendLine("BANT_VARSAYILAN");
        addLogEntry("Tarama bandı varsayılana döndürüldü.");
    });
    bantLayout->addWidget(bantVarsayilanButonu);

    // Sürekli tarama (her SPEC paketinde retune+capture+repaint) zayıf
    // sistemlerde arayüzü zorlayabiliyor -- operatör izlemeye ara vermek
    // isterse backend'i durdurup GUI'yi rahatlatan aç/kapa düğmesi. DİNLE
    // aktifken zaten tarama durur (bkz. main döngüsü), bu düğme SADECE
    // ARAMA/İZLEME modlarını hedefliyor.
    taramaDurdurButonu = new QPushButton("TARAMAYI DURDUR");
    taramaDurdurButonu->setCheckable(true);
    taramaDurdurButonu->setFixedHeight(26);
    connect(taramaDurdurButonu, &QAbstractButton::toggled, this, [this](bool durduruldu) {
        taramaDurdurButonu->setText(durduruldu ? "TARAMAYI DEVAM ETTİR" : "TARAMAYI DURDUR");
        if (mZmqPublisher)
            mZmqPublisher->sendLine(durduruldu ? "TARAMA_DURDUR" : "TARAMA_DEVAM");
        addLogEntry(durduruldu ? "Tarama durduruldu." : "Tarama devam ediyor.");
    });
    bantLayout->addWidget(taramaDurdurButonu);

    layout->addWidget(bantBolumu, 1);

    // ================= 1/3: YÖN VE KONUM BULMA (madde 5.1.4 + 5.1.5) =================
    // DF göstergeleri (açı/yöntem/RMS) ile konum kestirimi (LOB/TDOA
    // birleşiminden) tek bölümde -- gerçek veri geldiğinde (örn. "DF" seri
    // paket tipi) güncellenecek placeholder alanlar.
    QWidget *dfBolumu = new QWidget();
    QVBoxLayout *dfLayout = new QVBoxLayout(dfBolumu);
    dfLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *dfTitle = new QLabel("YÖN VE KONUM BULMA");
    dfTitle->setObjectName("infoTitle");
    dfTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    dfLayout->addWidget(dfTitle);

    QHBoxLayout *dfRow = new QHBoxLayout();
    yonAcisiLabel = new QLabel("Açı: -- °");
    yonYontemiLabel = new QLabel("Yöntem: --");
    rmsHataLabel = new QLabel("RMS: -- °");
    for (QLabel *lbl : {yonAcisiLabel, yonYontemiLabel, rmsHataLabel}) {
        lbl->setObjectName("infoTitle");
        dfRow->addWidget(lbl);
    }
    dfRow->addStretch();
    dfLayout->addLayout(dfRow);

    QHBoxLayout *konumRow = new QHBoxLayout();
    konumEnlemLabel = new QLabel("Enlem: --");
    konumBoylamLabel = new QLabel("Boylam: --");
    for (QLabel *lbl : {konumEnlemLabel, konumBoylamLabel}) {
        lbl->setObjectName("infoTitle");
        konumRow->addWidget(lbl);
    }
    konumRow->addStretch();
    dfLayout->addLayout(konumRow);
    dfLayout->addStretch();

    layout->addWidget(dfBolumu, 1);

    return panel;
}

// ================= ET SOL PANEL: Onaylı Hedef Adayları =================
// ED'de tespit edilen bir hedef otomatik buraya düşmez -- operatör önce
// ED kartındaki "ET'YE ONAYLA" butonuna basmalı (bkz. hedefGuncelle içindeki
// onay lambda'sı). Onaylanan hedefler burada birikir; hangisinin AKTİF
// karıştırma/aldatma hedefi olacağı ise bu kartlardan birine tıklanarak
// (bkz. etHedefSec) ayrıca seçilir -- iki adımlı, kasıtlı bir akış.
QWidget* MainWindow::buildETHedefPanel()
{
    QFrame *panel = new QFrame();
    panel->setObjectName("panelFrame");
    QVBoxLayout *layout = new QVBoxLayout(panel);

    QLabel *title = new QLabel("ET HEDEF ADAYLARI (ONAYLI)");
    title->setObjectName("infoTitle");
    title->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(title);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(50);
    scroll->setMaximumHeight(260);

    QWidget *kartAlani = new QWidget();
    etHedefKartlariLayout = new QVBoxLayout(kartAlani);
    etHedefKartlariLayout->setContentsMargins(0, 0, 0, 0);
    etHedefKartlariLayout->setSpacing(6);

    etBosHedefMesaji = new QLabel("Henüz onaylı hedef yok -- ED panelinde bir hedef kartındaki \"ET'YE ONAYLA\"ya bas.");
    etBosHedefMesaji->setObjectName("infoTitle");
    etBosHedefMesaji->setAlignment(Qt::AlignCenter);
    etBosHedefMesaji->setWordWrap(true);
    etHedefKartlariLayout->addWidget(etBosHedefMesaji);

    etHedefKartlariLayout->addStretch();
    scroll->setWidget(kartAlani);

    layout->addWidget(scroll);
    layout->addStretch();

    return panel;
}

// ================= ET SAĞ PANEL: Aktif Karıştırma Göstergesi (placeholder) =================
// İçerik (hangi bant karıştırılıyor, çıkış gücü, süre vb.) henüz netleşmedi;
// şimdilik sabit iki info kutusu. Veri kaynağı belirlenince doldurulacak.
QWidget* MainWindow::buildETGucPanel()
{
    QFrame *panel = new QFrame();
    panel->setObjectName("panelFrame");
    QVBoxLayout *layout = new QVBoxLayout(panel);

    QLabel *title = new QLabel("AKTİF KARIŞTIRMA");
    title->setObjectName("infoTitle");
    title->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(title);

    QHBoxLayout *row = new QHBoxLayout();
    row->addWidget(createInfoBox("AKTİF BANT", "--", &etAktifBantLabel));
    row->addWidget(createInfoBox("ÇIKIŞ GÜCÜ", "-- dBm", &etCikisGucuLabel));
    row->addStretch();
    layout->addLayout(row);

    // ===== ÇIKIŞ GÜCÜ KAYDIRICI =====
    // Aralık: -20 dBm (minimum) … +30 dBm (maksimum), 1 dBm adım.
    // et_kontrol protokolüyle uyumlu "SET_POWER <değer>" komutu gönderir.
    QFrame *gucFrame = new QFrame();
    gucFrame->setObjectName("panelFrame");
    gucFrame->setStyleSheet("QFrame#panelFrame { border: 1px solid #c0392b; border-radius: 4px; padding: 4px; }");
    QVBoxLayout *gucLayout = new QVBoxLayout(gucFrame);
    gucLayout->setSpacing(4);
    gucLayout->setContentsMargins(8, 6, 8, 6);

    QHBoxLayout *gucBaslikRow = new QHBoxLayout();
    QLabel *gucBaslik = new QLabel("ÇIKIŞ GÜCÜ AYARI");
    gucBaslik->setObjectName("infoTitle");
    gucBaslik->setStyleSheet("font-size: 11px; font-weight: bold; letter-spacing: 1px;");

    etGucSpinBox = new QSpinBox();
    etGucSpinBox->setRange(-20, 30);
    etGucSpinBox->setValue(0);
    etGucSpinBox->setSuffix(" dBm");
    etGucSpinBox->setFixedWidth(78);
    etGucSpinBox->setStyleSheet("font-size: 13px; font-weight: bold;");

    gucBaslikRow->addWidget(gucBaslik);
    gucBaslikRow->addStretch();
    gucBaslikRow->addWidget(etGucSpinBox);
    gucLayout->addLayout(gucBaslikRow);

    etGucSlider = new QSlider(Qt::Horizontal);
    etGucSlider->setRange(-20, 30);
    etGucSlider->setValue(0);
    etGucSlider->setTickPosition(QSlider::TicksBelow);
    etGucSlider->setTickInterval(10);
    etGucSlider->setMinimumHeight(28);
    etGucSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  height: 6px; background: #333; border-radius: 3px; }"
        "QSlider::sub-page:horizontal {"
        "  background: #c0392b; border-radius: 3px; }"
        "QSlider::handle:horizontal {"
        "  width: 16px; height: 16px; margin: -5px 0;"
        "  background: #e74c3c; border: 2px solid #fff; border-radius: 8px; }"
        "QSlider::handle:horizontal:hover {"
        "  background: #ff6b6b; }"
    );
    gucLayout->addWidget(etGucSlider);

    QHBoxLayout *gucEtiketRow = new QHBoxLayout();
    QLabel *eMin = new QLabel("-20 dBm");
    QLabel *eMid = new QLabel("0 dBm");
    QLabel *eMax = new QLabel("+30 dBm");
    for (QLabel *lbl : {eMin, eMid, eMax}) {
        lbl->setObjectName("infoTitle");
        lbl->setStyleSheet("font-size: 10px; color: #888;");
    }
    eMid->setAlignment(Qt::AlignCenter);
    eMax->setAlignment(Qt::AlignRight);
    gucEtiketRow->addWidget(eMin);
    gucEtiketRow->addWidget(eMid, 1);
    gucEtiketRow->addWidget(eMax);
    gucLayout->addLayout(gucEtiketRow);

    layout->addWidget(gucFrame);

    connect(etGucSlider, &QSlider::valueChanged, etGucSpinBox, &QSpinBox::setValue);
    connect(etGucSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            etGucSlider, &QSlider::setValue);

    connect(etGucSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int dbm) {
                etCikisGucuLabel->setText(QString("%1 dBm").arg(dbm));
                if (mZmqPublisher)
                    mZmqPublisher->sendLine(QString("SET_POWER %1").arg(dbm));
            });

    // ================= SÜREKLİ KARIŞTIRMA (5.2.1) + ARABAKIŞLI (5.2.2) =================
    QLabel *karistirmaTitle = new QLabel("KARIŞTIRMA TİPİ VE MODU");
    karistirmaTitle->setObjectName("infoTitle");
    karistirmaTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(karistirmaTitle);

    QHBoxLayout *tipRow = new QHBoxLayout();
    tekliButonu = new QPushButton("TEKLİ");
    cokluButonu = new QPushButton("ÇOKLU");
    barajButonu = new QPushButton("BARAJ");
    karistirmaTipiGrubu = new QButtonGroup(this);
    karistirmaTipiGrubu->setExclusive(true);
    for (QPushButton *btn : {tekliButonu, cokluButonu, barajButonu}) {
        btn->setCheckable(true);
        btn->setFixedSize(56, 24);
        karistirmaTipiGrubu->addButton(btn);
        tipRow->addWidget(btn);
    }
    tekliButonu->setChecked(true);
    tipRow->addStretch();
    layout->addLayout(tipRow);

    QHBoxLayout *manuelFrekansRow = new QHBoxLayout();
    QLabel *manuelFrekansLabel = new QLabel("Manuel Frekans (hedef seçilmezse):");
    manuelFrekansLabel->setObjectName("infoTitle");
    etManuelFrekansSpin = new QDoubleSpinBox();
    // Pluto TX aralığı -- et_control.py PLUTO_TX_MIN_HZ/MAX_HZ.
    etManuelFrekansSpin->setRange(70.0, 6000.0);
    etManuelFrekansSpin->setDecimals(3);
    etManuelFrekansSpin->setSingleStep(0.1);
    etManuelFrekansSpin->setValue(2450.0);
    etManuelFrekansSpin->setSuffix(" MHz");
    manuelFrekansRow->addWidget(manuelFrekansLabel);
    manuelFrekansRow->addWidget(etManuelFrekansSpin);
    layout->addLayout(manuelFrekansRow);

    QHBoxLayout *moduRow = new QHBoxLayout();
    surekliButonu = new QPushButton("SÜREKLİ");
    arabakisliButonu = new QPushButton("ARABAKIŞLI");
    karistirmaModuGrubu = new QButtonGroup(this);
    karistirmaModuGrubu->setExclusive(true);
    for (QPushButton *btn : {surekliButonu, arabakisliButonu}) {
        btn->setCheckable(true);
        btn->setFixedHeight(24);
        karistirmaModuGrubu->addButton(btn);
        moduRow->addWidget(btn);
    }
    surekliButonu->setChecked(true);
    moduRow->addStretch();
    layout->addLayout(moduRow);

    // "Arabakışlı" seçiliyken karıştırma sisteminin RF Karıştırma Sistemi ile
    // koordineli bir almaç kullandığını belirtmesi gerekir (madde 5.2.2) --
    // şimdilik bunu sadece buton seçimi olarak tutuyoruz, almaç bağlantısı
    // sonraki bir aşamada eklenecek.
    karistirmaBaslatButonu = new QPushButton("KARIŞTIRMAYI BAŞLAT");
    karistirmaBaslatButonu->setCheckable(true);
    karistirmaBaslatButonu->setFixedHeight(28);
    connect(karistirmaBaslatButonu, &QAbstractButton::toggled, this, &MainWindow::karistirmaBaslatDurdur);
    layout->addWidget(karistirmaBaslatButonu);

    // ================= ANALOG TELSİZ ALDATMA (5.2.3) =================
    QLabel *aldatmaTitle = new QLabel("ANALOG TELSİZ ALDATMA");
    aldatmaTitle->setObjectName("infoTitle");
    aldatmaTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(aldatmaTitle);

    // Mesaj kaynağı: KAYIT-TEKRAR (data/aldatma_sesleri/'den bir .wav) veya
    // PİPER TTS (girilen metni yerel/çevrimdışı Türkçe TTS ile seslendirir).
    // et_control.py'ye ALDATMAYI BAŞLAT'a basılınca ALDATMA_KAYNAK| komutuyla
    // iletilir (bkz. aldatmaBaslatDurdur).
    QHBoxLayout *aldatmaKaynakRow = new QHBoxLayout();
    aldatmaKayitTekrarButonu = new QPushButton("KAYIT-TEKRAR");
    aldatmaPiperTtsButonu = new QPushButton("PİPER TTS");
    aldatmaKaynakGrubu = new QButtonGroup(this);
    aldatmaKaynakGrubu->setExclusive(true);
    for (QPushButton *btn : {aldatmaKayitTekrarButonu, aldatmaPiperTtsButonu}) {
        btn->setCheckable(true);
        btn->setFixedHeight(24);
        aldatmaKaynakGrubu->addButton(btn);
        aldatmaKaynakRow->addWidget(btn);
    }
    aldatmaKayitTekrarButonu->setChecked(true);
    layout->addLayout(aldatmaKaynakRow);

    // KAYIT-TEKRAR: data/aldatma_sesleri/ altındaki .wav dosyalarını listeler.
    // NOT: EHARPP_1 (bu proje) ve Python backend (Ebabil_EH_AI) ayrı depolarda
    // yaşıyor -- geliştirme makinesine özgü sabit bir yol kullanıyoruz.
    aldatmaDosyaSecimi = new QComboBox();
    const QString sesDizini = "C:/Users/habib/Desktop/PycharmProjects/Ebabil_EH_AI/data/aldatma_sesleri";
    QDir dizin(sesDizini);
    const QStringList wavlar = dizin.entryList(QStringList{"*.wav"}, QDir::Files, QDir::Name);
    if (wavlar.isEmpty()) {
        aldatmaDosyaSecimi->addItem("(hiç .wav yok -- sentetik tona düşülür)");
        aldatmaDosyaSecimi->setEnabled(false);
    } else {
        aldatmaDosyaSecimi->addItems(wavlar);
    }
    layout->addWidget(aldatmaDosyaSecimi);

    // PİPER TTS: seslendirilecek metin -- başlangıçta gizli, sadece PİPER TTS
    // seçiliyken görünür.
    aldatmaMetinKutusu = new QLineEdit();
    aldatmaMetinKutusu->setPlaceholderText("Seslendirilecek metni yazın...");
    aldatmaMetinKutusu->hide();
    layout->addWidget(aldatmaMetinKutusu);

    connect(aldatmaKaynakGrubu, &QButtonGroup::buttonToggled, this, [this](QAbstractButton *btn, bool checked) {
        if (!checked) return;
        const bool piperSecili = (btn == aldatmaPiperTtsButonu);
        aldatmaDosyaSecimi->setVisible(!piperSecili);
        aldatmaMetinKutusu->setVisible(piperSecili);
    });

    // Diğer başlat/durdur butonlarıyla (KARIŞTIRMAYI BAŞLAT, GNSS ALDATMAYI
    // BAŞLAT) aynı boyutta olması için tam genişlikte, kendi satırında.
    aldatmaBaslatButonu = new QPushButton("ALDATMAYI BAŞLAT");
    aldatmaBaslatButonu->setCheckable(true);
    aldatmaBaslatButonu->setFixedHeight(28);
    connect(aldatmaBaslatButonu, &QAbstractButton::toggled, this, &MainWindow::aldatmaBaslatDurdur);
    layout->addWidget(aldatmaBaslatButonu);

    aldatmaDurumLabel = new QLabel("Durum: --");
    aldatmaDurumLabel->setObjectName("infoTitle");
    layout->addWidget(aldatmaDurumLabel);

    // ================= GNSS ALDATMA (5.2.4) =================
    QLabel *gnssTitle = new QLabel("GNSS ALDATMA");
    gnssTitle->setObjectName("infoTitle");
    gnssTitle->setStyleSheet("font-size: 13px; padding: 4px;");
    layout->addWidget(gnssTitle);

    layout->addWidget(buildServisSatiri("GPS", {"L1", "L2", "L5"}));
    layout->addWidget(buildServisSatiri("GLONASS", {"L1", "L2", "L3"}));
    layout->addWidget(buildServisSatiri("GALILEO", {"E1", "E5a", "E5b", "E6"}));
    layout->addWidget(buildServisSatiri("BEIDOU", {"B1", "B2", "B3"}));

    gnssBaslatButonu = new QPushButton("GNSS ALDATMAYI BAŞLAT");
    gnssBaslatButonu->setCheckable(true);
    gnssBaslatButonu->setFixedHeight(28);
    connect(gnssBaslatButonu, &QAbstractButton::toggled, this, &MainWindow::gnssAldatmaBaslatDurdur);
    layout->addWidget(gnssBaslatButonu);

    layout->addStretch();

    return panel;
}

// ================= GNSS SERVİS SATIRI (yardımcı) =================
QWidget* MainWindow::buildServisSatiri(const QString &baslik, const QStringList &servisler)
{
    QWidget *satir = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(satir);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel *baslikLbl = new QLabel(baslik);
    baslikLbl->setObjectName("infoTitle");
    baslikLbl->setFixedWidth(62);
    layout->addWidget(baslikLbl);

    for (const QString &servis : servisler) {
        QPushButton *btn = new QPushButton(servis);
        btn->setCheckable(true);
        btn->setFixedSize(42, 22);
        btn->setProperty("servisAdi", QString("%1 %2").arg(baslik, servis));
        // GPS L1: madde 5.2.4'te belirtilen asgari (geçer) kriter -- varsayılan seçili.
        if (baslik == "GPS" && servis == "L1") {
            btn->setChecked(true);
        }
        gnssServisButonlari.append(btn);
        layout->addWidget(btn);
    }
    layout->addStretch();
    return satir;
}

// ================= ALT PANEL: Şelale Grafiği =================
QCustomPlot* MainWindow::buildWaterfallPlot()
{
    QCustomPlot *plot = new QCustomPlot();
    plot->setBackground(QColor("#0b0f19"));

    colorMap = new QCPColorMap(plot->xAxis, plot->yAxis);
    colorMap->data()->setSize(FREQ_BINS, WATERFALL_HISTORY);
    // Başlangıç için makul bir yer tutucu pencere (ISM 433 MHz civarı) --
    // ilk SPEC/hedef verisi geldiğinde odaklanGerekirse() gerçek pencereyi
    // kuracak (bkz. specGuncelle).
    colorMap->data()->setRange(
        QCPRange(430.0, 437.0),
        QCPRange(0, WATERFALL_HISTORY)
        );

    // gqrx/GNU Radio tarzı "turbo" gradyan: gürültü tabanı bile arka planla
    // aynı renk olmasın diye 0.0 durağı arka plandan (#0b0f19) BİLEREK farklı
    // -- aksi halde hiç sinyal olmayan bin'ler görsel olarak "yok" gibi
    // görünüyor ve şelale boş/dokusuz duruyordu.
    QCPColorGradient gradient;
    gradient.setColorStopAt(0.0, QColor("#061a26"));
    gradient.setColorStopAt(0.15, QColor("#0d3b52"));
    gradient.setColorStopAt(0.35, QColor("#1f9c8f"));
    gradient.setColorStopAt(0.55, QColor("#8fd744"));
    gradient.setColorStopAt(0.75, QColor("#f2c744"));
    gradient.setColorStopAt(0.9, QColor("#f2793f"));
    gradient.setColorStopAt(1.0, QColor("#ff3b30"));
    colorMap->setGradient(gradient);
    // gqrx/SDR++ tarzı pürüzsüz (bilinear) görüntüleme -- hücre hücre keskin
    // kareler yerine komşu hücreler arasında yumuşak geçiş.
    colorMap->setInterpolate(true);
    // Gerçek kontrast/aralık (Taban/Tepe dB) buildWaterfallKontrastCubugu()
    // kontrolleriyle veya oto-modda spektrumVeSelaleGuncelle() içinde her
    // satırda yeniden ayarlanıyor. Burada sadece başlangıç için makul bir
    // varsayılan veriliyor.
    colorMap->setDataRange(QCPRange(-110, -40));

    plot->xAxis->setLabel("Frekans (MHz)");
    plot->xAxis->setLabelColor(QColor("#6fe0d0"));
    plot->xAxis->setTickLabelColor(QColor("#6fe0d0"));
    plot->xAxis->setBasePen(QPen(QColor("#1e3a4a")));

    plot->yAxis->setLabel("Zaman");
    plot->yAxis->setLabelColor(QColor("#6fe0d0"));
    plot->yAxis->setTickLabelColor(QColor("#6fe0d0"));
    plot->yAxis->setBasePen(QPen(QColor("#1e3a4a")));

    plot->rescaleAxes();

    for (int x = 0; x < FREQ_BINS; ++x) {
        for (int y = 0; y < WATERFALL_HISTORY; ++y) {
            colorMap->data()->setCell(x, y, -110);
        }
    }
    colorMap->rescaleDataRange();

    // Sürekli spektrum akışının (bkz. specGuncelle) taban katmanı -- henüz
    // hiçbir SPEC paketi gelmemiş bin'ler bu başlangıç değerinde kalır.
    spektrumZeminGucleri.resize(FREQ_BINS);
    spektrumZeminGucleri.fill(-110);
    spektrumEma.resize(FREQ_BINS);
    spektrumEma.fill(-110);

    return plot;
}

// gqrx/SDR++'taki "Ref/Range" kontrolleriyle aynı amaç: gürültü tabanını
// taban renge sabitlemek için manuel Taban/Tepe (dB) eşiği + bunu kapatıp
// otomatik (her satırın kendi min/max'ı) moda dönmek için bir onay kutusu.
// ebabil_sdr'ın gönderdiği ham skalanın gerçek aralığı önceden bilinemediği
// için varsayılan Oto AÇIK -- kullanıcı isterse kapatıp sabit bir aralığa kilitleyebilir.
QWidget* MainWindow::buildWaterfallKontrastCubugu()
{
    QWidget *cubuk = new QWidget();
    QHBoxLayout *lay = new QHBoxLayout(cubuk);
    lay->setContentsMargins(4, 0, 4, 0);
    lay->setSpacing(8);

    QLabel *baslik = new QLabel("Kontrast:");
    baslik->setStyleSheet("color:#6fe0d0; font-size:11px;");

    waterfallOtoKontrastCheck = new QCheckBox("Oto");
    waterfallOtoKontrastCheck->setChecked(true);
    waterfallOtoKontrastCheck->setStyleSheet("color:#6fe0d0; font-size:11px;");

    QLabel *tabanLabel = new QLabel("Taban (dB):");
    tabanLabel->setStyleSheet("color:#6fe0d0; font-size:11px;");
    waterfallTabanSpin = new QDoubleSpinBox();
    waterfallTabanSpin->setRange(-150.0, 150.0);
    waterfallTabanSpin->setValue(0.0);
    waterfallTabanSpin->setSingleStep(1.0);
    waterfallTabanSpin->setEnabled(false); // Oto açıkken pasif

    QLabel *tepeLabel = new QLabel("Tepe (dB):");
    tepeLabel->setStyleSheet("color:#6fe0d0; font-size:11px;");
    waterfallTepeSpin = new QDoubleSpinBox();
    waterfallTepeSpin->setRange(-150.0, 150.0);
    waterfallTepeSpin->setValue(30.0);
    waterfallTepeSpin->setSingleStep(1.0);
    waterfallTepeSpin->setEnabled(false);

    connect(waterfallOtoKontrastCheck, &QCheckBox::toggled, this, [this](bool oto) {
        waterfallTabanSpin->setEnabled(!oto);
        waterfallTepeSpin->setEnabled(!oto);
    });

    lay->addWidget(baslik);
    lay->addWidget(waterfallOtoKontrastCheck);
    lay->addWidget(tabanLabel);
    lay->addWidget(waterfallTabanSpin);
    lay->addWidget(tepeLabel);
    lay->addWidget(waterfallTepeSpin);
    lay->addStretch();

    return cubuk;
}

// ================= SAĞ ALT PANEL: Anlık Spektrum Çizgisi =================
// gqrx/SDR# tarzı: şelalenin sağında, aynı frekans eksenini paylaşan tek
// çizgilik anlık güç grafiği. ED/ET arasında ortak -- mod değişince yeniden
// çizilmiyor, aynı canlı veriyi gösteriyor.
QCustomPlot* MainWindow::buildSpectrumPlot()
{
    QCustomPlot *plot = new QCustomPlot();
    plot->setBackground(QColor("#0b0f19"));

    plot->xAxis->setLabel("Frekans (MHz)");
    plot->xAxis->setLabelColor(QColor("#6fe0d0"));
    plot->xAxis->setTickLabelColor(QColor("#6fe0d0"));
    plot->xAxis->setBasePen(QPen(QColor("#1e3a4a")));
    plot->xAxis->grid()->setPen(QPen(QColor("#1e3a4a"), 1, Qt::DotLine));
    // QCustomPlot varsayılan olarak 0 değerinden geçen "sıfır çizgisini" farklı
    // (daha koyu/belirgin) bir kalemle çiziyor -- ham spektrum değerleri
    // pozitif/negatif 0'ı sık geçtiği için bu, ekranda anlamsız yatay bir
    // çizgi gibi duruyordu. Normal grid çizgisiyle aynı yapıyoruz.
    plot->xAxis->grid()->setZeroLinePen(QPen(QColor("#1e3a4a"), 1, Qt::DotLine));
    plot->yAxis->grid()->setZeroLinePen(QPen(QColor("#1e3a4a"), 1, Qt::DotLine));
    plot->xAxis->setRange(430.0, 437.0); // odaklanGerekirse() ilk veriyle güncelleyecek

    // NOT: streamer.py şu an KALİBRE EDİLMEMİŞ bir değer gönderiyor (ham FFT
    // genliğinden 20*log10 ile hesaplanan göreceli dB -- gerçek bir referansa
    // (anten kazancı, kablo kaybı) kalibre edilmedi). Bu yüzden eksen "dBm"
    // değil "dB" -- gerçek kalibrasyon eklenirse (ebabil_sdr'ın orijinal
    // tasarımındaki gibi) burası "dBm"e çevrilebilir.
    plot->yAxis->setLabel("Güç (dB, göreceli)");
    plot->yAxis->setLabelColor(QColor("#6fe0d0"));
    plot->yAxis->setTickLabelColor(QColor("#6fe0d0"));
    plot->yAxis->setBasePen(QPen(QColor("#1e3a4a")));
    plot->yAxis->grid()->setPen(QPen(QColor("#1e3a4a"), 1, Qt::DotLine));
    plot->yAxis->setRange(-120, 0);

    spektrumGrafik = plot->addGraph();
    spektrumGrafik->setPen(QPen(QColor("#00ffcc"), 1.5));

    // Bin frekansları artık sabit değil -- görünüm penceresi kaydıkça
    // odaklanGerekirse() tarafından yeniden hesaplanır. Burada sadece
    // boyutlandırılıp yer tutucu bir aralıkla dolduruluyor.
    spektrumFrekanslar.resize(FREQ_BINS);
    for (int x = 0; x < FREQ_BINS; ++x) {
        double oran = (x + 0.5) / FREQ_BINS;
        spektrumFrekanslar[x] = 430.0 + oran * 7.0;
    }

    return plot;
}

void MainWindow::odaklanGerekirse(double merkezMhz, double spanMhz)
{
    spanMhz = std::max(spanMhz, 0.5); // çok dar bir pencereye düşmesin

    // Küçük dalgalanmalarda (span'ın %15'inden az kayma, span'ın kendisi de
    // %20'den az değiştiyse) pencereyi yeniden kurmuyoruz -- aksi halde her
    // SPEC paketinde geçmiş silinir, şelale hiç birikemez/akmaz.
    const bool ilkKurulum = gorunumSpanMhz <= 0.0;
    const bool merkezKaydi = std::abs(merkezMhz - gorunumMerkezMhz) > spanMhz * 0.15;
    const bool spanDegisti = !ilkKurulum && std::abs(spanMhz - gorunumSpanMhz) > gorunumSpanMhz * 0.2;
    if (!ilkKurulum && !merkezKaydi && !spanDegisti) {
        return;
    }

    gorunumMerkezMhz = merkezMhz;
    gorunumSpanMhz = spanMhz;
    const double baslangic = gorunumMerkezMhz - gorunumSpanMhz / 2.0;
    const double bitis = gorunumMerkezMhz + gorunumSpanMhz / 2.0;

    colorMap->data()->setRange(QCPRange(baslangic, bitis), QCPRange(0, WATERFALL_HISTORY));
    for (int x = 0; x < FREQ_BINS; ++x) {
        for (int y = 0; y < WATERFALL_HISTORY; ++y) {
            colorMap->data()->setCell(x, y, -110);
        }
        const double oran = (x + 0.5) / FREQ_BINS;
        spektrumFrekanslar[x] = baslangic + oran * (bitis - baslangic);
        spektrumZeminGucleri[x] = -110;
        spektrumEma[x] = -110; // yeni pencerede eski EMA'nın izi kalmasın
    }
    waterfallRow = 0;

    waterfallPlot->xAxis->setRange(baslangic, bitis);
    spektrumPlot->xAxis->setRange(baslangic, bitis);
}

void MainWindow::spektrumVeSelaleGuncelle()
{
    if (gorunumSpanMhz <= 0.0) {
        return; // henüz hiçbir SPEC/hedef verisi gelmedi, gösterilecek pencere yok
    }

    const double gorunumBaslangic = gorunumMerkezMhz - gorunumSpanMhz / 2.0;
    const double gorunumBitis = gorunumMerkezMhz + gorunumSpanMhz / 2.0;

    // NOT: Hedef tespit overlay'i (parlak bir "sıcak bant" yapay olarak
    // bindirme) KALDIRILDI -- SPEC'in kendi gerçek spektrumu zaten peak'i
    // doğal olarak gösteriyor, ayrıca overlay girdi.veri.signalPower KALİBRE
    // dBm biriminde (~-80'ler) iken spektrumZeminGucleri ham/göreceli dB
    // biriminde (~15-25) olduğundan ikisi karşılaştırılamazdı -- bu da
    // ekranda anlamsız/karanlık bir şerit olarak görünüyordu.
    QVector<double> guclerBuAn(FREQ_BINS);
    for (int x = 0; x < FREQ_BINS; ++x) {
        double power = spektrumZeminGucleri[x];
        colorMap->data()->setCell(x, waterfallRow % WATERFALL_HISTORY, power);
        guclerBuAn[x] = power;
    }

    waterfallRow++;

    // Kontrast: gqrx/SDR++'taki Ref/Range mantığı -- Oto işaretliyse SADECE bu
    // son satırın gerçek min/max'ına göre otomatik ayarlanır (pencere yeni
    // kurulduğunda çoğu satır hâlâ -110 placeholder olduğundan, TÜM ızgarayı
    // taramak bunu gerçek ~15-25 dB'lik sinyal farkıyla karıştırıp her şeyi
    // gradyanın tepesine sıkıştırırdı). Oto kapalıysa kullanıcının Taban/Tepe
    // (dB) değerleri doğrudan kullanılır -- gürültü tabanını taban renge
    // sabitlemek isteyen kullanıcı için (bkz. istenen "dynamic range" kontrolü).
    if (!waterfallOtoKontrastCheck || waterfallOtoKontrastCheck->isChecked()) {
        double minVal = *std::min_element(guclerBuAn.begin(), guclerBuAn.end());
        double maxVal = *std::max_element(guclerBuAn.begin(), guclerBuAn.end());
        if (maxVal - minVal < 3.0) {
            double orta = (minVal + maxVal) / 2.0;
            minVal = orta - 1.5;
            maxVal = orta + 1.5;
        }
        colorMap->setDataRange(QCPRange(minVal - 2.0, maxVal + 2.0));
    } else {
        double taban = waterfallTabanSpin->value();
        double tepe = std::max(waterfallTepeSpin->value(), taban + 1.0);
        colorMap->setDataRange(QCPRange(taban, tepe));
    }

    // Spektrum ÇİZGİSİ için EMA ("video averaging") -- anlık titremek yerine
    // gqrx/SDR++'taki gibi yumuşak akar. Şelale hücreleri (yukarıda) HAM
    // değeri kullanmaya devam ediyor, sadece bu çizgi yumuşatılıyor.
    QVector<double> guclerEma(FREQ_BINS);
    for (int x = 0; x < FREQ_BINS; ++x) {
        spektrumEma[x] = SPEKTRUM_EMA_ALPHA * guclerBuAn[x] + (1.0 - SPEKTRUM_EMA_ALPHA) * spektrumEma[x];
        guclerEma[x] = spektrumEma[x];
    }
    spektrumGrafik->setData(spektrumFrekanslar, guclerEma);

    // replot() ikisi de pahalı (waterfall renklendirme + spektrum çizgisi) --
    // veri (yukarıdaki colorMap hücresi/EMA/waterfallRow) HER SPEC paketinde
    // güncellendi, ama gerçek repaint'i REPLOT_MIN_ARALIK_MS'den sık
    // YAPMIYORUZ. ebabil_sdr saniyede onlarca paket gönderebiliyor -- her
    // birinde tam replot() sistem yükü altında (ör. TensorFlow aynı anda
    // CPU'da) arayüzü tıkayıp "donmuş" hissi veriyordu (sahada gözlemlendi).
    // Ekran zaten insan gözünün ayırt edemeyeceğinden daha sık güncellenmiş
    // olurdu -- veri kaybı yok, sadece gereksiz repaint atlanıyor.
    const qint64 simdiMs = QDateTime::currentMSecsSinceEpoch();
    if (simdiMs - sonReplotMs < REPLOT_MIN_ARALIK_MS) {
        return;
    }
    sonReplotMs = simdiMs;

    waterfallPlot->replot();
    // Şelaledeki dataRange gibi bu eksen de artık gelen gerçek veriye göre
    // otomatik ölçekleniyor -- sabit (-120,0) varsayımı, ebabil_sdr'ın ham
    // değer skalası farklıysa çizgiyi ekranın dışına taşırabiliyordu.
    spektrumPlot->yAxis->rescale();
    spektrumPlot->replot();
}

void MainWindow::specGuncelle(double merkezMhz, double fsMhz, const QVector<double> &ornekler)
{
    if (ornekler.isEmpty() || fsMhz <= 0.0) {
        return;
    }

    // Bu paketin kapsadığı gerçek frekans aralığı -- ebabil_sdr'ın o anki
    // tarama adımının merkez frekansı ve örnekleme hızından (bkz. SPEC
    // paket formatı, TelemetriGonderici).
    const double baslangicMhz = merkezMhz - fsMhz / 2.0;
    const double bitisMhz = merkezMhz + fsMhz / 2.0;

    // Aktif ("tespit edildi" durumundaki) bir hedef varsa, SADECE o hedefin
    // bandına ait SPEC paketlerini işliyoruz -- başka bir banda ait paket
    // gelirse görünüm o bantta karışmasın diye yok sayılır.
    const HedefGirdisi *enSonHedef = nullptr;
    for (const HedefGirdisi &h : hedefler) {
        if (!h.veri.detected) continue;
        if (!enSonHedef || h.sonGorulmeMs > enSonHedef->sonGorulmeMs) enSonHedef = &h;
    }
    if (enSonHedef && (enSonHedef->veri.frequency < baslangicMhz || enSonHedef->veri.frequency > bitisMhz)) {
        return;
    }

    // ÖNEMLİ: görünüm penceresi hedefin dalgalanan tepe frekansına DEĞİL,
    // doğrudan bu paketin (yani o an taranan bandın) sabit merkez/genişliğine
    // göre kuruluyor. Tepe frekansı ölçüm gürültüsüyle her adımda birkaç kHz
    // oynuyordu ve bu, pencereyi sürekli kaydırıp "titremeye" (kullanıcı:
    // "sürekli hareket ediyor") sebep oluyordu. Bant sınırları ebabil_sdr
    // tarafında sabit (main.cpp bantlar tablosu) olduğu için artık pencere
    // gqrx'teki gibi durağan kalıyor, sadece içerik akıyor.
    odaklanGerekirse(merkezMhz, fsMhz);

    const double gorunumBaslangic = gorunumMerkezMhz - gorunumSpanMhz / 2.0;
    const double gorunumBitis = gorunumMerkezMhz + gorunumSpanMhz / 2.0;

    for (int i = 0; i < ornekler.size(); ++i) {
        const double oran = (i + 0.5) / ornekler.size();
        const double freqMhz = baslangicMhz + oran * (bitisMhz - baslangicMhz);

        if (freqMhz < gorunumBaslangic || freqMhz > gorunumBitis) {
            continue;
        }

        int binX = static_cast<int>((freqMhz - gorunumBaslangic)
                                     / (gorunumBitis - gorunumBaslangic) * FREQ_BINS);
        binX = std::clamp(binX, 0, FREQ_BINS - 1);
        spektrumZeminGucleri[binX] = ornekler[i];
    }

    // Taban güncellendi, waterfall/çizgiyi tespit katmanıyla birlikte
    // yeniden çiz -- tespit olsun olmasın, her SPEC paketinde bir satır
    // ilerler (bkz. spektrumVeSelaleGuncelle içindeki waterfallRow++).
    spektrumVeSelaleGuncelle();
}

// ================= DİNAMİK HEDEF YÖNETİMİ =================
QColor MainWindow::sonrakiRenk()
{
    static const QColor palet[] = {
        QColor("#ff4444"), QColor("#ffcc00"), QColor("#cc66ff"), QColor("#44aaff"),
        QColor("#ff8844"), QColor("#66ff99"), QColor("#ff66cc"), QColor("#aaff44"),
    };
    QColor renk = palet[sonrakiRenkIndex % static_cast<int>(sizeof(palet) / sizeof(palet[0]))];
    ++sonrakiRenkIndex;
    return renk;
}

void MainWindow::hedefGuncelle(const QString &id, bool tespitEdildi, double lat, double lon, double alt,
                                double freqMhz, double powerDbm, double bandwidthKHz,
                                double sapmaMhz, double gurultuTabaniDb, double snrDb, const QString &sureklilik)
{
    const bool yeni = !hedefler.contains(id);
    HedefGirdisi &girdi = hedefler[id];  // yoksa varsayılan değerle oluşturur

    if (yeni) {
        girdi.veri.name = id;
        girdi.renk = sonrakiRenk();

        // İlk hedef eklendiğinde "henüz hedef yok" mesajı gizlenir.
        bosHedefMesaji->hide();

        // ED kartı -- sıra önemli: hedefGuncelle/hedefYapayZekaGuncelle
        // içindeki edDegerLabellari[indeks] erişimleri bu listeyle eşleşiyor.
        // Son 4'ü (9-12) sonradan eklendi -- KTR Tablo 8'in geri kalanı.
        static const QStringList edAlanlari = {
            "Durum", "Frekans", "Bant Gen.", "Güç",
            "Analog/Sayısal", "Modülasyon", "Enlem", "Boylam", "İrtifa",
            "Frekans Sapması", "Gürültü Tabanı", "SNR", "Süreklilik"
        };
        girdi.edKarti = buildHedefKarti(id, girdi.renk, edAlanlari, &girdi.edBaslikLabel, &girdi.edDegerLabellari);

        // "ET'YE ONAYLA" -- operatör bu hedefi ET adayı yapar (etKarti burada
        // OLUŞTURULUR, otomatik değil). Onay kaldırılırsa kart gizlenir ama
        // silinmez (tekrar onaylanınca aynı kart geri gelir). Gerçek
        // karıştırma/aldatma hedefi seçimi ise ET kartına tıklanarak AYRICA
        // yapılır (bkz. etHedefSec) -- iki adımlı, kasıtlı akış.
        girdi.onaylaButonu = new QPushButton("ET'YE ONAYLA");
        girdi.onaylaButonu->setFixedHeight(22);
        girdi.onaylaButonu->setCheckable(true);
        girdi.onaylaButonu->setStyleSheet(
            "QPushButton { font-size:10px; background:#1a1a2e; color:#aaa; border:1px solid #555; border-radius:3px; }"
            "QPushButton:checked { background:#7b1f1f; color:#fff; border-color:#e74c3c; }"
        );
        static_cast<QVBoxLayout*>(girdi.edKarti->layout())->addWidget(girdi.onaylaButonu);

        connect(girdi.onaylaButonu, &QPushButton::toggled, this, [this, id](bool onay) {
            if (!hedefler.contains(id)) return;
            HedefGirdisi &g = hedefler[id];
            g.onaylandi = onay;
            if (onay && g.etKarti == nullptr) {
                static const QStringList etAlanlari = {"Frekans", "Sinyal Gücü", "Karıştırma Gücü", "Durum"};
                g.etKarti = buildHedefKarti(id, g.renk, etAlanlari, &g.etBaslikLabel, &g.etDegerLabellari);
                if (!g.etKarti) return;
                g.etKarti->setProperty("etHedefId", id);
                g.etKarti->setCursor(Qt::PointingHandCursor);
                g.etKarti->installEventFilter(this);
                if (etHedefKartlariLayout)
                    etHedefKartlariLayout->insertWidget(etHedefKartlariLayout->count() - 1, g.etKarti);
                if (etBosHedefMesaji)
                    etBosHedefMesaji->hide();
                if (g.etDegerLabellari.size() >= 2) {
                    g.etDegerLabellari[0]->setText(QString::number(g.veri.frequency, 'f', 3) + " MHz");
                    g.etDegerLabellari[1]->setText(QString::number(g.veri.signalPower, 'f', 1) + " dB");
                }
                addLogEntry(QString("%1 ET'ye onaylandı (%2 MHz)").arg(id).arg(g.veri.frequency, 0, 'f', 3));
            } else if (!onay && g.etKarti != nullptr) {
                g.etKarti->hide();
                addLogEntry(QString("%1 ET onayı kaldırıldı").arg(id));
            } else if (onay && g.etKarti != nullptr) {
                g.etKarti->show();
            }
            if (g.onaylaButonu)
                g.onaylaButonu->setText(onay ? "✓ ONAYLANDI" : "ET'YE ONAYLA");
        });

        // Karta tıklayarak "buna kilitlen" seçimi yapılabilsin (bkz. edHedefSec).
        girdi.edKarti->setProperty("edHedefId", id);
        girdi.edKarti->setCursor(Qt::PointingHandCursor);
        girdi.edKarti->installEventFilter(this);

        hedefKartlariLayout->insertWidget(hedefKartlariLayout->count() - 1, girdi.edKarti);

        girdi.haritaNoktasi = miniMapPlot->addGraph();
        girdi.haritaNoktasi->setLineStyle(QCPGraph::lsNone);
        girdi.haritaNoktasi->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, girdi.renk, 10));

        addLogEntry(QString("Yeni hedef: %1").arg(id));
    }

    const bool ilkTespit = tespitEdildi && !girdi.veri.detected;

    girdi.veri.detected = tespitEdildi;
    girdi.veri.latitude = lat;
    girdi.veri.longitude = lon;
    girdi.veri.altitude = alt;
    girdi.veri.frequency = freqMhz;
    girdi.veri.signalPower = powerDbm;
    girdi.veri.bandwidthKHz = bandwidthKHz;
    girdi.sonGorulmeMs = calismaSuresi.elapsed();
    // Tekrar tespit edilince, daha önce pasif diye gizlenmiş olsa bile
    // ana listede hemen geri görünür olur (bkz. hedefTazelikKontrolu).
    if (girdi.edKarti) {
        girdi.edKarti->setVisible(true);
    }

    if (ilkTespit) {
        addLogEntry(QString("%1 TESPİT EDİLDİ").arg(id));
    }

    // ED kartı: {Durum(0), Frekans(1), Bant Gen.(2), Güç(3),
    // Analog/Sayısal(4), Modülasyon(5), Enlem(6), Boylam(7), İrtifa(8),
    // Frekans Sapması(9), Gürültü Tabanı(10), SNR(11), Süreklilik(12)}
    girdi.edDegerLabellari[0]->setText(tespitEdildi ? "TESPİT" : "-");
    girdi.edDegerLabellari[0]->setStyleSheet(tespitEdildi ? "font-size:12px; color:#00ff66;" : "font-size:12px; color:#666;");
    girdi.edDegerLabellari[1]->setText(QString::number(freqMhz, 'f', 3) + " MHz");
    girdi.edDegerLabellari[2]->setText(QString::number(bandwidthKHz, 'f', 1) + " kHz");
    // NOT: streamer.py'nin gönderdiği güç KALİBRE EDİLMEMİŞ (ham FFT
    // genliğinden göreceli dB, gerçek bir referansa göre değil) -- bu yüzden
    // "dBm" değil "dB" gösteriliyor. Gerçek kalibrasyon (anten kazancı, kablo
    // kaybı ölçülüp hesaba katılırsa) eklenirse burası "dBm"e çevrilebilir.
    girdi.edDegerLabellari[3]->setText(QString::number(powerDbm, 'f', 1) + " dB");
    // 4 (Analog/Sayısal) ve 5 (Modülasyon) burada güncellenmiyor -- bkz. hedefYapayZekaGuncelle().
    girdi.edDegerLabellari[6]->setText(std::isnan(lat) ? "nan" : QString::number(lat, 'f', 5));
    girdi.edDegerLabellari[7]->setText(std::isnan(lon) ? "nan" : QString::number(lon, 'f', 5));
    girdi.edDegerLabellari[8]->setText(QString::number(alt, 'f', 1) + " m");
    girdi.edDegerLabellari[9]->setText(std::isnan(sapmaMhz) ? "nan" : QString::number(sapmaMhz * 1000.0, 'f', 1) + " kHz");
    girdi.edDegerLabellari[10]->setText(std::isnan(gurultuTabaniDb) ? "nan" : QString::number(gurultuTabaniDb, 'f', 1) + " dB");
    girdi.edDegerLabellari[11]->setText(std::isnan(snrDb) ? "nan" : QString::number(snrDb, 'f', 1) + " dB");
    girdi.edDegerLabellari[12]->setText(sureklilik);

    // ET kartı sadece operatör onaylayınca oluşur; onaylanmamış hedeflerde
    // etDegerLabellari boş olduğundan önce kontrol et (bkz. segfault notu).
    if (girdi.etKarti && girdi.etDegerLabellari.size() >= 2) {
        girdi.etDegerLabellari[0]->setText(QString::number(freqMhz, 'f', 3) + " MHz");
        girdi.etDegerLabellari[1]->setText(QString::number(powerDbm, 'f', 1) + " dB"); // bkz. yukarıdaki NOT -- kalibre edilmemiş
    }

    if (!std::isnan(lat) && !std::isnan(lon)) {
        girdi.haritaNoktasi->setData(QVector<double>{lon}, QVector<double>{lat});
        miniMapPlot->replot();
    }

    // Sayaç ve görünürlük tek yerden (hedefTazelikKontrolu) yönetiliyor --
    // burada anlık çağırmak, yeni paket geldiğinde 1sn'lik zamanlayıcıyı
    // beklemeden listeyi/sayacı hemen güncel tutar.
    hedefTazelikKontrolu();

    // NOT: görünüm penceresi burada değil, specGuncelle() içinde (gelen SPEC
    // paketinin sabit bant merkez/genişliğine göre) kuruluyor -- tek yetkili
    // orası, aksi halde iki farklı kaynak birbirini "değişti" sayıp pencereyi
    // titretebiliyordu (bkz. specGuncelle'deki NOT).
    spektrumVeSelaleGuncelle();
}

// SYS'ten tamamen bağımsız: yapay zeka sınıflandırma modülü entegre olunca
// bu ayrı "AI" paketiyle sadece analog/sayısal ve modülasyon türü sütunları
// güncellenecek. Hedef henüz SYS ile açılmadıysa (frekans/güç bilgisi yoksa)
// bu paket yok sayılır -- tek başına anlamlı bir satır oluşturamaz.
void MainWindow::hedefYapayZekaGuncelle(const QString &id, const QString &analogSayisal, const QString &modulasyonTuru)
{
    if (!hedefler.contains(id)) {
        qDebug() << "AI paketi bilinmeyen hedef için geldi, yok sayıldı:" << id;
        return;
    }

    HedefGirdisi &girdi = hedefler[id];
    girdi.veri.analogSayisal = analogSayisal;
    girdi.veri.modulasyonTuru = modulasyonTuru;

    // ED kartında sıra: bkz. hedefGuncelle() -- 4=Analog/Sayısal, 5=Modülasyon.
    girdi.edDegerLabellari[4]->setText(analogSayisal);
    girdi.edDegerLabellari[5]->setText(modulasyonTuru);
}

QString MainWindow::dfYontemMetni(const QString &yontemKodu)
{
    static const QMap<QString, QString> metinler = {
        {"IHA_GENLIK", "İHA (Genlik Tabanlı)"},
        {"YER_YAGI", "Yer İstasyonu (Yagi)"},
    };
    return metinler.value(yontemKodu, yontemKodu);
}

// Madde 5.1.4 + 5.1.5: yön bulma açısı + konum kestirimi tek pakette gelir
// (bkz. parseLine, "DF" tipi). İki ayrı yeri günceller:
//   1) Global "YÖN VE KONUM BULMA" paneli -- her zaman EN SON gelen DF
//      sonucunu gösterir (seçili hedefe bağlı değil; DİNLE'nin hedef
//      seçiminden farklı bir amaca hizmet ediyor).
//   2) İlgili hedefin kartındaki Enlem/Boylam alanları + harita noktası --
//      SYS paketinin (İHA'nın kendi GPS'i/nan) yerine gerçek konum
//      kestirimini yansıtır. hedefId hiç SYS ile açılmadıysa (kart yoksa)
//      bu adım atlanır.
void MainWindow::dfSonucuGuncelle(const QString &hedefId, const QString &yontemKodu,
                                   double aciDeg, double rmsDerece, double lat, double lon)
{
    const QString yontemMetni = dfYontemMetni(yontemKodu);

    yonAcisiLabel->setText(QString("Açı: %1 °").arg(aciDeg, 0, 'f', 1));
    yonYontemiLabel->setText(QString("Yöntem: %1 (%2)").arg(yontemMetni, hedefId));
    rmsHataLabel->setText(QString("RMS: %1 °").arg(rmsDerece, 0, 'f', 1));

    if (!std::isnan(lat) && !std::isnan(lon)) {
        konumEnlemLabel->setText(QString("Enlem: %1").arg(lat, 0, 'f', 5));
        konumBoylamLabel->setText(QString("Boylam: %1").arg(lon, 0, 'f', 5));
    }

    if (!hedefler.contains(hedefId)) {
        qDebug() << "DF paketi bilinmeyen hedef için geldi, sadece global panel güncellendi:" << hedefId;
        return;
    }

    HedefGirdisi &girdi = hedefler[hedefId];
    girdi.sonGorulmeMs = calismaSuresi.elapsed();

    if (!std::isnan(lat) && !std::isnan(lon)) {
        // ED kartı: {..., Enlem(6), Boylam(7), ...} -- bkz. hedefGuncelle().
        // Artık İHA'nın kendi GPS'i/nan değil, DF/konum kestiriminin
        // ürettiği gerçek hedef konumu yazılıyor.
        girdi.veri.latitude = lat;
        girdi.veri.longitude = lon;
        girdi.edDegerLabellari[6]->setText(QString::number(lat, 'f', 5));
        girdi.edDegerLabellari[7]->setText(QString::number(lon, 'f', 5));

        if (girdi.haritaNoktasi) {
            girdi.haritaNoktasi->setData(QVector<double>{lon}, QVector<double>{lat});
            miniMapPlot->replot();
        }
    }
}

// ================= ET HEDEF SEÇME (onaylı adaylar arasından) =================
// ED kartındaki "ET'YE ONAYLA" (bkz. hedefGuncelle) bir hedefi ET ADAYI
// yapar; bu fonksiyon ise onaylı adaylardan HANGİSİNİN aktif karıştırma/
// aldatma hedefi olacağını belirler -- ET kartına tıklanınca çağrılır
// (bkz. eventFilter, property "etHedefId").
void MainWindow::etHedefSec(const QString &id)
{
    if (!hedefler.contains(id)) {
        return;
    }

    const QString tip = (karistirmaTipiGrubu && karistirmaTipiGrubu->checkedButton())
                             ? karistirmaTipiGrubu->checkedButton()->text() : "TEKLİ";

    if (tip == "ÇOKLU") {
        // ÇOKLU profilinde tıklama TEK seçim değil, EKLE/ÇIKAR (en fazla 3
        // hedef, bkz. et_control.py generate_multi_target_noise).
        HedefGirdisi &girdi = hedefler[id];
        if (etCokluSeciliHedefler.contains(id)) {
            etCokluSeciliHedefler.removeAll(id);
            if (girdi.etKarti)
                girdi.etKarti->setStyleSheet("");
            addLogEntry(QString("Çoklu karıştırma seçiminden çıkarıldı: %1 (%2/3)")
                            .arg(id).arg(etCokluSeciliHedefler.size()));
        } else {
            if (etCokluSeciliHedefler.size() >= 3) {
                addLogEntry("[ET] Çoklu karıştırma en fazla 3 hedef destekler -- önce birini çıkarın.");
                return;
            }
            etCokluSeciliHedefler.append(id);
            if (girdi.etKarti)
                girdi.etKarti->setStyleSheet(QString("QFrame#infoBox { border: 2px solid %1; }").arg(girdi.renk.name()));
            addLogEntry(QString("Çoklu karıştırma seçimine eklendi: %1 (%2/3)")
                            .arg(id).arg(etCokluSeciliHedefler.size()));
        }
        return;
    }

    // TEKLİ / BARAJ: eskisi gibi tekil, dışlayıcı seçim.
    if (!etSeciliHedefId.isEmpty() && hedefler.contains(etSeciliHedefId) && hedefler[etSeciliHedefId].etKarti) {
        hedefler[etSeciliHedefId].etKarti->setStyleSheet("");
    }

    if (id == etSeciliHedefId) {
        // Aynı karta tekrar tıklama -- seçimi kaldır.
        etSeciliHedefId.clear();
        return;
    }

    etSeciliHedefId = id;
    HedefGirdisi &girdi = hedefler[id];
    if (girdi.etKarti) {
        girdi.etKarti->setStyleSheet(QString("QFrame#infoBox { border: 2px solid %1; }").arg(girdi.renk.name()));
    }
    addLogEntry(QString("ET hedef seçildi: %1 (%2 MHz)").arg(id).arg(girdi.veri.frequency, 0, 'f', 3));
}

// ================= ED HEDEF SEÇME (backend'e "buna kilitlen" bildirimi) =================
// ED kartına tıklanınca çağrılır -- id önekine göre doğru backend'e komutu
// yönlendirir: "HEDEF-" ile başlıyorsa streamer.py'ye (RTL-SDR, 144/433 MHz),
// "PHEDEF-" ile başlıyorsa pluto_ed_scanner.py'ye (Pluto RX, 868-870/2.4GHz)
// -- ikisi de aynı komut kanalını (port 5557) paylaşıyor, sadece önek farklı
// (bkz. src/streamer.py ve src/pluto_ed_scanner.py'deki "HEDEF_SEC|"/
// "PLUTO_ED_HEDEF_SEC|" işleyicileri). Aynı karta tekrar tıklamak seçimi
// kaldırıp backend'i otomatik (en son bulunan hedef) moduna döndürür.
void MainWindow::edHedefSec(const QString &id)
{
    if (!hedefler.contains(id)) {
        return;
    }

    if (!edSeciliHedefId.isEmpty() && hedefler.contains(edSeciliHedefId) && hedefler[edSeciliHedefId].edKarti) {
        hedefler[edSeciliHedefId].edKarti->setStyleSheet("");
    }

    if (id == edSeciliHedefId) {
        // Aynı karta tekrar tıklama -- seçimi kaldır, backend'i otomatik moda döndür.
        // DİNLE bu hedef için aktifse önce onu düzgün durdur -- aksi halde
        // backend arka planda demodüle etmeye devam ediyor ama GUI'nin
        // DİNLE düğmesi/"Demodülasyon" etiketi bunu hiç bilmiyor, ikisi
        // senkronsuz kalıyordu (sahada gözlemlendi).
        if (dinleButonu->isChecked()) {
            dinleButonu->blockSignals(true);
            dinleButonu->setChecked(false);
            dinleButonu->setText("DİNLE");
            dinleButonu->blockSignals(false);
            demodulasyonDurumuLabel->setText("Demodülasyon: --");
            if (mZmqPublisher)
                mZmqPublisher->sendLine("DINLE_DURDUR");
            if (mAiRequestTimer)
                mAiRequestTimer->stop();
            addLogEntry("Sinyal dinleme durduruldu (hedef seçimi kaldırıldığı için)");
        }
        edSeciliHedefId.clear();
        mZmqPublisher->sendLine("HEDEF_SEC|OTOMATIK");
        mZmqPublisher->sendLine("PLUTO_ED_HEDEF_SEC|OTOMATIK");
        addLogEntry("ED hedef seçimi kaldırıldı, otomatik moda dönüldü.");
        return;
    }

    edSeciliHedefId = id;
    HedefGirdisi &girdi = hedefler[id];
    if (girdi.edKarti) {
        girdi.edKarti->setStyleSheet(QString("QFrame#infoBox { border: 2px solid %1; }").arg(girdi.renk.name()));
    }

    if (id.startsWith("PHEDEF-")) {
        mZmqPublisher->sendLine(QString("PLUTO_ED_HEDEF_SEC|%1").arg(id));
    } else {
        mZmqPublisher->sendLine(QString("HEDEF_SEC|%1").arg(id));
    }
    addLogEntry(QString("ED hedef seçildi: %1 (%2 MHz)").arg(id).arg(girdi.veri.frequency, 0, 'f', 3));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const QVariant etHedefId = watched->property("etHedefId");
        if (etHedefId.isValid()) {
            etHedefSec(etHedefId.toString());
            return true;
        }
        const QVariant edHedefId = watched->property("edHedefId");
        if (edHedefId.isValid()) {
            edHedefSec(edHedefId.toString());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::hedefTazelikKontrolu()
{
    const qint64 simdi = calismaSuresi.elapsed();
    bool haritaDegisti = false;
    int aktifSayisi = 0;

    for (auto it = hedefler.begin(); it != hedefler.end(); ++it) {
        HedefGirdisi &girdi = it.value();
        if (!girdi.edKarti) continue;

        const bool pasif = (simdi - girdi.sonGorulmeMs) > HEDEF_PASIF_ESIK_MS;
        if (!pasif) {
            ++aktifSayisi;
        }
        // Pasif hedefler ana ED listesinden gizlenir (silinmiyor) -- tekrar
        // tespit edilince hedefGuncelle() otomatik geri görünür yapıyor.
        // İSTİSNA: operatörün seçtiği (edSeciliHedefId -- DİNLE de bunu
        // kullanıyor) veya ET'ye onayladığı (onaylandi) hedef, pasife düşse
        // (yayın dursa) bile listeden ASLA gizlenmez -- operatör bilerek
        // ilgilendiği bir hedefin gözünün önünden gitmesini istemiyor.
        // Rengi yine de soluklaşır (pasif olduğu görsel olarak belli olsun).
        const bool korunuyor = (it.key() == edSeciliHedefId) || girdi.onaylandi;
        // ET onay kartı (etKarti) bilerek dokunulmuyor -- onun görünürlüğü
        // operatörün "ET'YE ONAYLA" seçimine bağlı, pasiflikten etkilenmemeli.
        girdi.edKarti->setVisible(!pasif || korunuyor);

        const QColor renk = pasif ? QColor("#555555") : girdi.renk;
        const QString baslikStil = QString("font-weight: bold; font-size: 12px; color: %1;").arg(renk.name());

        if (girdi.edBaslikLabel) {
            girdi.edBaslikLabel->setStyleSheet(baslikStil);
        }
        if (girdi.etKarti && girdi.etBaslikLabel) {
            girdi.etBaslikLabel->setStyleSheet(baslikStil);
        }

        if (girdi.haritaNoktasi) {
            QCPScatterStyle stil = girdi.haritaNoktasi->scatterStyle();
            stil.setBrush(renk);
            stil.setPen(QPen(renk));
            girdi.haritaNoktasi->setScatterStyle(stil);
            haritaDegisti = true;
        }
    }

    aktifHedefSayisiLabel->setText(QString("Aktif Hedef: %1").arg(aktifSayisi));

    // Tüm hedefler pasife düşünce (edKarti'ları gizlenince) liste boş
    // görünüyordu -- bosHedefMesaji ilk hedefte bir kere gizlenip bir daha
    // hiç geri gelmiyordu. Artık aktif hedef kalmadığında tekrar gösteriliyor.
    if (bosHedefMesaji) {
        bosHedefMesaji->setText(hedefler.isEmpty() ? "Henüz hedef tespit edilmedi."
                                                    : "Şu an aktif hedef yok.");
        bosHedefMesaji->setVisible(aktifSayisi == 0);
    }

    if (haritaDegisti) {
        miniMapPlot->replot();
    }
}

// ================= ZEROMQ HABERLEŞME =================
// Eskiden 915 MHz telemetri radyosundan (seri port) gelen SYS/AI/SPEC/UAV
// satırları artık ZeroMQ üzerinden geliyor:
//   - streamer.py  -> 5555 numaralı porttan PUB (SYS + SPEC paketleri)
//   - predict.py   -> 5556 numaralı porttan PUB (AI paketleri)
//   - bu arayüz    -> 5557 numaralı portta PUB bind eder (JAM_START/STOP,
//                     SDR_VERISI_ISTEK komutları); her iki Python betiği de
//                     buraya SUB olarak bağlanıyor.
// Varsayılan olarak hepsi 127.0.0.1 (loopback) üzerinde -- tek makine
// üzerindeki proses-arası haberleşme için yeterli izolasyonu sağlıyor.
// Backend (streamer.py/et_control.py) başka bir makinede (örn. Jetson)
// çalışıyorsa EBABIL_JETSON_IP ortam değişkenini o makinenin LAN IP'sine
// ayarla -- SUB uçları oraya bağlanır, PUB ucu da 0.0.0.0'a bind edilip
// o makinenin buraya bağlanabilmesine izin verir. Değişken yoksa davranış
// eskisiyle birebir aynı (127.0.0.1).
// Satır formatı (parseLine) değişmedi, sadece taşıma katmanı değişti.
void MainWindow::setupZmqConnections()
{
    const QString backendHost = qEnvironmentVariable("EBABIL_JETSON_IP", "127.0.0.1");
    const QString bindHost = (backendHost == "127.0.0.1") ? QStringLiteral("127.0.0.1") : QStringLiteral("0.0.0.0");

    mZmqSubscriber = new ZmqSubscriber(
        // 5555: SYS/SPEC (streamer.py) | 5556: AI (streamer.py) | 5559: UAV
        // (mavlink_bridge.py, ayrı port -- streamer.py zaten 5555'i kendi
        // açtığı için Matek telemetrisi ayrı bir PUB soketinden geliyor) |
        // 5560: SYS/SPEC (pluto_ed_scanner.py, 868-870/2.4GHz -- id'leri
        // "PHEDEF-" önekiyle geldiği için streamer.py'nin "HEDEF-"leriyle
        // karışmaz, parseLine/hedefGuncelle format olarak zaten ortak).
        // NOT: SPEC paketleri kaynak etiketi taşımıyor -- iki tarayıcı da
        // aynı anda çalışırsa şelale, ikisinin merkez frekansı arasında
        // sıçrayabilir. Şimdilik bilinen bir sınırlama, ayrı görünüm gerekirse
        // sonra eklenir.
        QStringList{
            QString("tcp://%1:5555").arg(backendHost),
            QString("tcp://%1:5556").arg(backendHost),
            QString("tcp://%1:5559").arg(backendHost),
            QString("tcp://%1:5560").arg(backendHost),
        },
        this);
    connect(mZmqSubscriber, &ZmqSubscriber::lineReceived, this, &MainWindow::handleZmqLine);
    mZmqSubscriber->start();

    mZmqPublisher = new ZmqCommandPublisher(QString("tcp://%1:5557").arg(bindHost), this);

    mAiRequestTimer = new QTimer(this);
    mAiRequestTimer->setInterval(1000);
    connect(mAiRequestTimer, &QTimer::timeout, this, [this]() {
        mZmqPublisher->sendLine("SDR_VERISI_ISTEK");
    });
}

void MainWindow::handleZmqLine(const QString &line)
{
    sonZmqMesajMs = calismaSuresi.elapsed();
    if (!mZmqConnected) {
        setConnectionStatus(true);
        addLogEntry("ZeroMQ bağlantısı kuruldu, veri alınıyor.");
    }
    parseLine(line);
}

void MainWindow::baglantiTazelikKontrolu()
{
    if (!mZmqConnected) {
        return; // henüz hiç bağlanmadı, kopma tespiti anlamsız
    }
    if (calismaSuresi.elapsed() - sonZmqMesajMs > BAGLANTI_KOPMA_ESIK_MS) {
        setConnectionStatus(false);
        addLogEntry("ZeroMQ bağlantısı kesildi -- backend'den veri gelmiyor.");
    }
}

// ================= PAKET AYRIŞTIRMA =================
void MainWindow::parseLine(const QString &line)
{
    QStringList parts = line.split(',');
    if (parts.isEmpty()) return;

    if (parts[0] == "SYS") {
        // parts[8] = bandwidthKHz, parts[9..12] = sapma/gürültü/SNR/süreklilik
        // -- KTR Tablo 8'in geri kalanı (bkz. sdr_common.py/streamer.py
        // build_sys_fields). Eski 9 alanlık format artık YOK, hep 13 bekleniyor.
        if (parts.size() != 13) {
            qDebug() << "Geçersiz SYS paketi:" << line;
            return;
        }

        // parts[1] artık sabit bir slot numarası değil, hedefi tanımlayan
        // bir kimlik (ör. bant adı) -- bkz. TelemetriGonderici.h.
        const QString id = parts[1];
        const bool tespitEdildi = (parts[2].toInt() == 1);
        const double lat = parts[3].toDouble();
        const double lon = parts[4].toDouble();
        const double alt = parts[5].toDouble();
        const double freqMhz = parts[6].toDouble();
        const double powerDbm = parts[7].toDouble();
        const double bandwidthKHz = parts[8].toDouble();
        const double sapmaMhz = parts[9].toDouble();
        const double gurultuTabaniDb = parts[10].toDouble();
        const double snrDb = parts[11].toDouble();
        const QString sureklilik = parts[12];

        hedefGuncelle(id, tespitEdildi, lat, lon, alt, freqMhz, powerDbm, bandwidthKHz,
                      sapmaMhz, gurultuTabaniDb, snrDb, sureklilik);

    } else if (parts[0] == "AI") {
        // Yapay zeka sınıflandırma modülünden gelen ayrı paket -- SYS'ten
        // bağımsız olarak, ayrı bir bilgisayarda entegre edilecek.
        // Format: AI,id,analogSayisal,modulasyonTuru
        if (parts.size() != 4) {
            qDebug() << "Geçersiz AI paketi:" << line;
            return;
        }

        const QString id = parts[1];
        const QString analogSayisal = parts[2];
        const QString modulasyonTuru = parts[3];

        hedefYapayZekaGuncelle(id, analogSayisal, modulasyonTuru);

    } else if (parts[0] == "SPEC") {
        // ebabil_sdr'ın o anki tarama adımının TAM spektrum snapshot'ı --
        // tespit eşiğinden bağımsız, her adımda gönderilir. SYS'ten farklı
        // olarak hedef değil, waterfall'ın sürekli akmasını sağlayan ham
        // veri (bkz. specGuncelle). Format: SPEC,merkez_mhz,fs_mhz,64 değer
        // -> toplam 3+64=67 alan.
        if (parts.size() != 67) {
            qDebug() << "Geçersiz SPEC paketi:" << line;
            return;
        }

        const double merkezMhz = parts[1].toDouble();
        const double fsMhz = parts[2].toDouble();
        QVector<double> ornekler;
        ornekler.reserve(64);
        for (int i = 0; i < 64; ++i) {
            ornekler.append(parts[3 + i].toDouble());
        }

        specGuncelle(merkezMhz, fsMhz, ornekler);

    } else if (parts[0] == "UAV") {
        if (parts.size() != 9) {
            qDebug() << "Geçersiz UAV paketi:" << line;
            return;
        }

        uavData.latitude  = parts[1].toDouble();
        uavData.longitude = parts[2].toDouble();
        uavData.altitude  = parts[3].toDouble();
        uavData.speed     = parts[4].toDouble();
        uavData.heading   = parts[5].toDouble();
        uavData.pitch     = parts[6].toDouble();
        uavData.roll      = parts[7].toDouble();
        uavData.battery   = parts[8].toDouble();

        speedLabel->setText(QString("%1 m/s").arg(uavData.speed, 0, 'f', 1));
        headingLabel->setText(QString("%1 °").arg(uavData.heading, 0, 'f', 0));
        altitudeLabel->setText(QString("%1 m").arg(uavData.altitude, 0, 'f', 1));
        batteryLabel->setText(QString("%1 %").arg(uavData.battery, 0, 'f', 0));
        pitchLabel->setText(QString("%1 °").arg(uavData.pitch, 0, 'f', 1));
        rollLabel->setText(QString("%1 °").arg(uavData.roll, 0, 'f', 1));

        updateUavRotation();

        // Mini haritada İHA noktasını güncelle
        uavMapPoint->setData(
            QVector<double>{uavData.longitude},
            QVector<double>{uavData.latitude}
            );
        miniMapPlot->replot();

    } else if (parts[0] == "DF") {
        // Yön bulma + konum kestirimi sonucu (madde 5.1.4 + 5.1.5) --
        // yonKonum tarafında hesaplanıp buraya iletilir. Format:
        // DF,<hedef_id>,<yontem_kodu>,<aci_deg>,<rms_derece>,<lat>,<lon>
        // yontem_kodu: "IHA_GENLIK" | "YER_YAGI" (bkz. dfYontemMetni).
        // Konum kestirimi henüz yoksa (sadece DF açısı hesaplanmışsa) lat/lon
        // "nan" gönderilebilir -- bu durumda Enlem/Boylam güncellenmez,
        // önceki değer (varsa) korunur.
        if (parts.size() != 7) {
            qDebug() << "Geçersiz DF paketi:" << line;
            return;
        }

        const QString hedefId = parts[1];
        const QString yontemKodu = parts[2];
        const double aciDeg = parts[3].toDouble();
        const double rmsDerece = parts[4].toDouble();
        const double lat = parts[5].toDouble();
        const double lon = parts[6].toDouble();

        dfSonucuGuncelle(hedefId, yontemKodu, aciDeg, rmsDerece, lat, lon);

    } else if (parts[0] == "SAYISAL") {
        // Sayısal amatör telsiz decode sonucu (madde 5.1.3'ün opsiyonel/ek
        // puan kısmı) -- multimon-ng'nin çözdüğü AX.25/APRS metni. Format:
        // SAYISAL,<hedef_id>,<metin>. metin İÇİNDE VİRGÜL OLABİLİR, o yüzden
        // section(',',2) ile 2. virgülden sonraki HER ŞEY tek parça alınır.
        if (parts.size() < 3) {
            qDebug() << "Geçersiz SAYISAL paketi:" << line;
            return;
        }

        const QString hedefId = parts[1];
        const QString metin = line.section(',', 2);

        addLogEntry(QString("[Sayısal/%1] %2").arg(hedefId, metin));

    } else if (parts[0] == "DURUM") {
        // Gerçek dinleme-modu durumu (madde 5.1.3) -- dinleButonu'nun
        // tıklanır tıklanmaz yaptığı iyimser/yerel güncellemenin, donanımdan
        // gelen GERÇEK teyidi. Format: DURUM,DINLEME_AKTIF,<freq_mhz>  ya da
        // DURUM,TARIYOR. <freq_mhz> -- Hz değil, SYS/SPEC ile tutarlı MHz.
        if (parts.size() < 2) {
            qDebug() << "Geçersiz DURUM paketi:" << line;
            return;
        }
        if (parts[1] == "DINLEME_AKTIF" && parts.size() == 3) {
            const double freqMhz = parts[2].toDouble();
            demodulasyonDurumuLabel->setText(
                QString("Demodülasyon: AKTİF (%1 MHz)").arg(freqMhz, 0, 'f', 3));
            addLogEntry(QString("Dinleme modu AKTİF onaylandı: %1 MHz").arg(freqMhz, 0, 'f', 3));
        } else if (parts[1] == "TARIYOR") {
            demodulasyonDurumuLabel->setText("Demodülasyon: --");
            if (dinleButonu->isChecked()) {
                // Donanım kendiliğinden (ör. hata sonrası) tarama moduna
                // dönmüş olabilir -- butonu da gerçek duruma senkronla.
                // blockSignals: bu programatik değişikliğin
                // dinlemeDurumuDegisti'yi tekrar tetiklemesini önler.
                dinleButonu->blockSignals(true);
                dinleButonu->setChecked(false);
                dinleButonu->setText("DİNLE");
                dinleButonu->blockSignals(false);
            }
            addLogEntry("Tarama moduna dönüldü (donanım onayı)");
        } else {
            qDebug() << "Bilinmeyen DURUM alt tipi:" << line;
        }

    } else {
        qDebug() << "Bilinmeyen paket tipi:" << line;
    }
}
