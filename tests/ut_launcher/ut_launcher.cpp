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

#include <QtTest/QtTest>
#include <MDesktopEntry>
#include <MApplicationIfProxy>
#include "ut_launcher.h"
#include "launcher.h"
#include "launcherpage.h"
#include "windowinfo_stub.h"
#include "launcherbutton_stub.h"
#include "launcherdatastore_stub.h"
#include "applicationpackagemonitor_stub.h"
#include "homewindowmonitor.h"
#include "launchermodel.h"
#include "launcherpagemodel.h"
#include "mockdatastore.h"

const static int BUTTONS_PER_PAGE = 12;
const static QString INSTALLER_EXTRA_PATH = APPLICATIONS_DIRECTORY + ApplicationPackageMonitor::INSTALLER_EXTRA_FOLDER;

QString qProcessProgramStarted;
bool QProcess::startDetached(const QString &program)
{
    qProcessProgramStarted = program;

    return true;
}

bool QDBusConnection::isConnected() const
{
    return true;
}

void Ut_Launcher::addButtonsToLauncher(int amount)
{
    for (int i = 0; i < amount; i++) {
        QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry(QString("testApp%1.desktop").arg(i)));
        launcher->addLauncherButton(desktopEntry);
    }
}

void Ut_Launcher::addButtonsToLauncherAndSetDataForAllDesktopEntries(int amount)
{
    QHash<QString, QVariant> dataForAllDesktopEntries;
    int page = 0;
    int pos = 0;
    for (int i = 0; i < amount; i++) {
        if (pos >= BUTTONS_PER_PAGE) {
            page++;
            pos = i - (page * BUTTONS_PER_PAGE);
        }
        QString testApp = QString("testApp%1%2.desktop").arg(page).arg(pos);
        QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry(testApp));
        launcher->addLauncherButton(desktopEntry);
        dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + testApp, QVariant(QString("launcher/%1/%2").arg(page).arg(pos)));
        pos++;
    }
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
}

// QFile stubs
bool fileExists;
bool QFileInfo::exists() const
{
    return fileExists;
}

// Tests
void Ut_Launcher::initTestCase()
{
}

void Ut_Launcher::cleanupTestCase()
{
}

void Ut_Launcher::init()
{
    // Set up the launcher data store
    launcherDataStore = new LauncherDataStore(new MockDataStore, QStringList());
    packageMonitor = new ApplicationPackageMonitor();
    // Create a launcher and connect the signals
    launcher = new Launcher;
    launcher->setLauncherDataStore(launcherDataStore);
    launcher->setApplicationPackageMonitor(packageMonitor);
    connect(this, SIGNAL(directoryChanged(const QString)), launcher, SLOT(updatePagesFromDataStore()));

    qProcessProgramStarted.clear();

    gLauncherButtonStub->stubReset();
    gLauncherDataStoreStub->stubReset();
    gApplicationPackageMonitorStub->stubReset();

    fileExists = true;
}

void Ut_Launcher::cleanup()
{
    delete launcher;
    delete launcherDataStore;
    delete packageMonitor;
    gLauncherDataStoreStub->stubReset();
}

int Ut_Launcher::buttonsCount()
{
    int count = 0;
    foreach (QSharedPointer<LauncherPage> page, launcher->model()->launcherPages()) {
        count += page.data()->model()->launcherButtons().count();
    }
    return count;
}

void Ut_Launcher::createdDefaultSetOfDesktopEntries()
{
    QHash<QString, QVariant> dataForAllDesktopEntries;
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp00.desktop", QVariant("launcher/0/0"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp10.desktop", QVariant("launcher/1/0"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp20.desktop", QVariant("launcher/2/0"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp21.desktop", QVariant("launcher/2/1"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp22.desktop", QVariant("launcher/2/2"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp30.desktop", QVariant("launcher/3/0"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp40.desktop", QVariant("launcher/4/0"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp41.desktop", QVariant("launcher/4/1"));
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + "testApp50.desktop", QVariant("launcher/5/0"));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);

    gLauncherButtonStub->stubSetReturnValueList("desktopEntry", dataForAllDesktopEntries.keys());
}

