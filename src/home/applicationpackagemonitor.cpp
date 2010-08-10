/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of mhome.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/
#include "applicationpackagemonitor.h"
#include "launcherdatastore.h"
#include <QDir>
#include <QDBusConnection>
#include <mdesktopentry.h>
#include <mfiledatastore.h>
#include <msubdatastore.h>

static const QString PACKAGE_MANAGER_DBUS_SERVICE="com.nokia.package_manager";
static const QString PACKAGE_MANAGER_DBUS_PATH="/com/nokia/package_manager";
static const QString PACKAGE_MANAGER_DBUS_INTERFACE="com.nokia.package_manager";

static const QString OPERATION_INSTALL = "Install";
static const QString OPERATION_UNINSTALL = "Uninstall";
static const QString OPERATION_REFRESH = "Refresh";
static const QString OPERATION_UPGRADE = "Upgrade";

static const QString PACKAGE_PREFIX = "Packages/";
static const QString INSTALLER_EXTRA = "installer-extra/";

static const QString CONFIG_PATH = "/.config/duihome";

static const QString PACKAGE_STATE_INSTALLED = "installed";

static const QString DESKTOP_ENTRY_KEY_PACKAGE_STATE = "PackageState";
static const QString DESKTOP_ENTRY_KEY_PACKAGE_NAME = "Package";
static const QString DESKTOP_ENTRY_GROUP_MEEGO = "X-MeeGo";

class ApplicationPackageMonitor::ExtraDirWatcher : public LauncherDataStore
{
public:
    ExtraDirWatcher(MDataStore *dataStore, const QString &directoryPath);
    ~ExtraDirWatcher();

protected:
    virtual bool isDesktopEntryValid(const MDesktopEntry &entry, const QStringList &acceptedTypes);
};


ApplicationPackageMonitor::ApplicationPackageMonitor()
: con(QDBusConnection::systemBus())
{
    con.connect(QString(),PACKAGE_MANAGER_DBUS_PATH, PACKAGE_MANAGER_DBUS_INTERFACE, "download_progress",
                    this, SLOT(packageDownloadProgress(const QString&, const QString&, const QString&, int, int)));
    con.connect(QString(),PACKAGE_MANAGER_DBUS_PATH, PACKAGE_MANAGER_DBUS_INTERFACE, "operation_started",
                    this, SLOT(packageOperationStarted(const QString&, const QString&, const QString&)));
    con.connect(QString(),PACKAGE_MANAGER_DBUS_PATH, PACKAGE_MANAGER_DBUS_INTERFACE, "operation_progress",
                    this, SLOT(packageOperationProgress(const QString&, const QString &, const QString&, int)));
    con.connect(QString(),PACKAGE_MANAGER_DBUS_PATH, PACKAGE_MANAGER_DBUS_INTERFACE, "operation_complete",
                    this, SLOT(packageOperationComplete(const QString&, const QString&, const QString&, const QString&, bool)));

    QString configPath = QDir::homePath() + CONFIG_PATH;

    if (!QDir::root().exists(configPath)) {
        QDir::root().mkpath(configPath);
    }

    QString dataStoreFileName = configPath + "/applicationpackage.data";

    dataStore = new MFileDataStore(dataStoreFileName);

    // ExtraDirWatcher takes ownership of dataStore
    extraDirWatcher = QSharedPointer<ExtraDirWatcher>(new ExtraDirWatcher(dataStore, APPLICATIONS_DIRECTORY+INSTALLER_EXTRA));

    connect(extraDirWatcher.data(), SIGNAL(desktopEntryAdded(QString)), this, SLOT(updatePackageState(QString)), Qt::UniqueConnection);
    connect(extraDirWatcher.data(), SIGNAL(desktopEntryChanged(QString)), this, SLOT(updatePackageState(QString)), Qt::UniqueConnection);
}

ApplicationPackageMonitor::~ApplicationPackageMonitor()
{
}

ApplicationPackageMonitor::PackageProperties &ApplicationPackageMonitor::activePackageProperties(const QString packageName)
{
    if (!activePackages.contains(packageName)) {
        // Set the desktopEntryName if already known
        activePackages[packageName].desktopEntryName = desktopEntryName(packageName);
    }

    return activePackages[packageName];
}

