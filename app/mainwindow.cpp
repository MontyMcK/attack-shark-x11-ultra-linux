#include "mainwindow.h"

#include "dpibar.h"
#include "mousediagram.h"
#include "theme.h"
#include "widgets.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QWidget *hline(QWidget *parent)
{
    auto *w = new QWidget(parent);
    w->setObjectName(QStringLiteral("sep"));
    w->setFixedHeight(1);
    return w;
}

QWidget *labelled(const QString &text, QWidget *w)
{
    auto *box = new QWidget;
    auto *col = new QVBoxLayout(box);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(7);
    auto *l = new QLabel(text, box);
    l->setObjectName(QStringLiteral("cardHint"));
    col->addWidget(l);
    col->addWidget(w);
    return box;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Attack Shark X11 Ultra"));
    resize(1120, 720);
    setMinimumSize(980, 640);

    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildRail());

    auto *right = new QWidget(central);
    auto *rcol = new QVBoxLayout(right);
    rcol->setContentsMargins(0, 0, 0, 0);
    rcol->setSpacing(0);
    rcol->addWidget(buildHeader());
    rcol->addWidget(hline(right));

    m_pages = new QStackedWidget(right);
    m_pages->addWidget(buildSensorPage());
    m_pages->addWidget(buildLightingPage());
    m_pages->addWidget(buildButtonsPage());
    m_pages->addWidget(buildAdvancedPage());
    rcol->addWidget(m_pages, 1);

    rcol->addWidget(hline(right));
    rcol->addWidget(buildFooter());
    root->addWidget(right, 1);

    setCentralWidget(central);

    m_path = ultra::findDevice();
    if (m_path.isEmpty()) {
        m_devLabel->setText(QStringLiteral("No X11 Ultra found"));
        setStatus(QStringLiteral("<span style='color:%1'>Plug the mouse in, or "
                                 "install the udev rule, then restart.</span>")
                      .arg(theme::Bad));
        m_apply->setEnabled(false);
        for (int i = 0; i < m_pages->count(); ++i)
            m_pages->widget(i)->setEnabled(false);
        return;
    }

    if (!refresh()) {
        // Asleep. It wakes on movement, so poll instead of making them restart.
        m_wake = new QTimer(this);
        m_wake->setInterval(2000);
        connect(m_wake, &QTimer::timeout, this, [this]() {
            if (refresh())
                m_wake->stop();
        });
        m_wake->start();
    }
}

// ------------------------------------------------------------- chrome ----