void Ut_Launcher::comparePageNumberArgument(QSignalSpy &spy, int page)
{
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), page);
}

void Ut_Launcher::testPaging()
{
    // Initially there should be no buttons
    QCOMPARE(buttonsCount(), 0);

    // Add some desktop files in the "desktop file directory"
    QHash<QString, QVariant> dataForAllDesktopEntries;
    dataForAllDesktopEntries.insert("noPlacement", QVariant());
    dataForAllDesktopEntries.insert("unknownPlacement", QVariant("unknown"));
    dataForAllDesktopEntries.insert("validPlacement1", QVariant("launcher/0/0"));
    dataForAllDesktopEntries.insert("validPlacement2", QVariant("launcher/1/1"));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);

    // Fake a directory change notification
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    // There should be one button on the first page and 2 buttons on the second
    QCOMPARE(buttonsCount(), 3);
    QCOMPARE(launcher->model()->launcherPages().count(), 2);
    QCOMPARE(launcher->model()->launcherPages().at(0).data()->model()->launcherButtons().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(1).data()->model()->launcherButtons().count(), 2);

}

void Ut_Launcher::testEmptyPage()
{
    // Add 25 desktop files in the "desktop file directory"
    QHash<QString, QVariant> dataForAllDesktopEntries;
    for (int i = 0; i < 25; i++) {
        dataForAllDesktopEntries.insert(QString("noPlacement%1").arg(i), QVariant());
    }
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);

    // Fake a directory change notification
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    // There should be three launcher pages with BUTTONS_PER_PAGE, BUTTONS_PER_PAGE and 1 items on them
    QCOMPARE(launcher->model()->launcherPages().count(), 3);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
    QCOMPARE(launcher->model()->launcherPages().at(2)->model()->launcherButtons().count(), 1);

    // Don't put desktop files 12-23 in the "desktop file directory"
    dataForAllDesktopEntries.clear();
    for (int i = 0; i < 25; i++) {
        if (i < 12 || i > 23) {
            dataForAllDesktopEntries.insert(QString("noPlacement%1").arg(i), QVariant());
        }
    }
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);

    // Fake a directory change notification
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    // There should be two launcher pages with BUTTONS_PER_PAGE and 1 buttons on them
    QCOMPARE(launcher->model()->launcherPages().count(), 2);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), 1);
}

void Ut_Launcher::testButtonPlacement()
{
    QString testAppName("testApp.desktop");
    QString badAppName("nonExistentApp.desktop");
    QString emptyAppName("");
    QString fullPathName = APPLICATIONS_DIRECTORY + testAppName;

    addButtonsToLauncher(6);
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", fullPathName);

    Launcher::Placement placement = launcher->buttonPlacement(fullPathName);
    QCOMPARE(placement.page, 0);
    QCOMPARE(placement.position, 0);

    placement = launcher->buttonPlacement(testAppName);
    QCOMPARE(placement.page, 0);
    QCOMPARE(placement.position, 0);

    placement = launcher->buttonPlacement(badAppName);
    QCOMPARE(placement.page, -1);
    QCOMPARE(placement.position, -1);

    placement = launcher->buttonPlacement(emptyAppName);
    QCOMPARE(placement.page, -1);
    QCOMPARE(placement.position, -1);
}

void Ut_Launcher::testFocusToButton()
{
    QSignalSpy spy(launcher, SIGNAL(focusToButtonRequested(const QString &)));

    QString testAppName("testApp.desktop");
    QString badAppName("nonExistentApp.desktop");
    QString fullPathName = APPLICATIONS_DIRECTORY + testAppName;

    addButtonsToLauncher(6);
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", fullPathName);

    int result = launcher->focusToButton(fullPathName);
    QCOMPARE(result, 0);
    comparePageNumberArgument(spy, 0);

    result = launcher->focusToButton(badAppName);
    QCOMPARE(result, -1);
    QCOMPARE(spy.count(), 0);
}

