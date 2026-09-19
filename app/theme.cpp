#include "theme.h"

namespace theme {

QString sheet()
{
    return QStringLiteral(R"(
* { outline: none; }

QWidget {
    background: %1;
    color: %6;
    font-family: "Inter", "Segoe UI", "Cantarell", "DejaVu Sans", sans-serif;
    font-size: 13px;
}

/* Labels and check boxes must not paint the window colour over a card. The
   blanket QWidget rule above would otherwise give every one of them an opaque
   #0f1115 rectangle. */
QLabel, QCheckBox { background: transparent; }

/* The default indicator is invisible against this background. */
QCheckBox::indicator {
    width: 19px;
    height: 19px;
    border-radius: 6px;
    border: 2px solid #3a434f;
    background: %1;
}
QCheckBox::indicator:hover   { border-color: %8; }
QCheckBox::indicator:checked { background: %8; border-color: %8; }
QCheckBox::indicator:disabled { border-color: #262c35; background: #14181e; }

/* ---- left nav rail ---- */
#rail { background: %2; border-right: 1px solid %5; }

#railBtn {
    background: transparent;
    border: none;
    border-left: 3px solid transparent;
    color: %7;
    padding: 14px 8px;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 1px;
}
#railBtn:hover  { background: %4; color: %6; }
#railBtn:checked {
    background: %4;
    color: %8;
    border-left: 3px solid %8;
}

/* ---- header ---- */
#hdrTitle   { font-size: 19px; font-weight: 600; letter-spacing: 0.5px; }
#hdrSub     { color: %7; font-size: 11px; letter-spacing: 1.5px; font-weight: 600; }
#hdrDevice  { color: %8; font-size: 12px; font-weight: 600; }

/* ---- cards ---- */
#card {
    background: %3;
    border: 1px solid %5;
    border-radius: 10px;
}
#cardTitle { font-size: 14px; font-weight: 600; }
#cardHint  { color: %7; font-size: 11px; }
#sectionTitle { font-size: 16px; font-weight: 600; letter-spacing: 0.3px; }

/* ---- segmented button rows ---- */
#segBtn {
    background: %1;
    border: 1px solid %5;
    border-radius: 7px;
    padding: 9px 16px;
    color: %7;
    font-weight: 600;
}
#segBtn:hover   { background: %4; color: %6; }
#segBtn:checked { background: %8; border-color: %8; color: #051016; }
#segBtn:disabled { color: #4a525e; border-color: #1d222a; }

/* ---- plain buttons ---- */
QPushButton {
    background: %4;
    border: 1px solid %5;
    border-radius: 7px;
    padding: 9px 18px;
    font-weight: 600;
}
QPushButton:hover:enabled { border-color: %8; color: %8; }
QPushButton:disabled { color: #4a525e; }

#primaryBtn {
    background: %8;
    border: 1px solid %8;
    color: #051016;
    padding: 11px 30px;
    font-weight: 700;
    letter-spacing: 0.5px;
}
#primaryBtn:hover:enabled { background: #22bdf5; border-color: #22bdf5; }
#primaryBtn:disabled { background: #1b2028; border-color: %5; color: #4a525e; }

/* ---- inputs ---- */
QSpinBox {
    background: %1;
    border: 1px solid %5;
    border-radius: 7px;
    padding: 8px 10px;
    selection-background-color: %8;
    selection-color: #051016;
}
QSpinBox:focus { border-color: %8; }
QSpinBox::up-button, QSpinBox::down-button { width: 16px; background: %4; border: none; }

QComboBox {
    background: %1;
    border: 1px solid %5;
    border-radius: 7px;
    padding: 8px 12px;
}
QComboBox:focus, QComboBox:on { border-color: %8; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: %3;
    border: 1px solid %5;
    selection-background-color: %8;
    selection-color: #051016;
    outline: none;
}

/* ---- sliders ---- */
QSlider::groove:horizontal {
    height: 4px;
    background: #272d36;
    border-radius: 2px;
}
QSlider::sub-page:horizontal { background: %8; border-radius: 2px; }
QSlider::handle:horizontal {
    background: %6;
    width: 15px;
    height: 15px;
    margin: -6px 0;
    border-radius: 7px;
}
QSlider::handle:horizontal:hover { background: %8; }
QSlider::groove:horizontal:disabled { background: #1c212a; }
QSlider::sub-page:horizontal:disabled { background: #2d3440; }
QSlider::handle:horizontal:disabled { background: #3a424e; }

/* ---- battery ---- */
QProgressBar {
    background: #1c212a;
    border: 1px solid %5;
    border-radius: 6px;
    text-align: center;
    color: %6;
    font-size: 11px;
    font-weight: 600;
    max-height: 18px;
}
QProgressBar::chunk { background: %9; border-radius: 5px; }

/* ---- misc ---- */
#statusLine { color: %7; font-size: 12px; }
#sep { background: %5; max-height: 1px; border: none; }
QToolTip {
    background: %3;
    color: %6;
    border: 1px solid %5;
    padding: 6px;
    border-radius: 5px;
}
QScrollArea { border: none; }
QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }
QScrollBar::handle:vertical { background: #333b46; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #44505e; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
)")
        .arg(Bg, Rail, Card, CardHover, Line, Text, Dim, Accent, Good);
}

} // namespace theme
