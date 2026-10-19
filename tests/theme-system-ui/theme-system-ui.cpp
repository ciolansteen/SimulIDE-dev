#include <cstdio>
#include <functional>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QElapsedTimer>
#include <QProcess>
#include <QDBusVirtualObject>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusVariant>
#include "mainwindow.h"
#include "thememanager.h"
#include "appdialog.h"
#include "circuit.h"
#include "circuitwidget.h"
#include "component.h"
#include "simulator.h"
#include "editorwindow.h"
#include "codeeditor.h"
#include "pin.h"
#include "propdialog.h"

class Portal : public QDBusVirtualObject
{
public:
    bool inject = false;
    QString introspect(const QString&) const override { return {}; }
    bool handleMessage(const QDBusMessage& msg, const QDBusConnection& bus) override
    {
        if (msg.interface() != "org.freedesktop.portal.Settings") return false;
        bus.send(msg.createReply(QVariantList{QVariant::fromValue(QDBusVariant(QVariant(1u)))}));
        return true;
    }
    void change(uint value)
    {
        if (inject) {
            ThemeManager::self()->observeSystemPreference(value == 1 ? SystemPreference::Dark : value == 2 ? SystemPreference::Light : SystemPreference::Unknown);
            return;
        }
        auto msg = QDBusMessage::createSignal("/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "SettingChanged");
        msg << QString("org.freedesktop.appearance") << QString("color-scheme") << QVariant::fromValue(QDBusVariant(QVariant(value)));
        QDBusConnection::sessionBus().send(msg);
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;
    const bool restart = argc > 2 && QByteArray(argv[1]) == "--restart";
    const QString profile = restart ? QString::fromLocal8Bit(argv[2]) : temporary.path();
    qputenv("HOME", profile.toUtf8());
    qputenv("XDG_CONFIG_HOME", (profile+"/config").toUtf8());
    qputenv("XDG_DATA_HOME", (profile+"/data").toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName("simulide");
    app.setQuitOnLastWindowClosed(false);
    const QString config = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(config);
    QSettings settings(config+"/simulide.ini", QSettings::IniFormat);
    if (!restart) { settings.setValue("autoUpdt", 0); settings.setValue("autoBck", -1); settings.setValue("theme", 2); settings.sync(); }
    Portal portal;
    portal.inject = app.arguments().contains("--injected");
    auto bus = QDBusConnection::sessionBus();
    if (!restart && (!bus.registerService("org.freedesktop.portal.Desktop") || !bus.registerVirtualObject("/org/freedesktop/portal/desktop", &portal))) return 3;
    MainWindow window;
    window.resize(1280, 850); window.showNormal(); window.show();
    int failures = 0;
    auto check = [&](bool ok, const char* message) { fprintf(stderr, "%s: %s\n", ok?"PASS":"FAIL", message); if (!ok) ++failures; };
    auto waitFor = [&](const std::function<bool()>& condition) {
        QElapsedTimer timer; timer.start();
        while (!condition() && timer.elapsed() < 3000) QTest::qWait(10);
        return condition();
    };
    auto manager = ThemeManager::self();
    if (portal.inject) manager->observeSystemPreference(SystemPreference::Dark);
    if (restart) {
        check(manager->theme() == Theme::System, "restart restores selected System");
        check(waitFor([&] { return manager->isDark(); }), "restart resolves portal Dark independently");
        window.close();
        return failures ? 1 : 0;
    }
    check(waitFor([&] { return manager->isDark(); }), "System startup follows actual private portal read");
    AppDialog dialog(&window);
    auto selector = dialog.findChild<QComboBox*>("theme");
    check(selector && selector->count() == 3 && selector->currentIndex() == 2, "real selector restores Follow system");
    if (!selector) return 4;
    dialog.show(); QTest::qWait(50);
    check(selector->width() >= selector->sizeHint().width(), "Follow system label not clipped");
    auto circuit = Circuit::self();
    circuit->setAutoBck(-1);
    auto resistor = circuit->createComponent("Resistor", "Resistor", QPoint(0,0));
    check(resistor != nullptr, "synthetic resistor created");
    auto meter = circuit->createComponent("Voltimeter", "Meter", QPoint(120,0));
    auto battery = circuit->createComponent("Battery", "Supply", QPoint(-100,0));
    check(meter && battery, "synthetic meter and supply created");
    if (resistor && meter && battery) {
        auto connectPins = [&](Pin* a, Pin* b) { circuit->newconnector(a, false); circuit->closeconnector(b, false); };
        connectPins(battery->getPins()[0], resistor->getPins()[0]);
        connectPins(resistor->getPins()[1], meter->getPins()[0]);
        connectPins(meter->getPins()[1], battery->getPins()[1]);
    }
    PropDialog properties(&window);
    if (resistor) { properties.setComponent(resistor); properties.show(); }
    if (resistor) resistor->setSelected(true);
    const QString editorPath = profile+"/sample.cpp";
    { QFile file(editorPath); file.open(QIODevice::WriteOnly); file.write("// theme fixture\nint answer = 42;\n"); }
    EditorWindow::self()->loadFile(editorPath);
    auto editor = EditorWindow::self()->getCodeEditor();
    check(editor != nullptr, "real editor document opened");
    if (!editor) return 5;
    editor->moveCursor(QTextCursor::End); editor->insertPlainText("// keep undo\n");
    const QString text = editor->toPlainText();
    const bool undo = editor->document()->isUndoAvailable();
    const bool modified = editor->document()->isModified();
    const QString circuitText = circuit->circuitToString();
    const QString title = window.windowTitle();
    const auto selectedItems = circuit->selectedItems();
    uint32_t* colors[4]; for (uint i=0; i<4; ++i) colors[i] = manager->getColorPtr(i);
    auto invariant = [&]() {
        check(circuit->circuitToString() == circuitText, "theme preserves circuit serialized structure/properties");
        check(window.windowTitle() == title, "theme preserves dirty title state");
        check(circuit->selectedItems() == selectedItems, "theme preserves circuit selection");
        check(editor->toPlainText() == text && editor->document()->isUndoAvailable() == undo && editor->document()->isModified() == modified, "theme preserves editor text undo modified state");
        for (uint i=0; i<4; ++i) check(colors[i] == manager->getColorPtr(i), "semantic color address stable");
    };
    const QString captureDir = QString::fromLocal8Bit(qgetenv("THEME_CAPTURE_DIR"));
    auto capture = [&](const QString& name) {
        if (captureDir.isEmpty()) return;
        QDir().mkpath(captureDir); QTest::qWait(50);
        check(window.grab().save(captureDir+"/"+name+"-app.png"), "application capture saved");
        check(dialog.grab().save(captureDir+"/"+name+"-settings.png"), "settings capture saved");
        check(properties.grab().save(captureDir+"/"+name+"-properties.png"), "existing property dialog capture saved");
    };
    capture("dark");
    portal.change(2);
    check(waitFor([&] { return !manager->isDark(); }), "live System-to-System switches effective Light");
    check(selector->currentIndex() == 2 && manager->theme() == Theme::System, "live event preserves selected System");
    check(*colors[0] == 0xFFF0F0D2, "light circuit semantic background refreshed");
    check(dialog.palette().color(QPalette::Window) == app.palette().color(QPalette::Window), "existing dialog palette refreshes");
    invariant(); capture("light");
    selector->setCurrentIndex(0); portal.change(1); QTest::qWait(60);
    check(!manager->isDark() && manager->theme() == Theme::Light, "manual Light ignores portal Dark");
    selector->setCurrentIndex(2);
    check(manager->isDark(), "return to System uses latest observed Dark");
    selector->setCurrentIndex(1); portal.change(2); QTest::qWait(60);
    check(manager->isDark() && manager->theme() == Theme::Dark, "manual Dark ignores portal Light");
    selector->setCurrentIndex(2);
    check(!manager->isDark(), "return to System uses latest observed Light");
    QPalette hostile(QColor("#101010"));
    app.setPalette(hostile); QTest::qWait(50);
    check(!manager->isDark() && manager->theme() == Theme::System, "self-generated queued palette events cannot become system preference");
    portal.change(1); check(waitFor([&] { return manager->isDark(); }), "independent portal Dark still works after app palette event");
    portal.change(2); check(waitFor([&] { return !manager->isDark(); }), "independent portal Light restores coherent palette");
    invariant();
    Simulator::self()->startSim(); QTest::qWait(100);
    const auto simState = Simulator::self()->simState();
    check(Simulator::self()->isRunning(), "synthetic simulation is running");
    portal.change(1); check(waitFor([&] { return manager->isDark(); }), "live theme updates during simulation");
    check(Simulator::self()->simState() == simState && circuit->compList()->contains(resistor), "theme preserves running simulation and component identities");
    Simulator::self()->stopSim();
    invariant();
    // Rapid input on the GUI thread while portal updates are queued.
    editor->moveCursor(QTextCursor::End);
    for (int i=0; i<4; ++i) { portal.change(i%2 ? 1 : 2); QTest::keyClicks(editor, "x"); QTest::qWait(10); }
    check(editor->toPlainText() == text+"xxxx", "queued appearance changes do not drop editor input");
    editor->document()->setModified(false);
    // Close only this synthetic circuit, never a user file, avoiding a save prompt.
    properties.close();
    CircuitWidget::self()->saveCirc(profile+"/synthetic.sim2");
    dialog.close(); selector->setCurrentIndex(2);
    check(window.close(), "application closes cleanly");
    window.settings()->sync(); settings.sync();
    check(settings.value("theme").toInt() == 2, "persistence saves System not effective Dark");
    QProcess child;
    QStringList childArgs{"--restart", profile};
    if (portal.inject) childArgs << "--injected";
    child.start(app.applicationFilePath(), childArgs);
    check(waitFor([&] { return child.state() == QProcess::NotRunning; }) && child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0, "fresh process restores System and resolves preference");
    fprintf(stderr, "%s", child.readAllStandardError().constData());
    return failures ? 1 : 0;
}
