#ifndef SYSTEM_H
#define SYSTEM_H

#include <QString>

struct EWSystem {
    QString name;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    bool detected = false;
    double frequency = 0.0;
    double signalPower = 0.0;
    // Frekans/güç ile aynı kaynaktan (parametre çıkarımı algoritması) gelir --
    // SYS paketinin bir parçası.
    double bandwidthKHz = 0.0;
    // Bu ikisi AYRI bir kaynaktan (yapay zeka sınıflandırma modülü) gelir --
    // ayrı bir "AI" paketiyle, SYS'ten bağımsız olarak güncellenir. Yapay
    // zeka modülü entegre edilene kadar "-" placeholder olarak kalır.
    QString analogSayisal = "-";
    QString modulasyonTuru = "-";
};

struct UAVTelemetry {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    double heading = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double battery = 0.0;
};

#endif // SYSTEM_H