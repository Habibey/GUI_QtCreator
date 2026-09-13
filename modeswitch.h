#ifndef MODESWITCH_H
#define MODESWITCH_H

#include <QAbstractButton>
#include <QPropertyAnimation>

// ED/ET arası geçiş için animasyonlu kayan toggle (iOS tarzı switch).
// checked = false  -> ED (Elektronik Destek)
// checked = true   -> ET (Elektronik Taarruz)
// QAbstractButton::toggled(bool) sinyali MainWindow'da moda göre panel
// değiştirmek için dinlenir.
class ModeSwitch : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal konum READ konum WRITE setKonum)

public:
    explicit ModeSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    qreal konum() const { return m_konum; }
    void setKonum(qreal konum);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void hedefKonumaAnimasyonla(bool checked);

private:
    qreal m_konum = 0.0; // 0.0 = ED (sol), 1.0 = ET (sağ)
    QPropertyAnimation *m_animasyon;
};

#endif // MODESWITCH_H
