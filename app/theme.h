#ifndef THEME_H
#define THEME_H

#include <QFont>
#include <QString>

// One dark palette for the whole app. Kept as plain constants rather than a
// stylesheet soup so the painted widgets (the mouse diagram, the DPI chips)
// can use exactly the same colours as the stylesheet ones.
namespace theme {

constexpr const char *Bg        = "#0f1115";   // window
constexpr const char *Rail      = "#0a0c10";   // left nav
constexpr const char *Card      = "#181c22";   // panel
constexpr const char *CardHover = "#1f242c";
constexpr const char *Line      = "#262c35";   // borders
constexpr const char *Text      = "#e6e9ed";
constexpr const char *Dim       = "#828b98";   // secondary text
constexpr const char *Accent    = "#00a8e8";   // selection, focus
constexpr const char *AccentDim = "#0b6f9c";
constexpr const char *Good      = "#3ddc84";
constexpr const char *Warn      = "#ffb020";
constexpr const char *Bad       = "#ff5c5c";

QString sheet();

// The stylesheet sets font-size in px, which leaves pointSizeF() at -1 on every
// styled widget. Painted code has to nudge whichever metric is actually set or
// Qt rejects the result and floods the log.
inline QFont scaled(const QFont &base, qreal delta, bool bold = false)
{
    QFont f = base;
    if (f.pixelSize() > 0)
        f.setPixelSize(qMax(1, int(f.pixelSize() + delta)));
    else if (f.pointSizeF() > 0)
        f.setPointSizeF(qMax(1.0, f.pointSizeF() + delta));
    f.setBold(bold);
    return f;
}

} // namespace theme

#endif // THEME_H