void Ut_Launcher::testUpdatingLauncherButton()
{
    createdDefaultSetOfDesktopEntries();
    // Fake a directory change notification
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY) + "testApp20.desktop"));
    launcher->updateLauncherButton(desktopEntry);

    QCOMPARE(gLauncherButtonStub->stubCallCount("updateFromDesktopEntry"), 1);
}

void Ut_Launcher::testUpdatingLauncherButtonFromInstallerExtraFolder()
{
    // Initialize launcher with two buttons of which one is in installer-extra folder
    QString installerExtraButtonEntry = INSTALLER_EXTRA_PATH + "testApp2.desktop";
    QString applicationsButtonEntry = QString(APPLICATIONS_DIRECTORY) + "testApp1.desktop";

    QHash<QString, QVariant> dataForAllDesktopEntries;
    dataForAllDesktopEntries.insert(applicationsButtonEntry, QVariant("launcher/0/0"));
    dataForAllDesktopEntries.insert(installerExtraButtonEntry, QVariant("launcher/0/1"));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
    launcher->updatePagesFromDataStore();

    QList<QString> entries;
    entries << applicationsButtonEntry << installerExtraButtonEntry;
    gLauncherButtonStub->stubSetReturnValueList("desktopEntry", entries);

    gApplicationPackageMonitorStub->stubSetReturnValue("isInstallerExtraEntry", true);

    QSharedPointer<MDesktopEntry> updateButtonEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY) + "testApp2.desktop"));
    launcher->updateLauncherButton(updateButtonEntry);

    QCOMPARE(gLauncherDataStoreStub->stubCallCount("removeDataForDesktopEntry"), 2);
    QCOMPARE(gLauncherDataStoreStub->stubCallCount("updateDataForDesktopEntry"), 1);
    QCOMPARE(gLauncherButtonStub->stubCallCount("updateFromDesktopEntry"), 1);
}

void Ut_Launcher::testAddingButtons()
{
    addButtonsToLauncher(BUTTONS_PER_PAGE);

    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
}

void Ut_Launcher::testAddingButtonsOnMultiplePages()
{
    int buttonCountOnSecondPage = 3;
    int addedButtons = BUTTONS_PER_PAGE + buttonCountOnSecondPage;
    addButtonsToLauncher(addedButtons);

    QCOMPARE(launcher->model()->launcherPages().count(), 2);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), buttonCountOnSecondPage);
}

void Ut_Launcher::testAddingButtonsWithExistingButtons()
{
    // Add some existing buttons
    int initialButtons = BUTTONS_PER_PAGE/2;
    QHash<QString, QVariant> dataForAllDesktopEntries;
    for (int i = 0; i < initialButtons; i++) {
        dataForAllDesktopEntries.insert(QString("noPlacement%1").arg(i), QVariant());
    }

    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    // adding more buttons
    int addedButtons = BUTTONS_PER_PAGE;
    int buttonCountOnSecondPage = initialButtons;
    addButtonsToLauncher(addedButtons);

    QCOMPARE(launcher->model()->launcherPages().count(), 2);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), buttonCountOnSecondPage);
}

void Ut_Launcher::testUpdatingButtonPlacementsOnPage()
{
    // adding more than one page
    int buttonCountOnFirstPage = BUTTONS_PER_PAGE;
    int buttonCountOnSecondPage = 2;
    int buttonCount = BUTTONS_PER_PAGE + buttonCountOnSecondPage;
    addButtonsToLauncher(buttonCount);

    // remove 2 buttons from 1st page
    buttonCountOnFirstPage = BUTTONS_PER_PAGE - 2;

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", QString("testApp3.desktop"));
    launcher->removeLauncherButton("testApp3.desktop");

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", QString("testApp4.desktop"));
    launcher->removeLauncherButton("testApp4.desktop");

    QCOMPARE(launcher->model()->launcherPages().count(), 2);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), buttonCountOnFirstPage);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), buttonCountOnSecondPage);
}

