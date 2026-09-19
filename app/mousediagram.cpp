#include "mousediagram.h"
#include "theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

MouseDiagram::MouseDiagram(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(460, 340);
    m_assign[Left]    = QStringLiteral("Primary Click");
    m_assign[Right]   = QStringLiteral("Secondary Click");
    m_assign[Middle]  = QStringLiteral("Middle Click");
    m_assign[Forward] = QStringLiteral("Forward");
    m_assign[Back]    = QStringLiteral("Back");
    m_assign[Dpi]     = QStringLiteral("DPI Cycle");
}

void MouseDiagram::setAssignment(Button b, const QString &text)
{
    m_assign[b] = text;
    update();
}

void MouseDiagram::setLedColor(uint32_t rgb)
{
    m_led = rgb;
    update();
}

// The body is sized to fit the shorter axis so the shell never runs off the
// widget, and every hotspot is a fraction of that box.
QRectF MouseDiagram::bodyRect() const
{
    constexpr qreal kAspect = 1.55;          // height / width
    // Leave room either side for the callout lines and their labels.
    const qreal maxW = width() * 0.26;
    const qreal maxH = height() * 0.74;
    qreal bw = maxW;
    if (bw * kAspect > maxH)
        bw = maxH / kAspect;
    const qreal bh = bw * kAspect;
    return QRectF((width() - bw) / 2.0, (height() - bh) / 2.0 + height() * 0.02, bw, bh);
}

QHash<int, MouseDiagram::Hotspot> MouseDiagram::spots() const
{
    QHash<int, Hotspot> s;
    s[Middle]  = { QPointF(0.50, 0.09), Up,    m_assign.value(Middle) };
    s[Left]    = { QPointF(0.26, 0.20), LeftS, m_assign.value(Left) };
    s[Right]   = { QPointF(0.74, 0.20), RightS,m_assign.value(Right) };
    s[Dpi]     = { QPointF(0.50, 0.41), RightS,m_assign.value(Dpi) };
    s[Forward] = { QPointF(0.03, 0.30), LeftS, m_assign.value(Forward) };
    s[Back]    = { QPointF(0.03, 0.40), LeftS, m_assign.value(Back) };
    return s;
}

void MouseDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF b = bodyRect();
    const qreal bw = b.width(), bh = b.height();
    const qreal cx = b.center().x();

    // ---- shell ----
    QPainterPath shell;
    shell.moveTo(cx - bw * 0.22, b.top());
    shell.cubicTo(cx - bw * 0.44, b.top() + bh * 0.01,
                  cx - bw * 0.50, b.top() + bh * 0.22,
                  cx - bw * 0.50, b.top() + bh * 0.52);
    shell.cubicTo(cx - bw * 0.50, b.top() + bh * 0.85,
                  cx - bw * 0.30, b.bottom(),
                  cx,             b.bottom());
    shell.cubicTo(cx + bw * 0.30, b.bottom(),
                  cx + bw * 0.50, b.top() + bh * 0.85,
                  cx + bw * 0.50, b.top() + bh * 0.52);
    shell.cubicTo(cx + bw * 0.50, b.top() + bh * 0.22,
                  cx + bw * 0.44, b.top() + bh * 0.01,
                  cx + bw * 0.22, b.top());
    shell.cubicTo(cx + bw * 0.10, b.top() - bh * 0.012,
                  cx - bw * 0.10, b.top() - bh * 0.012,
                  cx - bw * 0.22, b.top());
    shell.closeSubpath();

    QLinearGradient g(b.topLeft(), b.bottomRight());
    g.setColorAt(0.0, QColor("#2a313b"));
    g.setColorAt(1.0, QColor("#141820"));
    p.fillPath(shell, g);
    p.setPen(QPen(QColor(theme::Line), 1.6));
    p.drawPath(shell);

    // ---- button split ----
    const qreal splitY = b.top() + bh * 0.34;
    p.setPen(QPen(QColor(theme::Line), 1.3));
    p.drawLine(QPointF(cx - bw * 0.47, splitY), QPointF(cx + bw * 0.47, splitY));
    p.drawLine(QPointF(cx, b.top() + bh * 0.16), QPointF(cx, splitY));

    // ---- scroll wheel ----
    const QRectF wheel(cx - bw * 0.06, b.top() + bh * 0.04, bw * 0.12, bh * 0.12);
    QPainterPath wp;
    wp.addRoundedRect(wheel, wheel.width() / 2.0, wheel.width() / 2.0);
    p.fillPath(wp, QColor("#0c1014"));
    p.setPen(QPen(QColor("#4a5563"), 1.4));
    p.drawPath(wp);

    // ---- side buttons ----
    for (qreal fy : {0.28, 0.38}) {
        const QRectF sb(b.left() - bw * 0.04, b.top() + bh * fy, bw * 0.055, bh * 0.065);
        QPainterPath sp;
        sp.addRoundedRect(sb, 2.5, 2.5);
        p.fillPath(sp, QColor("#1b2129"));
        p.setPen(QPen(QColor(theme::Line), 1.1));
        p.drawPath(sp);
    }

    // ---- DPI button ----
    {
        const QRectF db(cx - bw * 0.085, b.top() + bh * 0.375, bw * 0.17, bh * 0.055);
        QPainterPath dp;
        dp.addRoundedRect(db, 3, 3);
        p.fillPath(dp, QColor("#1b2129"));
        p.setPen(QPen(QColor(theme::Line), 1.1));
        p.drawPath(dp);
    }

    // ---- active gear LED ----
    if (m_led != 0xFFFFFFFFu) {
        const QRectF led(cx - bw * 0.08, b.top() + bh * 0.545, bw * 0.16, bh * 0.017);
        QPainterPath lp;
        lp.addRoundedRect(led, 2.5, 2.5);
        const QColor c = QColor::fromRgb(static_cast<QRgb>(m_led));
        QColor halo = c;
        halo.setAlpha(55);
        p.setPen(QPen(halo, 6));
        p.drawPath(lp);
        p.setPen(Qt::NoPen);
        p.fillPath(lp, c);
    }

    // ---- callouts ----
    const QFont lf = theme::scaled(font(), 1, true);
    p.setFont(lf);
    const QFontMetrics fm(lf);
    const int textH = fm.height();

    const auto sp = spots();
    for (auto it = sp.constBegin(); it != sp.constEnd(); ++it) {
        const Hotspot &h = it.value();
        const QPointF at(b.left() + bw * h.at.x(), b.top() + bh * h.at.y());

        const bool isDpi = (it.key() == Dpi);
        const QColor lineCol(isDpi ? theme::Accent : QColor("#55606e"));
        const QColor textCol(isDpi ? theme::Accent : QColor(theme::Text));
        p.setPen(QPen(lineCol, 1.3));

        const QString text = h.label;
        const int tw = fm.horizontalAdvance(text);

        if (h.dir == Up) {
            const qreal endY = qMax(qreal(textH + 10), b.top() - height() * 0.10);
            p.drawLine(at, QPointF(at.x(), endY));
            p.setPen(textCol);
            p.drawText(QRectF(at.x() - tw / 2.0, endY - textH - 6, tw, textH),
                       Qt::AlignCenter, text);
        } else {
            const bool leftSide = (h.dir == LeftS);
            // Stop the line short of the edge so the label always has room.
            qreal endX = leftSide ? b.left() - bw * 0.85 : b.right() + bw * 0.85;
            endX = leftSide ? qMax(endX, qreal(tw + 10))
                            : qMin(endX, qreal(width() - tw - 10));
            p.drawLine(at, QPointF(endX, at.y()));
            p.setPen(textCol);
            const qreal tx = leftSide ? endX - tw : endX;
            p.drawText(QRectF(tx, at.y() - textH - 5, tw, textH),
                       Qt::AlignVCenter | (leftSide ? Qt::AlignRight : Qt::AlignLeft),
                       text);
        }

        p.setPen(QPen(lineCol, 1.6));
        p.setBrush(QColor(theme::Bg));
        p.drawEllipse(at, 4.0, 4.0);
        p.setBrush(Qt::NoBrush);
    }
}

void MouseDiagram::mousePressEvent(QMouseEvent *e)
{
    const QRectF b = bodyRect();
    const auto sp = spots();
    for (auto it = sp.constBegin(); it != sp.constEnd(); ++it) {
        const QPointF at(b.left() + b.width() * it.value().at.x(),
                         b.top() + b.height() * it.value().at.y());
        if (QLineF(at, e->position()).length() <= 14.0) {
            emit buttonClicked(static_cast<Button>(it.key()));
            return;
        }
    }
}
