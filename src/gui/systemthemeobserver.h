// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include "themepolicy.h"
#ifdef SIMULIDE_QTDBUS
#include <QDBusConnection>
#include <QDBusVariant>
#endif

Q_DECLARE_METATYPE(SystemPreference)

// Independent OS preference input: never sample the application-owned palette.
class SystemThemeObserver : public QObject
{
    Q_OBJECT
public:
    explicit SystemThemeObserver(QObject* parent = nullptr);
    void start();
    SystemPreference preference() const { return m_preference; }

signals:
    void preferenceChanged(SystemPreference preference);

private:
    void publish(SystemPreference preference);
    SystemPreference m_preference = SystemPreference::Unknown;
    bool m_started = false;
#ifdef SIMULIDE_QTDBUS
private slots:
    void settingChanged(const QString& nameSpace, const QString& key, const QDBusVariant& value);
private:
    void setOwner(const QString& owner);
    void read(bool legacy = false);
    QDBusConnection m_bus;
    QString m_owner;
    quint64 m_generation = 0;
#endif
};