void Ut_Launcher::testWhenMovingButtonsThenNewPlacementsAreStored()
{
    // Add some content to page
    addButtonsToLauncher(BUTTONS_PER_PAGE);
    QList<QString> desktopEntries;
    QList<QString> expectedValues;
    for (int i = 0; i < BUTTONS_PER_PAGE; i++) {
        desktopEntries.append(QString("testApp%1.desktop").arg(i));
        expectedValues.append(QString("launcher/0/%1").arg(i));
    }

    // Move some buttons around
    desktopEntries.move(10, 0);
    desktopEntries.move(4, 5);

    gLauncherButtonStub->stubSetReturnValueList("desktopEntry", desktopEntries);

    launcher->updateButtonPlacementsOnPage(launcher->model()->launcherPages().at(0).data());

    QHash<QString, QString> storedEntries = gLauncherDataStoreStub->stubLastCallTo("updateDataForDesktopEntries").parameter<QHash<QString, QString> >(0);
    int i = 0;
    foreach (const QString &entry, desktopEntries) {
        QCOMPARE(storedEntries.value(entry), expectedValues[i]);
        i++;
    }

    // Move some more buttons around
    desktopEntries.move(3, 0);
    desktopEntries.move(6, 10);
    desktopEntries.move(2, 1);
    gLauncherButtonStub->stubSetReturnValueList("desktopEntry", desktopEntries);

    launcher->updateButtonPlacementsOnPage(launcher->model()->launcherPages().at(0).data());

    storedEntries = gLauncherDataStoreStub->stubLastCallTo("updateDataForDesktopEntries").parameter<QHash<QString, QString> >(0);
    i = 0;
    foreach (const QString &entry, desktopEntries) {
        QCOMPARE(storedEntries.value(entry), expectedValues[i]);
        i++;
    }
}

void Ut_Launcher::testRemovingButtons()
{
    // adding more than one page
    int buttonCountOnFirstPage = BUTTONS_PER_PAGE;
    int buttonCountOnSecondPage = 2;
    int buttonCount = BUTTONS_PER_PAGE + buttonCountOnSecondPage;
    addButtonsToLauncher(buttonCount);

    // remove 2 buttons from 1st page
    buttonCountOnFirstPage = BUTTONS_PER_PAGE - 2;

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", QString("testApp3.desktop"));
    launcher->removeLauncherButton("testApp3.desktop");

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", QString("testApp4.desktop"));
    launcher->removeLauncherButton("testApp4.desktop");

    QCOMPARE(launcher->model()->launcherPages().count(), 2);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), buttonCountOnFirstPage);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), buttonCountOnSecondPage);
}

void Ut_Launcher::testSettingLauncherToFirstPage()
{
    QSignalSpy spy(launcher, SIGNAL(focusToPageRequested(uint)));
    launcher->setPage(0);
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toUInt(), (uint)0);
}

void Ut_Launcher::testUpdateButtonState()
{
    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry("/dev/null/test.desktop"));
    const QString packageName = "pkg";
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", desktopEntry->fileName());

    //First update state for button that doesn't exist in the launcher or datastore
    launcher->updateButtonState(desktopEntry, packageName, ApplicationPackageMonitor::PACKAGE_STATE_DOWNLOADING, true);
    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), 1);
    QCOMPARE(gLauncherButtonStub->stubCallCount("setState"), 1);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setState").parameter<LauncherButtonModel::State>(0), LauncherButtonModel::Downloading);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setPackageName").parameter<QString>(0), packageName);

    //Update button's state and progress
    launcher->updateButtonState(desktopEntry, packageName, ApplicationPackageMonitor::PACKAGE_STATE_INSTALLING, true);
    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), 1);
    QCOMPARE(gLauncherButtonStub->stubCallCount("setState"), 2);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setState").parameter<LauncherButtonModel::State>(0), LauncherButtonModel::Installing);
}

void Ut_Launcher::testUpdateButtonProgress()
{
    addButtonsToLauncher(1);

    const QString desktopEntry = "/dev/null/test.desktop";
    int progress = 50;
    int maximumProgress = 100;
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", desktopEntry);

    launcher->updateProgress(desktopEntry, progress, maximumProgress);
    QCOMPARE(gLauncherButtonStub->stubCallCount("setOperationProgress"), 1);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setOperationProgress").parameter<int>(0), progress);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setOperationProgress").parameter<int>(1), maximumProgress);

    progress = 100;
    launcher->updateProgress(desktopEntry, progress, maximumProgress);
    QCOMPARE(gLauncherButtonStub->stubCallCount("setOperationProgress"), 2);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setOperationProgress").parameter<int>(0), progress);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setOperationProgress").parameter<int>(1), maximumProgress);
}