QWidget *MainWindow::buildRail()
{
    auto *rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("rail"));
    rail->setFixedWidth(128);

    auto *col = new QVBoxLayout(rail);
    col->setContentsMargins(0, 18, 0, 18);
    col->setSpacing(2);

    auto *brand = new QLabel(QStringLiteral("X11\nULTRA"), rail);
    brand->setObjectName(QStringLiteral("hdrSub"));
    brand->setContentsMargins(18, 0, 0, 0);
    col->addWidget(brand);
    col->addSpacing(22);

    auto *group = new QButtonGroup(rail);
    group->setExclusive(true);
    const QStringList names{QStringLiteral("SENSOR"), QStringLiteral("LIGHTING"),
                            QStringLiteral("BUTTONS"), QStringLiteral("ADVANCED")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QPushButton(names.at(i), rail);
        b->setObjectName(QStringLiteral("railBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(i == 0);
        group->addButton(b, i);
        col->addWidget(b);
    }
    connect(group, &QButtonGroup::idClicked, this,
            [this](int id) { m_pages->setCurrentIndex(id); });

    col->addStretch();
    return rail;
}

QWidget *MainWindow::buildHeader()
{
    auto *hdr = new QWidget(this);
    auto *row = new QHBoxLayout(hdr);
    row->setContentsMargins(28, 18, 28, 18);
    row->setSpacing(14);

    auto *col = new QVBoxLayout;
    col->setSpacing(2);
    auto *sub = new QLabel(QStringLiteral("CONNECTED DEVICE"), hdr);
    sub->setObjectName(QStringLiteral("hdrSub"));
    col->addWidget(sub);
    m_devLabel = new QLabel(QStringLiteral("Attack Shark X11 Ultra"), hdr);
    m_devLabel->setObjectName(QStringLiteral("hdrTitle"));
    col->addWidget(m_devLabel);
    row->addLayout(col);
    row->addStretch();

    m_batteryText = new QLabel(QStringLiteral("Battery"), hdr);
    m_batteryText->setObjectName(QStringLiteral("cardHint"));
    row->addWidget(m_batteryText);
    m_battery = new QProgressBar(hdr);
    m_battery->setFixedWidth(120);
    m_battery->setRange(0, 100);
    m_battery->setValue(0);
    row->addWidget(m_battery);

    return hdr;
}

QWidget *MainWindow::buildFooter()
{
    auto *f = new QWidget(this);
    auto *row = new QHBoxLayout(f);
    row->setContentsMargins(28, 14, 28, 14);

    m_status = new QLabel(QStringLiteral("Ready"), f);
    m_status->setObjectName(QStringLiteral("statusLine"));
    row->addWidget(m_status);
    row->addStretch();

    auto *reload = new QPushButton(QStringLiteral("Reload"), f);
    connect(reload, &QPushButton::clicked, this, [this]() { refresh(); });
    row->addWidget(reload);

    m_apply = new QPushButton(QStringLiteral("APPLY"), f);
    m_apply->setObjectName(QStringLiteral("primaryBtn"));
    m_apply->setEnabled(false);
    connect(m_apply, &QPushButton::clicked, this, &MainWindow::apply);
    row->addWidget(m_apply);

    return f;
}

// -------------------------------------------------------------- pages ----

QWidget *MainWindow::buildSensorPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *page = new QWidget;
    auto *col = new QVBoxLayout(page);
    col->setContentsMargins(28, 24, 28, 24);
    col->setSpacing(18);

    // ---- DPI ----
    auto *dpiCard = new Card(QStringLiteral("DPI"),
        QStringLiteral("Click a gear to edit it. The outlined gear is the one "
                       "the mouse is using now; click a colour swatch to change "
                       "that gear's LED."));
    m_dpiBar = new DpiBar(dpiCard);
    dpiCard->body()->addWidget(m_dpiBar);

    auto *dpiRow = new QHBoxLayout;
    dpiRow->setSpacing(12);
    m_dpiSpin = new QSpinBox(dpiCard);
    m_dpiSpin->setRange(ultra::kDpiStep, ultra::kDpiMax);
    m_dpiSpin->setSingleStep(ultra::kDpiStep);
    m_dpiSpin->setSuffix(QStringLiteral(" DPI"));
    m_dpiSpin->setFixedWidth(140);
    m_dpiSpin->setToolTip(QStringLiteral(
        "50 to 30000 in steps of 50, then 30100 to 60000 in steps of 100."));
    dpiRow->addWidget(labelled(QStringLiteral("VALUE"), m_dpiSpin));

    m_colorBtn = new QPushButton(QStringLiteral("LED colour"), dpiCard);
    dpiRow->addWidget(labelled(QStringLiteral("GEAR LED"), m_colorBtn));

    m_setActive = new QPushButton(QStringLiteral("Set active"), dpiCard);
    dpiRow->addWidget(labelled(QStringLiteral("CURRENT GEAR"), m_setActive));
    dpiRow->addStretch();
    dpiCard->body()->addLayout(dpiRow);

    connect(m_dpiBar, &DpiBar::stageClicked, this, &MainWindow::selectStage);
    connect(m_dpiBar, &DpiBar::colorClicked, this, &MainWindow::pickColorFor);
    connect(m_colorBtn, &QPushButton::clicked, this,
            [this]() { pickColorFor(m_dpiBar->selected()); });
    connect(m_setActive, &QPushButton::clicked, this, [this]() {
        m_activeStage = m_dpiBar->selected();
        m_dpiBar->setStages(m_dpi, m_colors, m_activeStage);
        m_setActive->setEnabled(false);
        markDirty();
    });
    connect(m_dpiSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        const int st = m_dpiBar->selected();
        if (st < 0 || st >= m_dpi.size()) return;
        m_dpi[st] = v;
        m_dpiBar->setStageDpi(st, v);
        markDirty();
    });
    // Snap only when editing finishes, so typing "1" toward 1600 is not
    // rewritten to 50 under the cursor.
    connect(m_dpiSpin, &QAbstractSpinBox::editingFinished, this, [this]() {
        const int snapped = ultra::snapDpi(m_dpiSpin->value());
        if (snapped != m_dpiSpin->value())
            m_dpiSpin->setValue(snapped);
    });
    col->addWidget(dpiCard);

    // ---- polling rate ----
    auto *rateCard = new Card(QStringLiteral("Polling rate"),
        QStringLiteral("How often the mouse reports. 2000 and above are what "
                       "the older X11 protocol cannot reach at all."));
    m_rate = new Segmented(rateCard);
    for (int i = 0; i < ultra::kRateCount; ++i)
        m_rate->addOption(QStringLiteral("%1 Hz").arg(ultra::kRates[i].hz),
                          ultra::kRates[i].hz);
    connect(m_rate, &Segmented::changed, this, [this](const QVariant &) { markDirty(); });
    rateCard->body()->addWidget(m_rate);
    col->addWidget(rateCard);

    // ---- LOD + toggles ----
    auto *lodCard = new Card(QStringLiteral("Lift off distance"),
        QStringLiteral("How high you can lift before the sensor stops tracking."));
    m_lod = new Segmented(lodCard);
    for (int i = 0; i < ultra::kLodCount; ++i)
        m_lod->addOption(QString::fromLatin1(ultra::kLods[i].label), ultra::kLods[i].code);
    connect(m_lod, &Segmented::changed, this, [this](const QVariant &) { markDirty(); });
    lodCard->body()->addWidget(m_lod);
    col->addWidget(lodCard);

    auto *trackCard = new Card(QStringLiteral("Tracking"));
    m_motion = new ToggleRow(QStringLiteral("Motion sync"),
        QStringLiteral("Aligns sensor reads to the report clock."), trackCard);
    m_angle = new ToggleRow(QStringLiteral("Straight line correction"),
        QStringLiteral("Angle snapping. Straightens near horizontal movement."), trackCard);
    m_ripple = new ToggleRow(QStringLiteral("Ripple correction"),
        QStringLiteral("Smooths jitter at high DPI."), trackCard);
    for (ToggleRow *t : {m_motion, m_angle, m_ripple}) {
        connect(t, &ToggleRow::toggled, this, [this](bool) { markDirty(); });
        trackCard->body()->addWidget(t);
    }
    col->addWidget(trackCard);

    col->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget *MainWindow::buildLightingPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *page = new QWidget;
    auto *col = new QVBoxLayout(page);
    col->setContentsMargins(28, 24, 28, 24);
    col->setSpacing(18);

    auto *c = new Card(QStringLiteral("Gear LED colours"),
        QStringLiteral("Each DPI gear has its own colour. Edit them on the "
                       "Sensor page by clicking a gear's swatch."));
    col->addWidget(c);

    auto *fx = new Card(QStringLiteral("Gear LED effect"),
        QStringLiteral("Off keeps whichever effect you had, it just switches "
                       "the light off. Brightness applies to Always On, speed "
                       "applies to Breathing."));
    m_lightMode = new Segmented(fx);
    m_lightMode->addOption(QStringLiteral("Off"), ultra::LightOff);
    m_lightMode->addOption(QStringLiteral("Always On"), ultra::LightAlwaysOn);
    m_lightMode->addOption(QStringLiteral("Breathing"), ultra::LightBreathing);
    connect(m_lightMode, &Segmented::changed, this, [this](const QVariant &) {
        syncLightEnables();
        markDirty();
    });
    fx->body()->addWidget(m_lightMode);

    auto *sliders = new QWidget(fx);
    auto *grid = new QVBoxLayout(sliders);
    grid->setContentsMargins(0, 6, 0, 0);
    grid->setSpacing(14);

    struct Row { const char *name; QSlider **slider; QLabel **value; int lo, hi; };
    const Row rows[] = {
        { "Brightness", &m_brightness, &m_brightnessVal,
          ultra::kBrightnessMin, ultra::kBrightnessMax },
        { "Speed",      &m_lightSpeed, &m_lightSpeedVal,
          ultra::kLightSpeedMin, ultra::kLightSpeedMax },
    };
    for (const Row &r : rows) {
        auto *line = new QHBoxLayout;
        line->setSpacing(14);
        auto *nameLbl = new QLabel(QString::fromLatin1(r.name), sliders);
        nameLbl->setFixedWidth(80);
        line->addWidget(nameLbl);

        auto *sl = new QSlider(Qt::Horizontal, sliders);
        sl->setRange(r.lo, r.hi);
        sl->setPageStep(1);
        sl->setFixedWidth(260);
        line->addWidget(sl);

        auto *val = new QLabel(QString::number(r.lo), sliders);
        val->setFixedWidth(28);
        line->addWidget(val);
        line->addStretch();
        grid->addLayout(line);

        *r.slider = sl;
        *r.value = val;
        connect(sl, &QSlider::valueChanged, this, [this, val](int v) {
            val->setText(QString::number(v));
            markDirty();
        });
    }
    fx->body()->addWidget(sliders);
    col->addWidget(fx);

    col->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget *MainWindow::buildButtonsPage()
{
    auto *page = new QWidget;
    auto *col = new QVBoxLayout(page);
    col->setContentsMargins(28, 12, 28, 12);

    m_diagram = new MouseDiagram(page);
    col->addWidget(m_diagram, 1);

    auto *note = new QLabel(
        QStringLiteral("Remapping is not wired up yet. The table at offset 96 "
                       "decodes as 4 byte records of [type, button mask, 00, "
                       "checksum], but the full command vocabulary is not "
                       "mapped, so this view is read only for now."), page);
    note->setObjectName(QStringLiteral("cardHint"));
    note->setWordWrap(true);
    col->addWidget(note);

    return page;
}

