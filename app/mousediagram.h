#ifndef MOUSEDIAGRAM_H
#define MOUSEDIAGRAM_H

#include <QHash>
#include <QRectF>
#include <QWidget>

// The mouse with callout lines, drawn with QPainter rather than shipped as a
// bitmap. A render of the actual product would be the vendor's photograph, and
// a vector outline themes properly and stays sharp at any scale anyway.
class MouseDiagram : public QWidget
{
    Q_OBJECT
public:
    enum Button { Left, Right, Middle, Forward, Back, Dpi };

    explicit MouseDiagram(QWidget *parent = nullptr);

    // Label shown next to a button, e.g. "Primary Click".
    void setAssignment(Button b, const QString &text);
    void setLedColor(uint32_t rgb);   // the stage LED under the scroll wheel

signals:
    void buttonClicked(Button b);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    // Which way a callout line runs out of the body.
    enum Dir { LeftS, RightS, Up };
    struct Hotspot { QPointF at; Dir dir; QString label; };
    QHash<int, Hotspot> spots() const;
    QRectF bodyRect() const;

    QHash<int, QString> m_assign;
    uint32_t m_led = 0xFFFFFFFFu;
};

#endif // MOUSEDIAGRAM_H