void Ut_Launcher::testUpdateButtonStateForButtonInLauncher()
{
    addButtonsToLauncher(1);

    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry("/dev/null/testApp0.desktop"));
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", desktopEntry->fileName());
    launcher->updateButtonState(desktopEntry, QString(), ApplicationPackageMonitor::PACKAGE_STATE_INSTALLING, true);
    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), 1);
    QCOMPARE(gLauncherButtonStub->stubCallCount("setState"), 1);
    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setState").parameter<LauncherButtonModel::State>(0), LauncherButtonModel::Installing);
}

void Ut_Launcher::testUpdateButtonStateForButtonThatHasOtherLocationInDatastore()
{
    //Add button to datastore
    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry("test.desktop"));
    QHash<QString, QVariant> dataForAllDesktopEntries;
    dataForAllDesktopEntries.insert(QString(APPLICATIONS_DIRECTORY) + QString(desktopEntry->fileName()), QVariant("quicklaunchbar/0/0"));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
    //Shouldn't get added to map since it exists in datastore
    launcher->updateButtonState(desktopEntry, QString(), ApplicationPackageMonitor::PACKAGE_STATE_INSTALLING, true);
    QCOMPARE(gLauncherButtonStub->stubCallCount("setState"), 0);
}

void Ut_Launcher::testSettingPackageNameForButtonWhenButtonStateIsUpdated()
{
    const QString packageName = "pkg";
    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry("/dev/null/test.desktop"));
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", desktopEntry->fileName());
    gLauncherButtonStub->stubSetReturnValue("packageName", QString());

    launcher->updateButtonState(desktopEntry, packageName, ApplicationPackageMonitor::PACKAGE_STATE_DOWNLOADING, true);

    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setPackageName").parameter<QString>(0), packageName);
}

void Ut_Launcher::testSettingPackageRemovabilityForButtonWhenButtonStateIsUpdated()
{
    const QString packageName = "pkg";
    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry("/dev/null/test.desktop"));
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", desktopEntry->fileName());
    gLauncherButtonStub->stubSetReturnValue("packageName", QString());
    gLauncherButtonStub->stubSetReturnValue("packageRemovable", false);

    launcher->updateButtonState(desktopEntry, packageName, ApplicationPackageMonitor::PACKAGE_STATE_DOWNLOADING, true);

    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setPackageRemovable").parameter<bool>(0), true);
}

void Ut_Launcher::testAddPlaceholderButtonToLauncher()
{
    QVERIFY(launcher->model()->launcherPages().isEmpty());
    QSharedPointer<MDesktopEntry> entry1(new MDesktopEntry("entry1.desktop"));

    // add a new placeholder button
    launcher->placeholderButton(entry1);
    launcher->model()->launcherPages().at(0)->model();
    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), 1);

    // add a new placeholder button
    QSharedPointer<MDesktopEntry> entry2(new MDesktopEntry("entry2.desktop"));
    launcher->placeholderButton(entry2);
    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), 2);
}

void Ut_Launcher::testEntryPlamenentInDatastoreMethod()
{
    QHash<QString, QVariant> dataForAllDesktopEntries;
    QString location = "launcher";
    int page = 1;
    int position = 1;
    dataForAllDesktopEntries.insert("validPlacement", QVariant(QString("%1/%2/%3").arg(location).arg(page).arg(position)));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
    Launcher::Placement placement = launcher->entryPlacementInDatastore("validPlacement");
    QCOMPARE(placement.location, location);
    QCOMPARE(placement.page, page);
    QCOMPARE(placement.position, position);
}