QWidget *MainWindow::buildAdvancedPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *page = new QWidget;
    auto *col = new QVBoxLayout(page);
    col->setContentsMargins(28, 24, 28, 24);
    col->setSpacing(18);

    auto *dbCard = new Card(QStringLiteral("Key de-shake delay"),
        QStringLiteral("Debounce. Higher values reject switch chatter but add "
                       "latency to every click."));
    m_debounce = new Segmented(dbCard);
    for (int ms : {0, 2, 4, 6, 8, 10, 15, 20})
        m_debounce->addOption(QStringLiteral("%1 ms").arg(ms), ms);
    connect(m_debounce, &Segmented::changed, this, [this](const QVariant &) { markDirty(); });
    dbCard->body()->addWidget(m_debounce);
    col->addWidget(dbCard);

    auto *sensorCard = new Card(QStringLiteral("Sensor"));
    m_fps20k = new ToggleRow(QStringLiteral("20K FPS scan rate"),
        QStringLiteral("Faster sensor sampling. Costs battery."), sensorCard);
    connect(m_fps20k, &ToggleRow::toggled, this, [this](bool) { markDirty(); });
    sensorCard->body()->addWidget(m_fps20k);
    col->addWidget(sensorCard);

    auto *pending = new Card(QStringLiteral("Not decoded yet"));
    pending->setUnavailable(QStringLiteral(
        "Sleep time, sensor performance mode (LP / HP / Corded), long distance "
        "mode and mouse angle tuning all live in flash at known offsets, but "
        "each has only been observed at a single value, which is not enough to "
        "pin the encoding. They are left alone rather than guessed at."));
    col->addWidget(pending);

    col->addStretch();
    scroll->setWidget(page);
    return scroll;
}

