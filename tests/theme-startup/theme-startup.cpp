#include <cstdio>

#include <QApplication>
#include <QComboBox>
#include "appdialog.h"
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>

#include "mainwindow.h"
#include "thememanager.h"

int main( int argc, char** argv )
{
    QTemporaryDir profile;
    if( !profile.isValid() ) return 2;
    qputenv( "HOME", profile.path().toUtf8() );
    qputenv( "XDG_CONFIG_HOME", (profile.path()+"/config").toUtf8() );
    qputenv( "XDG_DATA_HOME", (profile.path()+"/data").toUtf8() );
    QApplication app( argc, argv );
    app.setApplicationName( "simulide" );

    const bool startDark = app.arguments().contains( "--dark" );
    const QString config = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );
    QDir().mkpath( config );
    QSettings settings( config+"/simulide.ini", QSettings::IniFormat );
    settings.setValue( "autoUpdt", 0 );
    settings.setValue( "theme", int(startDark ? Theme::Dark : Theme::Light) );
    settings.sync();

    // Simulate a dark desktop even though the saved application theme is light.
    QPalette hostile( QColor("#171b23") );
    hostile.setColor( QPalette::Window, QColor("#171b23") );
    hostile.setColor( QPalette::Base, QColor("#0f141a") );
    hostile.setColor( QPalette::WindowText, QColor("#40ea56") );
    hostile.setColor( QPalette::Text, QColor("#40ea56") );
    app.setPalette( hostile );

    MainWindow window;
    window.resize( 1280, 800 );
    window.showNormal();
    window.show();
    int failures = 0;
    auto check = [&]( bool condition, const char* message ) {
        fprintf( stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message );
        if( !condition ) ++failures;
    };
    AppDialog dialog( &window );
    auto* selector = dialog.findChild<QComboBox*>( "theme" );
    check( selector && selector->count() == 3, "selector exposes Light Dark Follow system" );
    check( selector && selector->itemText(2) == QString("Follow system"), "Follow system label" );

    auto lightPaletteMatches = [&]() {
        const QPalette expected = app.style()->standardPalette();
        const QPalette actual = app.palette();
        for( int group=QPalette::Active; group<=QPalette::Inactive; ++group )
            for( int role=0; role<QPalette::NColorRoles; ++role ) {
                if( role == QPalette::NoRole ) continue;
                if( actual.brush( QPalette::ColorGroup(group), QPalette::ColorRole(role) )
                    != expected.brush( QPalette::ColorGroup(group), QPalette::ColorRole(role) ) ) return false;
            }
        return true;
    };

    if( startDark ) {
        check( ThemeManager::self()->isDark(), "saved dark theme applied at startup" );
        check( app.palette().color(QPalette::Active, QPalette::Window) == QColor("#282828"),
               "dark startup window palette" );
    } else {
        check( !ThemeManager::self()->isDark(), "saved light theme applied at startup" );
        check( lightPaletteMatches(), "light startup uses Fusion light palette, not desktop palette" );
    }
    QTimer::singleShot( 0, [&]() {
        const QByteArray screenshot = qgetenv("THEME_SCREENSHOT");
        if( !screenshot.isEmpty() ) check( window.grab().save(QString::fromLocal8Bit(screenshot)), "startup screenshot saved" );
        ThemeManager::self()->setTheme( Theme::Dark );
        check( ThemeManager::self()->isDark(), "dark transition state" );
        check( app.palette().color(QPalette::Active, QPalette::Text) == QColor("#f0f0e0"), "dark transition text palette" );
        ThemeManager::self()->setTheme( Theme::Light );
        check( !ThemeManager::self()->isDark(), "light transition state" );
        check( lightPaletteMatches(), "dark to light restores Fusion light palette" );
        ThemeManager::self()->setTheme( Theme::Light );
        check( lightPaletteMatches(), "repeated light theme is stable" );
        app.exit( failures ? 1 : 0 );
    } );
    return app.exec();
}
