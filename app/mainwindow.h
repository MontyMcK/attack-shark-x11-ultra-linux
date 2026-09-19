#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>

#include "ultra.h"

class Card;
class DpiBar;
class MouseDiagram;
class Segmented;
class ToggleRow;

class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *buildRail();
    QWidget *buildHeader();
    QWidget *buildFooter();
    QWidget *buildSensorPage();
    QWidget *buildLightingPage();
    QWidget *buildButtonsPage();
    QWidget *buildAdvancedPage();

    // Pull the device state into the widgets. False if the mouse did not
    // answer, which on this device usually just means it is asleep.
    bool refresh();
    void apply();
    void markDirty();
    void setStatus(const QString &html);
    void pickColorFor(int stage);
    void selectStage(int stage);

    QString m_path;
    ultra::Settings m_dev;          // what the last good read found
    QList<int> m_dpi;               // pending stage values
    QList<uint32_t> m_colors;       // pending stage colours
    int m_activeStage = 0;          // pending active stage
    bool m_dirty = false;
    bool m_loading = false;         // suppress dirty marking while populating

    QStackedWidget *m_pages = nullptr;
    QLabel *m_devLabel = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_battery = nullptr;
    QLabel *m_batteryText = nullptr;
    QPushButton *m_apply = nullptr;
    QTimer *m_wake = nullptr;

    DpiBar *m_dpiBar = nullptr;
    QSpinBox *m_dpiSpin = nullptr;
    QPushButton *m_setActive = nullptr;
    QPushButton *m_colorBtn = nullptr;
    Segmented *m_rate = nullptr;
    Segmented *m_lod = nullptr;
    ToggleRow *m_motion = nullptr;
    ToggleRow *m_ripple = nullptr;
    ToggleRow *m_angle = nullptr;
    ToggleRow *m_fps20k = nullptr;
    Segmented *m_debounce = nullptr;
    MouseDiagram *m_diagram = nullptr;
};

#endif // MAINWINDOW_H