// --------------------------------------------------------------- data ----

void MainWindow::syncLightEnables()
{
    const int mode = m_lightMode->currentValue().toInt();
    // The vendor greys these the same way: a static light has no speed, and a
    // breathing one drives its own brightness.
    m_brightness->setEnabled(mode == ultra::LightAlwaysOn);
    m_brightnessVal->setEnabled(mode == ultra::LightAlwaysOn);
    m_lightSpeed->setEnabled(mode == ultra::LightBreathing);
    m_lightSpeedVal->setEnabled(mode == ultra::LightBreathing);
}

void MainWindow::setStatus(const QString &html)
{
    m_status->setText(html);
}

void MainWindow::markDirty()
{
    if (m_loading)
        return;
    m_dirty = true;
    m_apply->setEnabled(true);
}

void MainWindow::selectStage(int stage)
{
    if (stage < 0 || stage >= m_dpi.size())
        return;
    m_dpiBar->setSelected(stage);
    QSignalBlocker block(m_dpiSpin);
    m_dpiSpin->setValue(m_dpi.at(stage));
    m_setActive->setEnabled(stage != m_activeStage);
}

void MainWindow::pickColorFor(int stage)
{
    if (stage < 0 || stage >= m_colors.size())
        return;
    const uint32_t cur = m_colors.at(stage);
    const QColor start = (cur == 0xFFFFFFFFu) ? QColor(Qt::white) : QColor(QRgb(cur));
    const QColor c = QColorDialog::getColor(start, this,
        QStringLiteral("Gear %1 LED colour").arg(stage + 1));
    if (!c.isValid())
        return;
    m_colors[stage] = (static_cast<uint32_t>(c.red()) << 16)
                    | (static_cast<uint32_t>(c.green()) << 8)
                    |  static_cast<uint32_t>(c.blue());
    m_dpiBar->setStageColor(stage, m_colors.at(stage));
    if (stage == m_activeStage)
        m_diagram->setLedColor(m_colors.at(stage));
    markDirty();
}

