#include "widgets.h"
#include "theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------- Card ----

Card::Card(const QString &title, const QString &hint, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 16, 18, 18);
    outer->setSpacing(4);

    auto *t = new QLabel(title, this);
    t->setObjectName(QStringLiteral("cardTitle"));
    outer->addWidget(t);

    m_hint = new QLabel(hint, this);
    m_hint->setObjectName(QStringLiteral("cardHint"));
    m_hint->setWordWrap(true);
    m_hint->setVisible(!hint.isEmpty());
    outer->addWidget(m_hint);

    outer->addSpacing(10);

    m_body = new QVBoxLayout;
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(12);
    outer->addLayout(m_body);
}

void Card::setUnavailable(const QString &why)
{
    m_hint->setText(why);
    m_hint->setVisible(true);
    // Disable the contents but not the card, so the title and reason stay
    // readable instead of greying into the background.
    for (QObject *o : children()) {
        if (auto *w = qobject_cast<QWidget *>(o)) {
            if (w != m_hint && !w->objectName().startsWith(QStringLiteral("card")))
                w->setEnabled(false);
        }
    }
    for (int i = 0; i < m_body->count(); ++i) {
        if (QWidget *w = m_body->itemAt(i)->widget())
            w->setEnabled(false);
    }
}

// ----------------------------------------------------------- Segmented ----

Segmented::Segmented(QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    row->addStretch();

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        if (id >= 0 && id < m_values.size())
            emit changed(m_values.at(id));
    });
}

void Segmented::addOption(const QString &label, const QVariant &value)
{
    auto *row = qobject_cast<QHBoxLayout *>(layout());
    auto *b = new QPushButton(label, this);
    b->setObjectName(QStringLiteral("segBtn"));
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    // Insert before the trailing stretch so the row stays left aligned.
    row->insertWidget(row->count() - 1, b);
    m_group->addButton(b, m_values.size());
    m_values.append(value);
}

void Segmented::setCurrentValue(const QVariant &value)
{
    for (int i = 0; i < m_values.size(); ++i) {
        if (m_values.at(i) == value) {
            if (QAbstractButton *b = m_group->button(i)) {
                const bool was = m_group->exclusive();
                m_group->setExclusive(false);
                for (QAbstractButton *other : m_group->buttons())
                    other->setChecked(false);
                m_group->setExclusive(was);
                b->setChecked(true);
            }
            return;
        }
    }
    // Value not in the list: leave nothing selected rather than lying.
    m_group->setExclusive(false);
    for (QAbstractButton *b : m_group->buttons())
        b->setChecked(false);
    m_group->setExclusive(true);
}

QVariant Segmented::currentValue() const
{
    const int id = m_group->checkedId();
    return (id >= 0 && id < m_values.size()) ? m_values.at(id) : QVariant();
}

void Segmented::clear()
{
    for (QAbstractButton *b : m_group->buttons()) {
        m_group->removeButton(b);
        b->deleteLater();
    }
    m_values.clear();
}

// ----------------------------------------------------------- ToggleRow ----

ToggleRow::ToggleRow(const QString &label, const QString &hint, QWidget *parent)
    : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    auto *col = new QVBoxLayout;
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(1);

    auto *l = new QLabel(label, this);
    col->addWidget(l);
    if (!hint.isEmpty()) {
        auto *h = new QLabel(hint, this);
        h->setObjectName(QStringLiteral("cardHint"));
        col->addWidget(h);
    }
    row->addLayout(col);
    row->addStretch();

    m_box = new QCheckBox(this);
    m_box->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_box);

    connect(m_box, &QCheckBox::toggled, this, &ToggleRow::toggled);
}

bool ToggleRow::isChecked() const { return m_box->isChecked(); }

void ToggleRow::setChecked(bool on)
{
    QSignalBlocker block(m_box);
    m_box->setChecked(on);
}
