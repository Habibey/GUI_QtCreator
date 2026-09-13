#include "modeswitch.h"
#include <QPainter>

ModeSwitch::ModeSwitch(QWidget *parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);

    m_animasyon = new QPropertyAnimation(this, "konum", this);
    m_animasyon->setDuration(160);

    connect(this, &QAbstractButton::toggled, this, &ModeSwitch::hedefKonumaAnimasyonla);
}

QSize ModeSwitch::sizeHint() const
{
    return QSize(92, 36);
}

void ModeSwitch::setKonum(qreal konum)
{
    m_konum = konum;
    update();
}

void ModeSwitch::hedefKonumaAnimasyonla(bool checked)
{
    m_animasyon->stop();
    m_animasyon->setStartValue(m_konum);
    m_animasyon->setEndValue(checked ? 1.0 : 0.0);
    m_animasyon->start();
}

// İki renk arasında oran (0..1) kadar doğrusal geçiş.
static QColor karistir(const QColor &a, const QColor &b, qreal oran)
{
    return QColor(
        a.red()   + int((b.red()   - a.red())   * oran),
        a.green() + int((b.green() - a.green()) * oran),
        a.blue()  + int((b.blue()  - a.blue())   * oran)
        );
}

void ModeSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF trackRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = trackRect.height() / 2.0;

    // Palet, MainWindow::temaUygula() ile birebir aynı: ikisi de aynı koyu
    // temanın (#0b0f19) üzerinde, tek fark aksan rengi -- ED orijinal
    // turkuaz (#00ffcc), ET kırmızı (#ff4444) varyantı.
    const QColor trackED("#0f2a2a");   // koyu turkuaz-lacivert
    const QColor trackET("#2a1212");   // koyu kırmızı-lacivert
    const QColor kenarED("#00ffcc");
    const QColor kenarET("#ff4444");
    const QColor metinED("#eafffa");
    const QColor metinET("#ffecec");

    const QColor trackColor = karistir(trackED, trackET, m_konum);
    const QColor kenarColor = karistir(kenarED, kenarET, m_konum);
    const QColor metinColor = karistir(metinED, metinET, m_konum);

    p.setPen(QPen(kenarColor, 1.4));
    p.setBrush(trackColor);
    p.drawRoundedRect(trackRect, radius, radius);

    // Kayan daire -- ED'de lacivert, ET'de pastel turkuaz.
    const qreal handleDiameter = trackRect.height() - 8;
    const qreal handleX = trackRect.left() + 4 + m_konum * (trackRect.width() - handleDiameter - 8);
    const QRectF handleRect(handleX, trackRect.top() + 4, handleDiameter, handleDiameter);

    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? kenarET : kenarED);
    p.drawEllipse(handleRect);

    // Sabit "ED" / "ET" etiketleri -- track koyulaştıkça yazı da açılır.
    QFont f = p.font();
    f.setPointSize(9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(metinColor);
    p.drawText(trackRect.adjusted(10, 0, -trackRect.width() / 2, 0), Qt::AlignVCenter | Qt::AlignLeft, "ED");
    p.drawText(trackRect.adjusted(trackRect.width() / 2, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, "ET");
}