bool MainWindow::refresh()
{
    const ultra::Settings s = ultra::readSettings(m_path);
    if (!s.valid) {
        m_apply->setEnabled(false);
        setStatus(QStringLiteral(
            "<span style='color:%1'>Mouse is asleep. Move it and this will "
            "pick it up.</span>").arg(theme::Warn));
        return false;
    }

    m_loading = true;
    m_dev = s;
    m_dpi = s.dpiStages;
    m_colors = s.dpiColors;
    m_activeStage = qBound(0, s.currentDpiStage, qMax(0, m_dpi.size() - 1));

    m_dpiBar->setStages(m_dpi, m_colors, m_activeStage);
    selectStage(m_dpiBar->selected());

    m_rate->setCurrentValue(s.reportRateHz);
    m_lod->setCurrentValue(s.lod);
    m_debounce->setCurrentValue(s.debounceMs);
    m_motion->setChecked(s.motionSync);
    m_angle->setChecked(s.angleSnap);
    m_ripple->setChecked(s.ripple);
    m_fps20k->setChecked(s.fps20k);

    m_lightMode->setCurrentValue(s.lightMode);
    {
        QSignalBlocker b1(m_brightness), b2(m_lightSpeed);
        m_brightness->setValue(qBound(ultra::kBrightnessMin, s.brightness,
                                      ultra::kBrightnessMax));
        m_lightSpeed->setValue(qBound(ultra::kLightSpeedMin, s.lightSpeed,
                                      ultra::kLightSpeedMax));
        m_brightnessVal->setText(QString::number(m_brightness->value()));
        m_lightSpeedVal->setText(QString::number(m_lightSpeed->value()));
    }
    syncLightEnables();

    if (m_activeStage < m_colors.size())
        m_diagram->setLedColor(m_colors.at(m_activeStage));

    const ultra::Battery b = ultra::readBattery(m_path);
    if (b.valid) {
        m_battery->setValue(b.percent);
        m_batteryText->setText(b.charging ? QStringLiteral("Battery (charging)")
                                          : QStringLiteral("Battery"));
    }

    const QString conn = (s.deviceType == 3) ? QStringLiteral("wired")
                                             : QStringLiteral("2.4 GHz");
    m_devLabel->setText(QStringLiteral("Attack Shark X11 Ultra"));
    setStatus(QStringLiteral("<span style='color:%1'>%2 on %3, %4 Hz, %5 gears</span>")
                  .arg(theme::Good, conn, m_path).arg(s.reportRateHz).arg(m_dpi.size()));

    m_loading = false;
    m_dirty = false;
    m_apply->setEnabled(false);
    return true;
}

