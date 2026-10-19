#include <QtTest>
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QTimer>
#include <QElapsedTimer>
#include "systemthemeobserver.h"

static const QString service = "org.freedesktop.portal.Desktop";
static const QString path = "/org/freedesktop/portal/desktop";
static const QString iface = "org.freedesktop.portal.Settings";

class Portal : public QDBusVirtualObject
{
public:
    bool legacy = false;
    bool nested = false;
    bool malformed = false;
    uint value = 1;
    int delay = 0;
    int calls = 0;
    QStringList methods;
    QString introspect(const QString&) const override { return {}; }
    bool handleMessage(const QDBusMessage& msg, const QDBusConnection& bus) override
    {
        if (msg.interface() != iface) return false;
        ++calls;
        methods << msg.member();
        if (legacy && msg.member() == "ReadOne") {
            bus.send(msg.createErrorReply("org.freedesktop.DBus.Error.UnknownMethod", "v1"));
            return true;
        }
        QVariant result = malformed ? QVariant(QString("dark")) : QVariant(value);
        if (nested || msg.member() == "Read") result = QVariant::fromValue(QDBusVariant(result));
        auto reply = msg.createReply(QVariantList{QVariant::fromValue(QDBusVariant(result))});
        QTimer::singleShot(delay, this, [bus, reply]() { bus.send(reply); });
        return true;
    }
    void changed(uint next, const QString& ns = "org.freedesktop.appearance", const QString& key = "color-scheme")
    {
        value = next;
        auto msg = QDBusMessage::createSignal(path, iface, "SettingChanged");
        msg << ns << key << QVariant::fromValue(QDBusVariant(QVariant(next)));
        QDBusConnection::sessionBus().send(msg);
    }
};

class SystemThemeTest : public QObject
{
    Q_OBJECT
    Portal portal;
private slots:
    void init()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(service));
        QVERIFY(bus.registerVirtualObject(path, &portal));
        portal.legacy = portal.nested = portal.malformed = false;
        portal.value = 1; portal.delay = portal.calls = 0; portal.methods.clear();
    }
    void cleanup()
    {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterObject(path); bus.unregisterService(service);
        QDBusConnection::disconnectFromBus("replacement");
        QTest::qWait(10);
    }
    void readOneAndLiveChanges()
    {
        SystemThemeObserver observer;
        QSignalSpy spy(&observer, &SystemThemeObserver::preferenceChanged);
        observer.start();
        QTRY_COMPARE(observer.preference(), SystemPreference::Dark);
        QCOMPARE(portal.methods, QStringList{"ReadOne"});
        portal.changed(2);
        QTRY_COMPARE(observer.preference(), SystemPreference::Light);
        int count = spy.count();
        portal.changed(2); portal.changed(1, "unrelated"); portal.changed(1, "org.freedesktop.appearance", "accent");
        QTest::qWait(50);
        QCOMPARE(spy.count(), count);
        portal.changed(0);
        QTRY_COMPARE(observer.preference(), SystemPreference::Unknown);
        portal.changed(47);
        QTest::qWait(50);
        QCOMPARE(observer.preference(), SystemPreference::Unknown);
    }
    void legacyNestedVariants()
    {
        portal.legacy = portal.nested = true;
        portal.value = 2;
        SystemThemeObserver observer;
        observer.start();
        QTRY_COMPARE(observer.preference(), SystemPreference::Light);
        QCOMPARE(portal.methods, (QStringList{"ReadOne", "Read"}));
    }
    void malformedRead()
    {
        portal.malformed = true;
        SystemThemeObserver observer;
        observer.start();
        QTRY_VERIFY(portal.calls > 0);
        QTest::qWait(50);
        QCOMPARE(observer.preference(), SystemPreference::Unknown);
    }
    void signalBeforeReadReply()
    {
        portal.delay = 150;
        SystemThemeObserver observer;
        observer.start();
        QTRY_VERIFY(portal.calls > 0);
        portal.changed(2);
        QTRY_COMPARE(observer.preference(), SystemPreference::Light);
        QTest::qWait(200);
        QCOMPARE(observer.preference(), SystemPreference::Light);
    }
    void ownerRestartKeepsLastThenReadsNew()
    {
        SystemThemeObserver observer;
        observer.start();
        QTRY_COMPARE(observer.preference(), SystemPreference::Dark);
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.unregisterService(service));
        QTest::qWait(50);
        QCOMPARE(observer.preference(), SystemPreference::Dark);
        portal.value = 2;
        QVERIFY(bus.registerService(service));
        QTRY_COMPARE(observer.preference(), SystemPreference::Light);
    }
    void ownerReplacementRejectsOldReply()
    {
        portal.delay = 150;
        SystemThemeObserver observer;
        observer.start();
        QTRY_VERIFY(portal.calls > 0);
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.unregisterService(service));
        QTest::qWait(30);
        portal.value = 2; portal.delay = 0;
        auto replacement = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "replacement");
        QVERIFY(replacement.registerVirtualObject(path, &portal));
        QVERIFY(replacement.registerService(service));
        QTRY_COMPARE(observer.preference(), SystemPreference::Light);
        QTest::qWait(200);
        QCOMPARE(observer.preference(), SystemPreference::Light);
    }
    void timeoutIsNonBlocking()
    {
        // A separate connection forces messages through the private bus. Qt
        // optimizes calls to objects on its own connection (no transport timeout).
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.unregisterService(service));
        auto remote = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "replacement");
        QVERIFY(remote.registerVirtualObject(path, &portal));
        QVERIFY(remote.registerService(service));
        portal.delay = 1700;
        SystemThemeObserver observer;
        QElapsedTimer elapsed; elapsed.start();
        observer.start();
        QTRY_VERIFY(portal.calls > 0);
        QVERIFY(elapsed.elapsed() < 500);
        QTest::qWait(1750);
        QCOMPARE(observer.preference(), SystemPreference::Unknown);
    }
    void missingServiceAndTeardown()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.unregisterService(service));
        { SystemThemeObserver observer; observer.start(); QTest::qWait(50);
          QCOMPARE(observer.preference(), SystemPreference::Unknown); }
        QVERIFY(bus.registerService(service));
        portal.delay = 100;
        { SystemThemeObserver observer; observer.start(); QTest::qWait(20); }
        QTest::qWait(150);
    }
};
QTEST_GUILESS_MAIN(SystemThemeTest)
#include "system-theme.moc"
