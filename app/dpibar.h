#ifndef DPIBAR_H
#define DPIBAR_H

#include <QList>
#include <QWidget>

// The row of DPI stage chips, one per gear, each showing its LED colour and
// value. Clicking one selects it for editing; the stage the mouse is actually
// using is outlined. Modelled on the vendor page's stage row, which is the one
// part of its layout worth keeping.
class DpiBar : public QWidget
{
    Q_OBJECT
public:
    explicit DpiBar(QWidget *parent = nullptr);

    void setStages(const QList<int> &dpi, const QList<uint32_t> &colors,
                   int activeStage);
    void setSelected(int stage);
    int  selected() const { return m_selected; }
    void setStageDpi(int stage, int dpi);
    void setStageColor(int stage, uint32_t rgb);
    int  stageCount() const { return m_dpi.size(); }

signals:
    void stageClicked(int stage);
    void colorClicked(int stage);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    QSize sizeHint() const override;

private:
    QRect chipRect(int i) const;
    QRect swatchRect(int i) const;

    QList<int> m_dpi;
    QList<uint32_t> m_colors;
    int m_active = -1;
    int m_selected = 0;
};

#endif // DPIBAR_H