void MainWindow::apply()
{
    ultra::Writable w;
    w.reportRateHz = m_rate->currentValue().toInt();
    w.lod          = m_lod->currentValue().isValid() ? m_lod->currentValue().toInt() : -1;
    w.debounceMs   = m_debounce->currentValue().isValid()
                       ? m_debounce->currentValue().toInt() : -1;
    w.angleSnap    = m_angle->isChecked()  ? 1 : 0;
    w.ripple       = m_ripple->isChecked() ? 1 : 0;
    w.motionSync   = m_motion->isChecked() ? 1 : 0;
    w.fps20k       = m_fps20k->isChecked() ? 1 : 0;

    const QVariant modeVal = m_lightMode->currentValue();
    if (modeVal.isValid()) {
        w.lightMode = modeVal.toInt();
        // Only send the value the chosen effect actually uses, so switching to
        // Breathing cannot quietly rewrite the Always On brightness.
        if (w.lightMode == ultra::LightAlwaysOn)
            w.brightness = m_brightness->value();
        else if (w.lightMode == ultra::LightBreathing)
            w.lightSpeed = m_lightSpeed->value();
    }

    if (!ultra::applyWritable(m_path, w)) {
        setStatus(QStringLiteral("<span style='color:%1'>Write failed. Move the "
                                 "mouse to wake it and try again.</span>").arg(theme::Bad));
        return;
    }

    // Only rewrite the gears that actually changed.
    QList<ultra::DpiWrite> dpiWrites;
    for (int i = 0; i < m_dpi.size() && i < m_dev.dpiStages.size(); ++i) {
        const int want = ultra::snapDpi(m_dpi.at(i));
        m_dpi[i] = want;
        if (want != m_dev.dpiStages.at(i))
            dpiWrites.append({i, want});
    }
    const int wantStage = (m_activeStage != m_dev.currentDpiStage) ? m_activeStage : -1;
    if ((!dpiWrites.isEmpty() || wantStage >= 0)
        && !ultra::applyDpi(m_path, dpiWrites, wantStage)) {
        setStatus(QStringLiteral("<span style='color:%1'>DPI write failed.</span>")
                      .arg(theme::Bad));
        return;
    }

    for (int i = 0; i < m_colors.size() && i < m_dev.dpiColors.size(); ++i) {
        if (m_colors.at(i) == m_dev.dpiColors.at(i) || m_colors.at(i) == 0xFFFFFFFFu)
            continue;
        if (!ultra::applyDpiColor(m_path, i, m_colors.at(i))) {
            setStatus(QStringLiteral("<span style='color:%1'>LED colour write "
                                     "failed on gear %2.</span>").arg(theme::Bad).arg(i + 1));
            return;
        }
    }

    // Read back so the UI shows what the mouse holds, not what we asked for.
    if (refresh())
        setStatus(QStringLiteral("<span style='color:%1'>Applied. Device reports "
                                 "%2 Hz, gear %3 at %4 DPI.</span>")
                      .arg(theme::Good).arg(m_dev.reportRateHz)
                      .arg(m_activeStage + 1).arg(m_dpi.value(m_activeStage)));
}