void ApplicationPackageMonitor::packageDownloadProgress(const QString &operation,
                                    const QString &packageName,
                                    const QString &packageVersion,
                                    int already, int total)
{
    Q_UNUSED(operation)
    Q_UNUSED(packageVersion)

    PackageProperties &properties = activePackageProperties(packageName);

    if (isValidOperation(properties, operation)) {
        emit downloadProgress(packageName, properties.desktopEntryName, already, total);
    }
}

void ApplicationPackageMonitor::packageOperationStarted(const QString &operation,
                                const QString &packageName,
                                const QString &version)
{
    Q_UNUSED(operation)
    Q_UNUSED(packageName)
    Q_UNUSED(version)
}

void ApplicationPackageMonitor::packageOperationProgress(const QString &operation,
                                const QString &packageName,
                                const QString &packageVersion,
                                int percentage)
{
    Q_UNUSED(packageVersion)

    PackageProperties &properties = activePackageProperties(packageName);

    if (isValidOperation(properties, operation)) {
        properties.installing = true;
        emit installProgress(packageName, properties.desktopEntryName, percentage);
    }
}

void ApplicationPackageMonitor::packageOperationComplete(const QString &operation,
                                const QString &packageName,
                                const QString &packageVersion,
                                const QString &error,
                                bool need_reboot)
{
    Q_UNUSED(packageVersion)
    Q_UNUSED(need_reboot)
    Q_UNUSED(operation)

    PackageProperties &properties = activePackageProperties(packageName);

    if (isValidOperation(properties, operation)) {
        if (!error.isEmpty()) {
            emit operationError(packageName, properties.desktopEntryName, error);
        } else if (activePackages[packageName].installing) {
            emit operationSuccess(packageName,
                                  properties.desktopEntryName.replace(INSTALLER_EXTRA, QString()));
        }
    }

    activePackages.remove(packageName);
}

bool ApplicationPackageMonitor::isValidOperation(const PackageProperties &properties, const QString &operation)
{
    if ((operation.compare(OPERATION_INSTALL, Qt::CaseInsensitive) == 0 ||
           operation.compare(OPERATION_UPGRADE, Qt::CaseInsensitive) == 0 ) &&
        !properties.desktopEntryName.isEmpty() ) {
         return true;
    } else {
         return false;
    }
}

void ApplicationPackageMonitor::updatePackageState(const QString &desktopEntryPath)
{
    MDesktopEntry entry(desktopEntryPath);

    QString packageName = entry.value(DESKTOP_ENTRY_GROUP_MEEGO, DESKTOP_ENTRY_KEY_PACKAGE_NAME);
    QString packageState = entry.value(DESKTOP_ENTRY_GROUP_MEEGO, DESKTOP_ENTRY_KEY_PACKAGE_STATE);

    if (!packageName.isEmpty()) {
        QString pkgKey = PACKAGE_PREFIX+packageName;

        if (packageState == "installable") {
            dataStore->remove(pkgKey);
        } else {
            dataStore->createValue(pkgKey, desktopEntryPath);

            if (activePackages.contains(packageName)) {
                // Package is being installed, add the desktop entry path directly
                activePackages[packageName].desktopEntryName = desktopEntryPath;
            }
        }
    }

    extraDirWatcher->updateDataForDesktopEntry(desktopEntryPath, packageState);
}

QString ApplicationPackageMonitor::desktopEntryName(const QString &packageName)
{
    QString pkgKey = PACKAGE_PREFIX+packageName;

    if (!dataStore->contains(pkgKey)) {
        // Package not installed
        return QString();
    }

    if (!QFile::exists(dataStore->value(pkgKey).toString())) {
        // The extra desktop file doesn't exist anymore, the package has been uninstalled
        dataStore->remove(pkgKey);
        return QString();
    }

    return dataStore->value(pkgKey).toString();
}

ApplicationPackageMonitor::ExtraDirWatcher::ExtraDirWatcher(MDataStore *dataStore, const QString &directoryPath) :
    LauncherDataStore(dataStore, directoryPath)
{
}

ApplicationPackageMonitor::ExtraDirWatcher::~ExtraDirWatcher()
{
}

bool ApplicationPackageMonitor::ExtraDirWatcher::isDesktopEntryValid(const MDesktopEntry &entry, const QStringList &acceptedTypes)
{
    Q_UNUSED(acceptedTypes);

    return entry.contains(DESKTOP_ENTRY_GROUP_MEEGO, DESKTOP_ENTRY_KEY_PACKAGE_NAME)
        && entry.contains(DESKTOP_ENTRY_GROUP_MEEGO, DESKTOP_ENTRY_KEY_PACKAGE_STATE);
}
