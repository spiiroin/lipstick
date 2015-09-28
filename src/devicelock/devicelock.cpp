/***************************************************************************
**
** Copyright (C) 2013, 2014 Jolla Ltd.
** Contact: Jonni Rainisto <jonni.rainisto@jollamobile.com>
**
** This file is part of lipstick.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include <QCoreApplication>
#include <QSettings>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include "devicelock.h"
#include <sys/time.h>
#include <QDBusConnection>
#include <QFile>
#include <QFileInfo>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include "homeapplication.h"

namespace {
const char * const settingsFile = "/usr/share/lipstick/devicelock/devicelock_settings.conf";
const char * const lockingKey = "/desktop/nemo/devicelock/automatic_locking";
}
/* ------------------------------------------------------------------------- *
 * struct timeval helpers
 * ------------------------------------------------------------------------- */
static void tv_get_monotime(struct timeval *tv)
{
#if defined(CLOCK_BOOTTIME)
  struct timespec ts;
  if (clock_gettime(CLOCK_BOOTTIME, &ts) < 0)
      if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
          qFatal("Can't clock_gettime!");
  TIMESPEC_TO_TIMEVAL(tv, &ts);
#endif
}

static int tv_diff_in_s(const struct timeval *tv1, const struct timeval *tv2)
{
    struct timeval tv;
    timersub(tv1, tv2, &tv);
    return tv.tv_sec;
}

DeviceLock::DeviceLock(QObject * parent) :
    QObject(parent),
    QDBusContext(),
    lockingDelay(-1),
    lockTimer(new QTimer(this)),
    deviceLockState(Undefined),
    m_activity(true),
    m_displayOn(true),
    m_activeCall(false),
    m_blankingPause(false),
    m_blankingInhibit(false)
{
    // flag timer as not-started
    monoTime.tv_sec = 0;

    // Note: deviceLockState stays Undefined until init() gets called
    connect(static_cast<HomeApplication *>(qApp), &HomeApplication::homeReady, this, &DeviceLock::init);

    connect(lockTimer, SIGNAL(timeout()), this, SLOT(lock()));

    trackCallState();
    trackDisplayState();
    trackInactivityState();
    trackBlankingPause();
    trackBlankingInhibit();
}

// Call State
void DeviceLock::handleCallStateChanged(const QString &state)
{
    qDebug() << state;

    bool active = (state == "active" || state == "ringing");

    if (m_activeCall != active) {
        m_activeCall = active;
        setStateAndSetupLockTimer();
    }
}

void DeviceLock::handleCallStateReply(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to mce failed:" << reply.error();
    } else {
        handleCallStateChanged(reply.value());
    }
    call->deleteLater();
}

void DeviceLock::trackCallState()
{
    QDBusConnection::systemBus().connect(QString(), "/com/nokia/mce/signal", "com.nokia.mce.signal", "sig_call_state_ind",
                                         this, SLOT(handleCallStateChanged(QString)));

    QDBusMessage call = QDBusMessage::createMethodCall("com.nokia.mce", "/", "com.nokia.mce.request", "get_call_state");
    QDBusPendingCall reply = QDBusConnection::systemBus().asyncCall(call);
    QDBusPendingCallWatcher *watch = new QDBusPendingCallWatcher(reply, this);
    connect(watch, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(handleCallStateReply(QDBusPendingCallWatcher*)));
}

// Display State

void DeviceLock::handleDisplayStateChanged(const QString &state)
{
    qDebug() << state;

    bool displayOn = (state == "on" || state == "dimmed");

    if (m_displayOn != displayOn) {
        m_displayOn = displayOn;
        setStateAndSetupLockTimer();
    }
}

void DeviceLock::handleDisplayStateReply(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to mce failed:" << reply.error();
    } else {
        handleDisplayStateChanged(reply.value());
    }
    call->deleteLater();
}

void DeviceLock::trackDisplayState()
{
    QDBusConnection::systemBus().connect(QString(), "/com/nokia/mce/signal", "com.nokia.mce.signal", "display_status_ind",
                                         this, SLOT(handleDisplayStateChanged(QString)));

    QDBusMessage call = QDBusMessage::createMethodCall("com.nokia.mce", "/", "com.nokia.mce.request", "get_display_status");
    QDBusPendingCall reply = QDBusConnection::systemBus().asyncCall(call);
    QDBusPendingCallWatcher *watch = new QDBusPendingCallWatcher(reply, this);
    connect(watch, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(handleDisplayStateReply(QDBusPendingCallWatcher*)));
}

// Inactivity State

