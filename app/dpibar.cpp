#include "dpibar.h"
#include "theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kChipW   = 112;
constexpr int kChipH   = 54;
constexpr int kGap     = 10;
constexpr int kSwatchW = 10;
}

DpiBar::DpiBar(QWidget *parent) : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(kChipH + 4);
}

QSize DpiBar::sizeHint() const
{
    const int n = qMax(1, m_dpi.size());
    return QSize(n * kChipW + (n - 1) * kGap, kChipH + 4);
}

QRect DpiBar::chipRect(int i) const
{
    return QRect(i * (kChipW + kGap), 2, kChipW, kChipH);
}

QRect DpiBar::swatchRect(int i) const
{
    const QRect c = chipRect(i);
    return QRect(c.left() + 1, c.top() + 1, kSwatchW, c.height() - 2);
}

void DpiBar::setStages(const QList<int> &dpi, const QList<uint32_t> &colors,
                       int activeStage)
{
    m_dpi = dpi;
    m_colors = colors;
    m_active = activeStage;
    if (m_selected >= m_dpi.size())
        m_selected = qMax(0, m_dpi.size() - 1);
    updateGeometry();
    update();
}

void DpiBar::setSelected(int stage)
{
    if (stage < 0 || stage >= m_dpi.size() || stage == m_selected)
        return;
    m_selected = stage;
    update();
}

void DpiBar::setStageDpi(int stage, int dpi)
{
    if (stage < 0 || stage >= m_dpi.size()) return;
    m_dpi[stage] = dpi;
    update();
}

void DpiBar::setStageColor(int stage, uint32_t rgb)
{
    if (stage < 0 || stage >= m_colors.size()) return;
    m_colors[stage] = rgb;
    update();
}

void DpiBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < m_dpi.size(); ++i) {
        const QRect r = chipRect(i);
        const bool sel = (i == m_selected);
        const bool act = (i == m_active);

        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, QColor(sel ? theme::CardHover : theme::Bg));

        // The active stage is the one the mouse is using right now; the
        // selected one is just what you are editing. They are different things
        // so they get different cues, an outline and a fill.
        QPen pen(QColor(act ? theme::Accent : theme::Line));
        pen.setWidth(act ? 2 : 1);
        p.setPen(pen);
        p.drawPath(path);

        const uint32_t rgb = m_colors.value(i, 0xFFFFFFFFu);
        if (rgb != 0xFFFFFFFFu) {
            QPainterPath sw;
            sw.addRoundedRect(swatchRect(i), 4, 4);
            p.fillPath(sw, QColor(QRgb(rgb)));
        }

        p.setPen(QColor(theme::Dim));
        p.setFont(theme::scaled(font(), -2));
        p.drawText(QRect(r.left() + kSwatchW + 10, r.top() + 7, r.width() - kSwatchW - 14, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   act ? QStringLiteral("GEAR %1  *").arg(i + 1)
                       : QStringLiteral("GEAR %1").arg(i + 1));

        p.setPen(QColor(theme::Text));
        p.setFont(theme::scaled(font(), 3, true));
        p.drawText(QRect(r.left() + kSwatchW + 10, r.top() + 22, r.width() - kSwatchW - 14, 24),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_dpi.at(i)));
    }
}

void DpiBar::mousePressEvent(QMouseEvent *e)
{
    for (int i = 0; i < m_dpi.size(); ++i) {
        if (!chipRect(i).contains(e->pos()))
            continue;
        if (swatchRect(i).adjusted(-2, -2, 4, 2).contains(e->pos())) {
            setSelected(i);
            emit stageClicked(i);
            emit colorClicked(i);
            return;
        }
        setSelected(i);
        emit stageClicked(i);
        return;
    }
}
