#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("attackshark-ultra"));
    app.setApplicationDisplayName(QStringLiteral("Attack Shark X11 Ultra"));
    app.setOrganizationName(QStringLiteral("attackshark-ultra"));
    app.setDesktopFileName(QStringLiteral("attackshark-ultra"));

    // Prefer the installed theme icon so a user themed copy wins, and fall back
    // to the one compiled into the binary when nothing is installed yet.
    QIcon icon = QIcon::fromTheme(QStringLiteral("attackshark-ultra"));
#ifdef HAVE_APPICON
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/appicon.png"));
#endif
    if (!icon.isNull())
        app.setWindowIcon(icon);

    // Set the base palette as well as the sheet: dialogs we do not style by
    // hand (the colour picker) pick their colours up from here.
    QPalette pal;
    pal.setColor(QPalette::Window,          QColor(theme::Bg));
    pal.setColor(QPalette::WindowText,      QColor(theme::Text));
    pal.setColor(QPalette::Base,            QColor(theme::Bg));
    pal.setColor(QPalette::AlternateBase,   QColor(theme::Card));
    pal.setColor(QPalette::Text,            QColor(theme::Text));
    pal.setColor(QPalette::Button,          QColor(theme::CardHover));
    pal.setColor(QPalette::ButtonText,      QColor(theme::Text));
    pal.setColor(QPalette::Highlight,       QColor(theme::Accent));
    pal.setColor(QPalette::HighlightedText, QColor("#051016"));
    pal.setColor(QPalette::ToolTipBase,     QColor(theme::Card));
    pal.setColor(QPalette::ToolTipText,     QColor(theme::Text));
    app.setPalette(pal);
    app.setStyleSheet(theme::sheet());

    MainWindow w;
    w.show();
    return app.exec();
}