void DeviceLock::handleInactivityStateChanged(const bool state)
{
    qDebug() << state;

    bool activity = !state;

    if (m_activity != activity) {
        m_activity = activity;
        setStateAndSetupLockTimer();
    }
}

void DeviceLock::handleInactivityStateReply(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<bool> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to mce failed:" << reply.error();
    } else {
        handleInactivityStateChanged(reply.value());
    }
    call->deleteLater();
}

void DeviceLock::trackInactivityState(void)
{
    QDBusConnection::systemBus().connect(QString(), "/com/nokia/mce/signal", "com.nokia.mce.signal", "system_inactivity_ind",
                                         this, SLOT(handleInactivityStateChanged(bool)));

    QDBusMessage call = QDBusMessage::createMethodCall("com.nokia.mce", "/", "com.nokia.mce.request", "get_inactivity_status");
    QDBusPendingCall reply = QDBusConnection::systemBus().asyncCall(call);
    QDBusPendingCallWatcher *watch = new QDBusPendingCallWatcher(reply, this);
    connect(watch, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(handleInactivityStateReply(QDBusPendingCallWatcher*)));
}

// Blanking Inhibit

void DeviceLock::handleBlankingInhibitChanged(const QString &state)
{
    qDebug() << state;

    bool blankingInhibit = (state == "active");
    if (m_blankingInhibit != blankingInhibit ) {
        m_blankingInhibit = blankingInhibit;
        emit blankingInhibitChanged();
    }
}

void DeviceLock::handleBlankingInhibitReply(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to mce failed:" << reply.error();
    } else {
        handleBlankingInhibitChanged(reply.value());
    }
    call->deleteLater();
}

void DeviceLock::trackBlankingInhibit()
{
    QDBusConnection::systemBus().connect(QString(), "/com/nokia/mce/signal", "com.nokia.mce.signal", "display_blanking_inhibit_ind", this, SLOT(handleBlankingInhibitChanged(QString)));

    QDBusMessage call = QDBusMessage::createMethodCall("com.nokia.mce", "/", "com.nokia.mce.request", "get_display_blanking_inhibit");
    QDBusPendingCall reply = QDBusConnection::systemBus().asyncCall(call);
    QDBusPendingCallWatcher *inhibitWatcher = new QDBusPendingCallWatcher(reply, this);
    connect(inhibitWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(handleBlankingInhibitReply(QDBusPendingCallWatcher*)));
}

// Blanking Pause

void DeviceLock::handleBlankingPauseChanged(const QString &state)
{
    qDebug() << state;

    bool blankingPause = (state == "active");
    if (m_blankingPause != blankingPause ) {
        m_blankingPause = blankingPause;
        emit blankingPauseChanged();
    }
}

void DeviceLock::handleBlankingPauseReply(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to mce failed:" << reply.error();
    } else {
        handleBlankingPauseChanged(reply.value());
    }
    call->deleteLater();
}

void DeviceLock::trackBlankingPause()
{
    // track blanking pause state
    QDBusConnection::systemBus().connect(QString(), "/com/nokia/mce/signal", "com.nokia.mce.signal", "display_blanking_pause_ind", this, SLOT(handleBlankingPauseChanged(QString)));

    QDBusMessage pauseCall = QDBusMessage::createMethodCall("com.nokia.mce", "/", "com.nokia.mce.request", "get_display_blanking_pause");
    QDBusPendingCall pauseReply = QDBusConnection::systemBus().asyncCall(pauseCall);
    QDBusPendingCallWatcher *pauseWatcher = new QDBusPendingCallWatcher(pauseReply, this);
    connect(pauseWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(handleBlankingPauseReply(QDBusPendingCallWatcher*)));
}

void DeviceLock::init()
{
    if (QFile(settingsFile).exists() && watcher.addPath(settingsFile)) {
        connect(&watcher, SIGNAL(fileChanged(QString)), this, SLOT(readSettings()));
        readSettings();
    }

    setState((lockingDelay<0) ? Unlocked : Locked);
}

void DeviceLock::setupLockTimer()
{
    // FIXME: unit test code wants to have both setupLockTimer() and
    //        setStateAndSetupLockTimer() defined and I do not want
    //        to meddle with the unit test logic
    setStateAndSetupLockTimer();
}