void Ut_Launcher::testSetMaximumPageSize()
{
    int maximumPageSize = 4;
    launcher->setMaximumPageSize(maximumPageSize);

    // Add 9 desktop files in the "desktop file directory"
    QHash<QString, QVariant> dataForAllDesktopEntries;
    for (int i = 0; i < 9; i++) {
        dataForAllDesktopEntries.insert(QString("noPlacement%1").arg(i), QVariant());
    }
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);

    // Fake a directory change notification
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    // There should be three launcher pages with maximumPageSize, maximumPageSize and 1 items on them
    QCOMPARE(launcher->model()->launcherPages().count(), 3);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), maximumPageSize);
    QCOMPARE(launcher->model()->launcherPages().at(1)->model()->launcherButtons().count(), maximumPageSize);
    QCOMPARE(launcher->model()->launcherPages().at(2)->model()->launcherButtons().count(), 1);
}

void Ut_Launcher::testSetOperationErrorWhenButtonFoundFromLauncher()
{
    QSharedPointer<MDesktopEntry> applicationsEntry(new MDesktopEntry("/applications/test.desktop"));
    QSharedPointer<MDesktopEntry> installerExtraEntry(new MDesktopEntry("/applications/installer-extra/test.desktop"));

    launcher->addLauncherButton(applicationsEntry);
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", applicationsEntry->fileName());
    gApplicationPackageMonitorStub->stubSetReturnValue("toInstallerExtraEntryPath", installerExtraEntry->fileName());
    gApplicationPackageMonitorStub->stubSetReturnValue("toApplicationsEntryPath", applicationsEntry->fileName());

    gApplicationPackageMonitorStub->stubSetReturnValue("isInstallerExtraEntry", true);
    launcher->updateButtonState(installerExtraEntry, QString(), ApplicationPackageMonitor::PACKAGE_STATE_BROKEN, true);

    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setState").parameter<LauncherButtonModel::State>(0), LauncherButtonModel::Broken);
    QCOMPARE(gLauncherButtonStub->stubCallCount("updateFromDesktopEntry"), 0);
}

void Ut_Launcher::testButtonInfoIsUpdatedFromDesktopEntryWhenButtonStateIsUpdatedFromApplicationsEntry()
{
    QSharedPointer<MDesktopEntry> installedApplicationsEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY) + "test.desktop"));

    gApplicationPackageMonitorStub->stubSetReturnValue("toApplicationsEntryPath", installedApplicationsEntry->fileName());
    gApplicationPackageMonitorStub->stubSetReturnValue("isInstallerExtraEntry", true);
    fileExists = true;

    launcher->updateButtonState(installedApplicationsEntry, "package", ApplicationPackageMonitor::PACKAGE_STATE_INSTALLED, true);

    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setState").parameter<LauncherButtonModel::State>(0), LauncherButtonModel::Installed);
    QCOMPARE(gLauncherButtonStub->stubCallCount("updateFromDesktopEntry"), 1);

    QCOMPARE(gLauncherDataStoreStub->stubLastCallTo("removeDataForDesktopEntry").parameter<QString>(0), installedApplicationsEntry->fileName());
    QCOMPARE(gLauncherDataStoreStub->stubLastCallTo("updateDataForDesktopEntry").parameter<QString>(0), installedApplicationsEntry->fileName());
}

void Ut_Launcher::testSetOperationErrorWhenButtonHasPlaceholder()
{
    // add a new placeholder button
    QSharedPointer<MDesktopEntry> entry(new MDesktopEntry("test.desktop"));
    launcher->placeholderButton(entry);

    launcher->updateButtonState(entry, QString(), ApplicationPackageMonitor::PACKAGE_STATE_BROKEN, true);

    QCOMPARE(gLauncherButtonStub->stubLastCallTo("setState").parameter<LauncherButtonModel::State>(0), LauncherButtonModel::Broken);
}

