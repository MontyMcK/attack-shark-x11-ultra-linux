#ifndef WIDGETS_H
#define WIDGETS_H

#include <QFrame>
#include <QList>
#include <QVariant>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QVBoxLayout;

// A titled panel. Body widgets go into body().
class Card : public QFrame
{
    Q_OBJECT
public:
    explicit Card(const QString &title, const QString &hint = QString(),
                  QWidget *parent = nullptr);
    QVBoxLayout *body() const { return m_body; }
    // Grey the whole card out and say why, for things the protocol does not
    // cover yet. Better than a live looking control that writes nothing.
    void setUnavailable(const QString &why);

private:
    QVBoxLayout *m_body = nullptr;
    QLabel *m_hint = nullptr;
};

// Row of mutually exclusive buttons, the shape both G HUB and the vendor page
// use for small fixed choices (polling rate, LOD, lighting mode).
class Segmented : public QWidget
{
    Q_OBJECT
public:
    explicit Segmented(QWidget *parent = nullptr);

    void addOption(const QString &label, const QVariant &value);
    void setCurrentValue(const QVariant &value);   // no signal
    QVariant currentValue() const;
    void clear();

signals:
    void changed(const QVariant &value);

private:
    QButtonGroup *m_group = nullptr;
    QList<QVariant> m_values;
};

// Label plus checkbox, laid out like the vendor page's toggle rows.
class ToggleRow : public QWidget
{
    Q_OBJECT
public:
    ToggleRow(const QString &label, const QString &hint = QString(),
              QWidget *parent = nullptr);
    bool isChecked() const;
    void setChecked(bool on);   // no signal

signals:
    void toggled(bool on);

private:
    QCheckBox *m_box = nullptr;
};

#endif // WIDGETS_H