void DeviceLock::setStateAndSetupLockTimer()
{
    LockState requireState = deviceLockState;

    bool disabled = (lockingDelay < 0);
    bool immediate = (lockingDelay == 0);
    bool triggered = false;

    if (monoTime.tv_sec) {
        /* Timer was started, check if it should have already triggered */
        struct timeval compareTime;
        tv_get_monotime(&compareTime);
        if (lockingDelay*60 < tv_diff_in_s(&compareTime, &monoTime))
            triggered = true;

    }

    /* Check in what deviceLockState we ought to be in
     */
    if (deviceLockState == Undefined) {
        // Device lock state is not yet set
        // -> need to wait for init() to finish
    }
    else if (disabled) {
        // Device locking is disabled
        requireState = Unlocked;
    }
    else if (triggered) {
        // Timer should have already triggered
        requireState = Locked;
    }
    else if (immediate && !m_displayOn) {
        // Display is off in immediate lock mode
        requireState = Locked;
    }

    if (deviceLockState != requireState) {
        /* We should be in different deviceLockState. Set the
         * state and assume that setState() recurses back here
         * and we get to deal with the stable state
         */
        setState(requireState);
    }
    else {
        /* Start/stop device lock timer as needed
         */

        bool locked = (deviceLockState == Locked);
        bool active = (m_activity && m_displayOn);

        if (disabled || locked || active || m_activeCall) {
            if( monoTime.tv_sec ) {
                qDebug() << "stop device lock timer";
                lockTimer->stop();
                monoTime.tv_sec = 0;
            }
        }
        else {
            if( !monoTime.tv_sec ) {
                qDebug() << "start device lock timer";
                lockTimer->start(lockingDelay * 60 * 1000);
                tv_get_monotime(&monoTime);
            }
            else {
                qDebug() << "device lock timer already running";
                // FIXME: we should really reprogram the qtimer
                //        since it will trigger too late if we
                //        have been in suspend - or even better:
                //        use keepalive timer to begin with
            }
        }
    }
}

void DeviceLock::lock()
{
    setState(Locked);
}

int DeviceLock::state() const
{
    return deviceLockState;
}

bool  DeviceLock::blankingPause() const
{
    return m_blankingPause;
}

bool DeviceLock::blankingInhibit() const
{
    return m_blankingInhibit;
}

static const char *reprLockState(int state)
{
    switch (state) {
    case DeviceLock::Unlocked:  return "Unlocked";
    case DeviceLock::Locked:    return "Locked";
    case DeviceLock::Undefined: return "Undefined";
    default: break;
    }
    return "Invalid";
}

void DeviceLock::setState(int state)
{
    if (deviceLockState != (LockState)state) {
        if (state == Locked || isPrivileged()) {
            qDebug() << reprLockState(deviceLockState) <<
                " -> " << reprLockState(state);

            deviceLockState = (LockState)state;
            emit stateChanged(state);
            emit _notifyStateChanged();

            setStateAndSetupLockTimer();
        } else {
            sendErrorReply(QDBusError::AccessDenied, QString("Caller is not in privileged group"));
        }
    }
}

bool DeviceLock::checkCode(const QString &code)
{
    return runPlugin(QStringList() << "--check-code" << code);
}

bool DeviceLock::setCode(const QString &oldCode, const QString &newCode)
{
    return runPlugin(QStringList() << "--set-code" << oldCode << newCode);
}

bool DeviceLock::isSet() {
    return runPlugin(QStringList() << "--is-set" << "lockcode");
}

bool DeviceLock::runPlugin(const QStringList &args)
{
    QSettings settings("/usr/share/lipstick/devicelock/devicelock.conf", QSettings::IniFormat);
    QString pluginName = settings.value("DeviceLock/pluginName").toString();

    if (pluginName.isEmpty()) {
        qWarning("No plugin configuration set in /usr/share/lipstick/devicelock/devicelock.conf");
        return false;
    }

    QProcess process;
    process.start(pluginName, args);
    if (!process.waitForFinished()) {
        qWarning("Plugin did not finish in time");
        return false;
    }

#ifdef DEBUG_DEVICELOCK
    qDebug() << process.readAllStandardOutput();
    qWarning() << process.readAllStandardError();
#endif

    return process.exitCode() == 0;
}

void DeviceLock::readSettings()
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    if( isSet() )
        lockingDelay = settings.value(lockingKey, "-1").toInt();
    else
        lockingDelay = -1;
    setStateAndSetupLockTimer();
}

bool DeviceLock::isPrivileged()
{
    pid_t pid = -1;
    if (!calledFromDBus()) {
        // Local function calls are always privileged
        return true;
    }
    // Get the PID of the calling process
    pid = connection().interface()->servicePid(message().service());
    // The /proc/<pid> directory is owned by EUID:EGID of the process
    QFileInfo info(QString("/proc/%1").arg(pid));
    if (info.group() != "privileged" && info.group() != "disk" && info.owner() != "root") {
        return false;
    }
    return true;
}