void Ut_Launcher::testRemovingPlaceholderMapEntryWhenButtonIsRemoved()
{
    QSharedPointer<MDesktopEntry> installExtraEntry(new MDesktopEntry("install-extra/testRemoveInstallExtra.desktop"));
    QSharedPointer<MDesktopEntry> applicationsEntry(new MDesktopEntry("applications/testRemoveInstallExtra.desktop"));

    addButtonsToLauncher(BUTTONS_PER_PAGE-1);
    // add a new placeholder button
    launcher->placeholderButton(installExtraEntry);
    launcher->addLauncherButton(applicationsEntry);

    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", applicationsEntry->fileName());
    launcher->removeLauncherButton(applicationsEntry->fileName());

    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE-1);
}

void Ut_Launcher::testRemovingLauncherButtonPlaceholderWhenInstallExtraEntryIsRemoved()
{
    QSharedPointer<MDesktopEntry> entry(new MDesktopEntry("testRemoveInstallExtra.desktop"));
    addButtonsToLauncher(BUTTONS_PER_PAGE-1);

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", QString(""));

    // add a new placeholder button
    launcher->placeholderButton(entry);
    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", entry->fileName());
    launcher->removePlaceholderButton(entry->fileName());

    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE-1);
}

void Ut_Launcher::testWhenApplicationIsInstalledAndInstallExtraEntryIsRemovedThenLauncherButtonIsNotRemoved()
{
    QSharedPointer<MDesktopEntry> installedEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY)+ "testRemoveInstallExtra.desktop"));
    QSharedPointer<MDesktopEntry> placeholderEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY)+ "extra/"+"testRemoveInstallExtra.desktop"));
    addButtonsToLauncherAndSetDataForAllDesktopEntries(BUTTONS_PER_PAGE-1);

    QHash<QString, QVariant> dataStore = launcher->dataStore->dataForAllDesktopEntries();
    dataStore.insert(installedEntry->fileName(), QVariant("launcher/0/11"));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataStore);

    // Application is installed and has a launcher button and desktop entry.
    launcher->addLauncherButton(installedEntry);

    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);

    // launcher button is already in launcher
    // this sets LauncherPage::launcherButtonPosition to find button
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", installedEntry->fileName());

    launcher->removePlaceholderButton(placeholderEntry->fileName());

    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
}

void Ut_Launcher::testRemovingNonExistentPlaceholderButton()
{
    addButtonsToLauncherAndSetDataForAllDesktopEntries(BUTTONS_PER_PAGE-1);

    QSharedPointer<MDesktopEntry> entry(new MDesktopEntry("test.desktop"));
    // add a new placeholder button
    launcher->placeholderButton(entry);

    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", QString("test.desktop"));
    launcher->removePlaceholderButton("nonExistent.desktop");

    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), BUTTONS_PER_PAGE);
}

void Ut_Launcher::testOperationSuccessForButtonWithoutDesktopEntry()
{
    addButtonsToLauncherAndSetDataForAllDesktopEntries(1);

    QSharedPointer<MDesktopEntry> installedApplicationsEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY) + "testApp00.desktop"));

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", installedApplicationsEntry->fileName());
    gApplicationPackageMonitorStub->stubSetReturnValue("toApplicationsEntryPath", installedApplicationsEntry->fileName());
    gApplicationPackageMonitorStub->stubSetReturnValue("isInstallerExtraEntry", true);
    fileExists = false;

    launcher->updateButtonState(installedApplicationsEntry, "package", ApplicationPackageMonitor::PACKAGE_STATE_INSTALLED, true);

    QCOMPARE(launcher->model()->launcherPages().count(), 0);
}

void Ut_Launcher::testConnectionsAfterLauncherInitialization()
{
    launcher->updatePagesFromDataStore();

    // Connections to launcher data store
    QVERIFY(connect(launcherDataStore, SIGNAL(dataStoreChanged()), launcher, SLOT(updatePagesFromDataStore())));
    QVERIFY(disconnect(launcherDataStore, SIGNAL(desktopEntryAdded(QSharedPointer<MDesktopEntry>)), launcher, SLOT(addLauncherButton(QSharedPointer<MDesktopEntry>))));
    QVERIFY(disconnect(launcherDataStore, SIGNAL(desktopEntryRemoved(QString)), launcher, SLOT(removeLauncherButton(QString))));
    QVERIFY(disconnect(launcherDataStore, SIGNAL(desktopEntryChanged(QSharedPointer<MDesktopEntry>)), launcher, SLOT(updateLauncherButton(QSharedPointer<MDesktopEntry>))));

    // Connections to package monitor
    QVERIFY(disconnect(packageMonitor, SIGNAL(packageStateUpdated(QSharedPointer<MDesktopEntry>, QString, QString, bool)),
            launcher, SLOT(updateButtonState(QSharedPointer<MDesktopEntry>, QString, QString, bool))));
    QVERIFY(disconnect(packageMonitor, SIGNAL(installExtraEntryRemoved(QString)),
            launcher, SLOT(removePlaceholderButton(QString))));
    QVERIFY(disconnect(packageMonitor, SIGNAL(downloadProgressUpdated(QString, int, int)),
            launcher, SLOT(updateProgress(QString, int, int))));
}

