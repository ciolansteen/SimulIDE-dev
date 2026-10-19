// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemthemeobserver.h"

#ifdef SIMULIDE_QTDBUS
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

namespace {
const QString service = "org.freedesktop.portal.Desktop";
const QString path = "/org/freedesktop/portal/desktop";
const QString interface = "org.freedesktop.portal.Settings";
const QString appearance = "org.freedesktop.appearance";
const QString colorScheme = "color-scheme";

bool decode(QVariant value, SystemPreference& preference)
{
    // ReadOne returns v; historical Read may return v containing another v.
    for (int depth = 0; depth < 8; ++depth) {
        if (value.userType() == qMetaTypeId<QDBusVariant>())
            value = qvariant_cast<QDBusVariant>(value).variant();
        else if (value.userType() == qMetaTypeId<QDBusArgument>()) {
            auto argument = qvariant_cast<QDBusArgument>(value);
            if (argument.currentType() != QDBusArgument::VariantType) return false;
            QDBusVariant inner;
            argument >> inner;
            value = inner.variant();
        } else break;
    }
    if (value.type() != QVariant::UInt) return false;
    const uint scheme = value.toUInt();
    preference = scheme == 1 ? SystemPreference::Dark
               : scheme == 2 ? SystemPreference::Light : SystemPreference::Unknown;
    return true;
}
}
#endif

SystemThemeObserver::SystemThemeObserver(QObject* parent)
    : QObject(parent)
#ifdef SIMULIDE_QTDBUS
    , m_bus(QDBusConnection::sessionBus())
#endif
{
    qRegisterMetaType<SystemPreference>("SystemPreference");
}

void SystemThemeObserver::publish(SystemPreference preference)
{
    if (m_preference == preference) return;
    m_preference = preference;
    emit preferenceChanged(preference);
}

void SystemThemeObserver::start()
{
    if (m_started) return;
    m_started = true;
#ifdef SIMULIDE_QTDBUS
    if (!m_bus.isConnected()) return;
    auto watcher = new QDBusServiceWatcher(service, m_bus,
                        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString&, const QString&, const QString& owner) { setOwner(owner); });
    // Subscribe first, then asynchronously obtain the existing owner. A newer
    // owner notification invalidates this bootstrap reply as well as reads.
    auto message = QDBusMessage::createMethodCall("org.freedesktop.DBus",
                    "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner");
    message << service;
    const auto generation = m_generation;
    auto pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 1500), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher* call) {
        const auto reply = call->reply();
        call->deleteLater();
        if (generation != m_generation || reply.type() == QDBusMessage::ErrorMessage) return;
        if (reply.arguments().size() == 1 && reply.arguments().first().type() == QVariant::String)
            setOwner(reply.arguments().first().toString());
    });
#endif
}

#ifdef SIMULIDE_QTDBUS
void SystemThemeObserver::setOwner(const QString& owner)
{
    ++m_generation;
    if (!m_owner.isEmpty())
        m_bus.disconnect(m_owner, path, interface, "SettingChanged", this,
                         SLOT(settingChanged(QString,QString,QDBusVariant)));
    m_owner = owner;
    // Retain the last valid observation during an outage. A fresh service may
    // explicitly report no preference, which resolves to the documented Light.
    if (owner.isEmpty()) return;
    m_bus.connect(owner, path, interface, "SettingChanged", this,
                  SLOT(settingChanged(QString,QString,QDBusVariant)));
    read();
}

void SystemThemeObserver::read(bool legacy)
{
    if (m_owner.isEmpty()) return;
    const auto owner = m_owner;
    const auto generation = m_generation;
    auto message = QDBusMessage::createMethodCall(owner, path, interface,
                                                legacy ? "Read" : "ReadOne");
    message << appearance << colorScheme;
    auto pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 1500), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, owner, generation, legacy](QDBusPendingCallWatcher* call) {
        const auto reply = call->reply();
        call->deleteLater();
        if (owner != m_owner || generation != m_generation) return;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (!legacy && reply.errorName() == "org.freedesktop.DBus.Error.UnknownMethod") read(true);
            return;
        }
        SystemPreference preference;
        if (reply.arguments().size() == 1 && decode(reply.arguments().first(), preference))
            publish(preference);
    });
}

void SystemThemeObserver::settingChanged(const QString& nameSpace, const QString& key,
                                          const QDBusVariant& value)
{
    if (nameSpace != appearance || key != colorScheme) return;
    SystemPreference preference;
    if (!decode(value.variant(), preference)) return;
    ++m_generation; // Even duplicate valid signals supersede an in-flight read.
    publish(preference);
}
#endif