void Ut_Launcher::testInitializingLauncherWithButtonsInUnknownLocation()
{
    // Initialize desktop entries and restart launcher
    QHash<QString, QVariant> dataForAllDesktopEntries;
    for (int i = 0; i < 2; i++) {
        dataForAllDesktopEntries.insert(QString("noPlacement%1").arg(i), QVariant());
    }
    delete launcher;
    launcher = new Launcher;
    launcher->setLauncherDataStore(launcherDataStore);
    launcher->setApplicationPackageMonitor(packageMonitor);
    connect(this, SIGNAL(directoryChanged(const QString)), launcher, SLOT(updatePagesFromDataStore()));

    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
    emit directoryChanged(APPLICATIONS_DIRECTORY);

    QCOMPARE(launcher->model()->launcherPages().count(), 1);
    QCOMPARE(launcher->model()->launcherPages().at(0)->model()->launcherButtons().count(), 2);
}

void Ut_Launcher::testThatLauncherIconIsRemovedWhenApplicationUninstallProgressIsStarted()
{
    QSharedPointer<MDesktopEntry> uninstallableInstallerExtraEntry(new MDesktopEntry(INSTALLER_EXTRA_PATH + "uninstallableApp.desktop"));
    QSharedPointer<MDesktopEntry> uninstallableApplicationsEntry(new MDesktopEntry(QString(APPLICATIONS_DIRECTORY) + "uninstallableApp.desktop"));

    QHash<QString, QVariant> dataForAllDesktopEntries;
    dataForAllDesktopEntries.insert(uninstallableApplicationsEntry->fileName(), QVariant("launcher/0/0"));
    gLauncherDataStoreStub->stubSetReturnValue("dataForAllDesktopEntries", dataForAllDesktopEntries);
    launcher->updatePagesFromDataStore();

    gLauncherButtonStub->stubSetReturnValue("desktopEntry", uninstallableApplicationsEntry->fileName());
    launcher->updateButtonState(uninstallableInstallerExtraEntry, "testApp", ApplicationPackageMonitor::PACKAGE_STATE_UNINSTALLING, true);

    QCOMPARE(launcher->model()->launcherPages().count(), 0);
}

void Ut_Launcher::testWhenLauncherPageCreatedThenPruningConnectedToButtonRemoval()
{
    QSharedPointer<LauncherPage> page = launcher->createLauncherPage();

    QVERIFY(disconnect(page.data(), SIGNAL(buttonRemoved()), launcher, SLOT(prunePage())));
}

void Ut_Launcher::testPlaceholderButtonIsNotAddedIfApplicationDesktopEntryIsInvalid()
{
    QSharedPointer<MDesktopEntry> desktopEntry(new MDesktopEntry("/dev/null/test.desktop"));
    gLauncherButtonStub->stubSetReturnValue("desktopEntry", desktopEntry->fileName());
    gLauncherDataStoreStub->stubSetReturnValue("isDesktopEntryKnownToBeInvalid", true);

    // If the application desktop entry is invalid nothing should happen
    launcher->updateButtonState(desktopEntry, "pkg", ApplicationPackageMonitor::PACKAGE_STATE_INSTALLED, true);
    QCOMPARE(launcher->model()->launcherPages().count(), 0);
}

QTEST_MAIN(Ut_Launcher)
