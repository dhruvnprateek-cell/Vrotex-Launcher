// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Authors: Andrew Okin
 *               Peterix
 *               Orochimarufan <orochimarufan.x3@gmail.com>
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QEasingCurve>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <algorithm>
#include <memory>

#include <BaseInstance.h>
#include <BuildConfig.h>
#include <DesktopServices.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <icons/IconList.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/AccountList.h>
#include <net/ApiDownload.h>
#include <net/NetJob.h>
#include <news/NewsChecker.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include "InstanceWindow.h"

#include "ui/GuiUtil.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/ExportPackDialog.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/ImportResourceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/NewsDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"
#include "ui/widgets/LabeledToolButton.h"

#include "minecraft/PackProfile.h"
#include "minecraft/VersionFile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "minecraft/mod/tasks/LocalResourceParse.h"

#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"

#include "KonamiCode.h"

#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"

#include "Json.h"

#include "MMCTime.h"

namespace {
QString profileInUseFilter(const QString& profile, bool used)
{
    if (used) {
        return QObject::tr("%1 (in use)").arg(profile);
    } else {
        return profile;
    }
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    buildLauncherShell();
    applyVortexStyle();

    setMinimumSize(980, 640);
    resize(1240, 820);
    setWindowIcon(APPLICATION->logo());
    setWindowTitle(APPLICATION->applicationDisplayName());
#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
#endif

    // instance toolbar stuff
    {
        // Qt doesn't like vertical moving toolbars, so we have to force them...
        // See https://github.com/PolyMC/PolyMC/issues/493
        connect(ui->instanceToolBar, &QToolBar::orientationChanged,
                [this](Qt::Orientation) { ui->instanceToolBar->setOrientation(Qt::Vertical); });

        // if you try to add a widget to a toolbar in a .ui file
        // qt designer will delete it when you save the file >:(
        changeIconButton = new LabeledToolButton(this);
        changeIconButton->setObjectName(QStringLiteral("changeIconButton"));
        changeIconButton->setIcon(QIcon::fromTheme("news"));
        changeIconButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(changeIconButton, &QToolButton::clicked, this, &MainWindow::on_actionChangeInstIcon_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, changeIconButton);

        renameButton = new LabeledToolButton(this);
        renameButton->setObjectName(QStringLiteral("renameButton"));
        renameButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(renameButton, &QToolButton::clicked, this, &MainWindow::on_actionRenameInstance_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, renameButton);

        ui->instanceToolBar->insertSeparator(ui->actionLaunchInstance);

        // restore the instance toolbar settings
        auto const setting_name = QString("WideBarVisibility_%1").arg(ui->instanceToolBar->objectName());
        instanceToolbarSetting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

        ui->instanceToolBar->setVisibilityState(QByteArray::fromBase64(instanceToolbarSetting->get().toString().toUtf8()));

        ui->instanceToolBar->addContextMenuAction(ui->newsToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->instanceToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->actionToggleStatusBar);
        ui->instanceToolBar->addContextMenuAction(ui->actionLockToolbars);
    }

    // set the menu for the folders help, accounts, and export tool buttons
    {
        auto foldersMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionFoldersButton));
        ui->actionFoldersButton->setMenu(ui->foldersMenu);
        foldersMenuButton->setPopupMode(QToolButton::InstantPopup);

        helpMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionHelpButton));
        ui->actionHelpButton->setMenu(new QMenu(this));
        ui->actionHelpButton->menu()->addActions(ui->helpMenu->actions());
        ui->actionHelpButton->menu()->removeAction(ui->actionCheckUpdate);
        helpMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto accountMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionAccountsButton));
        accountMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto exportInstanceMenu = new QMenu(this);
        exportInstanceMenu->addAction(ui->actionExportInstanceZip);
        exportInstanceMenu->addAction(ui->actionExportInstanceMrPack);
        exportInstanceMenu->addAction(ui->actionExportInstanceFlamePack);
        ui->actionExportInstance->setMenu(exportInstanceMenu);
    }

    // hide, disable and show stuff
    {
        ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());
        ui->actionMATRIX->setVisible(!BuildConfig.MATRIX_URL.isEmpty());
        ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());
        ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

        ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

#ifndef Q_OS_MAC
        ui->actionAddToPATH->setVisible(false);
#endif

        // disabled until we have an instance selected
        ui->instanceToolBar->setEnabled(false);
        setInstanceActionsEnabled(false);

        // add a close button at the end of the main toolbar when running on gamescope / steam deck
        // this is only needed on gamescope because it defaults to an X11/XWayland session and
        // does not implement decorations
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
            ui->mainToolBar->addAction(ui->actionCloseWindow);
        }

        ui->actionViewJavaFolder->setEnabled(BuildConfig.JAVA_DOWNLOADER_ENABLED);
    }

    {  // logs viewing
        connect(ui->actionViewLog, &QAction::triggered, this, [] { APPLICATION->showLogWindow(); });
    }

    // The old toolbars remain available to the backend actions, but are not part of the redesigned shell.
    ui->instanceToolBar->toggleViewAction()->setVisible(false);
    ui->newsToolBar->toggleViewAction()->setVisible(false);
    ui->mainToolBar->toggleViewAction()->setVisible(false);
    ui->instanceToolBar->hide();
    ui->newsToolBar->hide();

    updateThemeMenu();
    updateMainToolBar();
    // OSX magic.
    setUnifiedTitleAndToolBarOnMac(true);

    // Global shortcuts
    {
        // you can't set QKeySequence::StandardKey shortcuts in qt designer >:(
        ui->actionAddInstance->setShortcut(QKeySequence::New);
        ui->actionSettings->setShortcut(QKeySequence::Preferences);
        ui->actionUndoTrashInstance->setShortcut(QKeySequence::Undo);
        ui->actionDeleteInstance->setShortcuts({ QKeySequence(tr("Backspace")), QKeySequence::Delete });
        ui->actionCloseWindow->setShortcut(QKeySequence::Close);
        connect(ui->actionCloseWindow, &QAction::triggered, APPLICATION, &Application::closeCurrentWindow);

        // FIXME: This is kinda weird. and bad. We need some kind of managed shutdown.
        auto q = new QShortcut(QKeySequence::Quit, this);
        connect(q, &QShortcut::activated, APPLICATION, &Application::quit);
    }

    // Konami Code
    {
        secretEventFilter = new KonamiCode(this);
        connect(secretEventFilter, &KonamiCode::triggered, this, &MainWindow::konamiTriggered);
    }

    // News is opt-in for this fork; an empty feed avoids background polling and stale upstream branding.
    if (!BuildConfig.NEWS_RSS_URL.isEmpty()) {
        m_newsChecker.reset(new NewsChecker(APPLICATION->network(), BuildConfig.NEWS_RSS_URL));
        newsLabel = new QToolButton(this);
        newsLabel->setIcon(QIcon::fromTheme("news"));
        newsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        newsLabel->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        newsLabel->setFocusPolicy(Qt::NoFocus);
        ui->newsToolBar->insertWidget(ui->actionMoreNews, newsLabel);
        connect(newsLabel, &QAbstractButton::clicked, this, &MainWindow::newsButtonClicked);
        connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this, &MainWindow::updateNewsLabel);
        updateNewsLabel();
    } else {
        ui->newsToolBar->hide();
        ui->actionMoreNews->setVisible(false);
        ui->newsToolBar->toggleViewAction()->setVisible(false);
    }

    // Create the instance list widget
    {
        view = new InstanceView(m_instancesContent);
        view->setObjectName(QStringLiteral("vortexInstanceView"));
        view->setAccessibleName(tr("Minecraft instances"));
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        // FIXME: leaks ListViewDelegate
        auto delegate = new ListViewDelegate(this);
        view->setItemDelegate(delegate);
        view->setFrameShape(QFrame::NoFrame);
        // do not show ugly blue border on the mac
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        connect(delegate, &ListViewDelegate::textChanged, this, [this](QString before, QString after) {
            if (auto newRoot = askToUpdateInstanceDirName(m_selectedInstance, before, after, this); !newRoot.isEmpty()) {
                auto oldID = m_selectedInstance->id();
                auto newID = QFileInfo(newRoot).fileName();
                QString origGroup(APPLICATION->instances()->getInstanceGroup(oldID));
                bool syncGroup = origGroup != GroupId() && oldID != newID;
                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(oldID, GroupId());

                refreshInstances();
                setSelectedInstanceById(newID);

                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(newID, origGroup);
            }
        });

        view->installEventFilter(this);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, &MainWindow::showInstanceContextMenu);
        connect(view, &InstanceView::droppedURLs, this, &MainWindow::processURLs, Qt::QueuedConnection);

        proxymodel = new InstanceProxyModel(this);
        proxymodel->setSourceModel(APPLICATION->instances());
        proxymodel->sort(0);
        connect(proxymodel, &InstanceProxyModel::dataChanged, this, &MainWindow::instanceDataChanged);
        connect(APPLICATION->instances(), &InstanceList::instancesChanged, this, &MainWindow::updateDashboard);
        connect(m_instanceSearch, &QLineEdit::textChanged, proxymodel, &QSortFilterProxyModel::setFilterFixedString);

        view->setModel(proxymodel);
        view->setSourceOfGroupCollapseStatus(
            [](const QString& groupName) -> bool { return APPLICATION->instances()->isGroupCollapsed(groupName); });
        connect(view, &InstanceView::groupStateChanged, APPLICATION->instances(), &InstanceList::on_GroupStateChanged);
        m_instanceListLayout->addWidget(view, 1);
    }
    // The cat background
    {
        // set the cat action priority here so you can still see the action in qt designer
        ui->actionCAT->setPriority(QAction::LowPriority);
        bool cat_enable = APPLICATION->settings()->get("TheCat").toBool();
        ui->actionCAT->setChecked(cat_enable);
        connect(ui->actionCAT, &QAction::toggled, this, &MainWindow::onCatToggled);
        connect(APPLICATION, &Application::currentCatChanged, this, &MainWindow::onCatChanged);
        setCatBackground(cat_enable);
    }

    // Togglable status bar
    {
        bool statusBarVisible = APPLICATION->settings()->get("StatusBarVisible").toBool();
        ui->actionToggleStatusBar->setChecked(statusBarVisible);
        connect(ui->actionToggleStatusBar, &QAction::toggled, this, &MainWindow::setStatusBarVisibility);
        setStatusBarVisibility(statusBarVisible);
    }

    // Lock toolbars
    {
        bool toolbarsLocked = APPLICATION->settings()->get("ToolbarsLocked").toBool();
        ui->actionLockToolbars->setChecked(toolbarsLocked);
        connect(ui->actionLockToolbars, &QAction::toggled, this, &MainWindow::lockToolbars);
        lockToolbars(toolbarsLocked);
    }
    // start instance when double-clicked
    connect(view, &InstanceView::activated, this, &MainWindow::instanceActivated);

    // track the selection -- update the instance toolbar
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::instanceChanged);

    // track icon changes and update the toolbar!
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, &MainWindow::iconUpdated);

    // model reset -> selection is invalid. All the instance pointers are wrong.
    connect(APPLICATION->instances(), &InstanceList::dataIsInvalid, this, &MainWindow::selectionBad);

    // handle newly added instances
    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, &MainWindow::instanceSelectRequest);

    // When the global settings page closes, we want to know about it and update our state
    connect(APPLICATION, &Application::globalSettingsApplied, this, &MainWindow::globalSettingsClosed);

    m_statusLeft = new QLabel(tr("No instance selected"), this);
    m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
    statusBar()->addPermanentWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusCenter, 0);

    // Add "manage accounts" button, right align
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->insertWidget(ui->actionAccountsButton, spacer);

    // Use undocumented property... https://stackoverflow.com/questions/7121718/create-a-scrollbar-in-a-submenu-qt
    ui->accountsMenu->setStyleSheet("QMenu { menu-scrollable: 1; }");

    repopulateAccountsMenu();

    // Update the menu when the active account changes.
    // Shouldn't have to use lambdas here like this, but if I don't, the compiler throws a fit.
    // Template hell sucks...
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listChanged, [this] { defaultAccountChanged(); });

    // Show initial account
    defaultAccountChanged();

    // TODO: refresh accounts here?
    // auto accounts = APPLICATION->accounts();

    // A network request is only started when a feed URL is explicitly configured.
    if (m_newsChecker) {
        m_newsChecker->reloadNews();
        updateNewsLabel();
    }

    if (APPLICATION->updaterEnabled()) {
        bool updatesAllowed = APPLICATION->updatesAreAllowed();
        updatesAllowedChanged(updatesAllowed);

        connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::checkForUpdates);

        // set up the updater object.
        auto updater = APPLICATION->updater();

        if (updater) {
            connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this, &MainWindow::updatesAllowedChanged);
        }
    }

    connect(ui->actionUndoTrashInstance, &QAction::triggered, this, &MainWindow::undoTrashInstance);

    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());
    if (!view->selectionModel()->currentIndex().isValid() && APPLICATION->instances()->count() > 0) {
        setSelectedInstanceById(APPLICATION->instances()->at(0)->id());
    }
    updateDashboard();

    // Keep keyboard navigation focused on the library rather than a decorative dashboard card.
    view->setFocus();

    retranslateUi();
}

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt && !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool())
        ui->menuBar->setVisible(!ui->menuBar->isVisible());
    else
        QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::retranslateUi()
{
    if (m_selectedInstance) {
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
    } else {
        m_statusLeft->setText(tr("No instance selected"));
    }

    ui->retranslateUi(this);

    MinecraftAccountPtr defaultAccount = APPLICATION->accounts()->defaultAccount();
    if (defaultAccount) {
        auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
    }

    changeIconButton->setToolTip(ui->actionChangeInstIcon->toolTip());
    renameButton->setToolTip(ui->actionRenameInstance->toolTip());

    // replace the %1 with the launcher display name in some actions
    if (helpMenuButton->toolTip().contains("%1"))
        helpMenuButton->setToolTip(helpMenuButton->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));

    for (auto action : ui->helpMenu->actions()) {
        if (action->text().contains("%1"))
            action->setText(action->text().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        if (action->toolTip().contains("%1"))
            action->setToolTip(action->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}

void MainWindow::buildLauncherShell()
{
    auto* rootLayout = ui->horizontalLayout;
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_sidebar = new QWidget(ui->centralWidget);
    m_sidebar->setObjectName(QStringLiteral("vortexSidebar"));
    m_sidebar->setFixedWidth(232);
    auto* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(16, 18, 16, 16);
    sidebarLayout->setSpacing(6);

    auto* brand = new QWidget(m_sidebar);
    auto* brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(4, 2, 2, 16);
    brandLayout->setSpacing(11);
    auto* brandIcon = new QLabel(brand);
    brandIcon->setPixmap(APPLICATION->logo().pixmap(42, 42));
    brandIcon->setFixedSize(42, 42);
    brandIcon->setScaledContents(true);
    brandIcon->setAccessibleName(tr("Vortex Launcher logo"));
    brandLayout->addWidget(brandIcon, 0, Qt::AlignVCenter);
    auto* brandText = new QWidget(brand);
    auto* brandTextLayout = new QVBoxLayout(brandText);
    brandTextLayout->setContentsMargins(0, 0, 0, 0);
    brandTextLayout->setSpacing(0);
    auto* brandName = new QLabel(tr("VORTEX"), brandText);
    brandName->setObjectName(QStringLiteral("vortexBrandName"));
    auto* brandSub = new QLabel(tr("MINECRAFT LAUNCHER"), brandText);
    brandSub->setObjectName(QStringLiteral("vortexBrandSub"));
    brandTextLayout->addWidget(brandName);
    brandTextLayout->addWidget(brandSub);
    brandLayout->addWidget(brandText, 1, Qt::AlignVCenter);
    sidebarLayout->addWidget(brand);

    auto* libraryLabel = new QLabel(tr("LIBRARY"), m_sidebar);
    libraryLabel->setProperty("eyebrow", true);
    libraryLabel->setObjectName(QStringLiteral("vortexNavHeading"));
    libraryLabel->setContentsMargins(8, 8, 0, 4);
    sidebarLayout->addWidget(libraryLabel);

    auto makeNavigationButton = [this, sidebarLayout](const QString& title, const QString& iconName, bool checkable) {
        auto* button = new QToolButton(m_sidebar);
        button->setObjectName(QStringLiteral("vortexNavigation"));
        button->setProperty("navItem", true);
        button->setText(title);
        button->setIcon(QIcon::fromTheme(iconName));
        button->setIconSize(QSize(19, 19));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMinimumHeight(43);
        button->setCheckable(checkable);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setAccessibleName(title);
        sidebarLayout->addWidget(button);
        return button;
    };

    m_navHome = makeNavigationButton(tr("Home"), "minecraft", true);
    m_navInstances = makeNavigationButton(tr("Instances"), "new", true);
    m_navigationButtons = { m_navHome, m_navInstances };
    m_navHome->setChecked(true);

    auto* manageLabel = new QLabel(tr("INSTANCE TOOLS"), m_sidebar);
    manageLabel->setProperty("eyebrow", true);
    manageLabel->setObjectName(QStringLiteral("vortexNavHeading"));
    manageLabel->setContentsMargins(8, 14, 0, 4);
    sidebarLayout->addWidget(manageLabel);
    m_navMods = makeNavigationButton(tr("Mods"), "loadermods", false);
    m_navResources = makeNavigationButton(tr("Resource packs"), "resourcepacks", false);
    m_navShaders = makeNavigationButton(tr("Shaders"), "shaderpacks", false);
    m_navServers = makeNavigationButton(tr("Servers"), "server", false);
    for (auto* button : { m_navMods, m_navResources, m_navShaders, m_navServers }) {
        button->setToolTip(tr("Open this manager for the selected Minecraft instance"));
    }

    auto* divider = new QFrame(m_sidebar);
    divider->setObjectName(QStringLiteral("vortexDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    sidebarLayout->addWidget(divider);
    sidebarLayout->addStretch(1);

    auto* createInstance = new QPushButton(QIcon::fromTheme("new"), tr("New instance"), m_sidebar);
    createInstance->setObjectName(QStringLiteral("vortexCreateInstance"));
    createInstance->setProperty("primary", true);
    createInstance->setMinimumHeight(44);
    createInstance->setAccessibleName(tr("Create or import a Minecraft instance"));
    sidebarLayout->addWidget(createInstance);

    m_navProfile = new QToolButton(m_sidebar);
    m_navProfile->setObjectName(QStringLiteral("vortexProfileButton"));
    m_navProfile->setProperty("navItem", true);
    m_navProfile->setIcon(QIcon::fromTheme("accounts"));
    m_navProfile->setIconSize(QSize(19, 19));
    m_navProfile->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_navProfile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_navProfile->setMinimumHeight(43);
    m_navProfile->setText(tr("Accounts"));
    m_navProfile->setToolTip(tr("Manage Microsoft and offline profiles"));
    sidebarLayout->addWidget(m_navProfile);

    m_navSettings = makeNavigationButton(tr("Settings"), "settings", false);
    m_navSettings->setToolTip(tr("Launcher, Java, performance, language and theme settings"));

    const QString launcherVersion = QStringLiteral("%1.%2.%3").arg(BuildConfig.VERSION_MAJOR).arg(BuildConfig.VERSION_MINOR).arg(BuildConfig.VERSION_PATCH);
    auto* versionLabel = new QLabel(tr("Vortex Launcher %1").arg(launcherVersion), m_sidebar);
    versionLabel->setObjectName(QStringLiteral("vortexBuildLabel"));
    versionLabel->setContentsMargins(8, 8, 0, 0);
    sidebarLayout->addWidget(versionLabel);

    m_navIndicator = new QWidget(m_sidebar);
    m_navIndicator->setObjectName(QStringLiteral("vortexNavIndicator"));
    m_navIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_navIndicator->hide();

    auto* workArea = new QWidget(ui->centralWidget);
    workArea->setObjectName(QStringLiteral("vortexWorkArea"));
    auto* workLayout = new QVBoxLayout(workArea);
    workLayout->setContentsMargins(0, 0, 0, 0);
    workLayout->setSpacing(0);
    m_shellPages = new QStackedWidget(workArea);
    m_shellPages->setObjectName(QStringLiteral("vortexShellPages"));
    workLayout->addWidget(m_shellPages, 1);
    rootLayout->addWidget(m_sidebar);
    rootLayout->addWidget(workArea, 1);

    auto makeScrollPage = [this](QWidget** contentOut) {
        auto* scroll = new QScrollArea(m_shellPages);
        scroll->setObjectName(QStringLiteral("vortexScrollArea"));
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        auto* content = new QWidget(scroll);
        content->setObjectName(QStringLiteral("vortexPageContent"));
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(34, 27, 34, 32);
        layout->setSpacing(18);
        scroll->setWidget(content);
        *contentOut = content;
        return scroll;
    };

    QWidget* homeContent = nullptr;
    m_dashboardPage = makeScrollPage(&homeContent);
    m_shellPages->addWidget(m_dashboardPage);
    auto* homeLayout = qobject_cast<QVBoxLayout*>(homeContent->layout());

    auto* homeHeader = new QWidget(homeContent);
    auto* homeHeaderLayout = new QVBoxLayout(homeHeader);
    homeHeaderLayout->setContentsMargins(0, 0, 0, 0);
    homeHeaderLayout->setSpacing(3);
    auto* homeEyebrow = new QLabel(tr("VORTEX  /  YOUR GAME LIBRARY"), homeHeader);
    homeEyebrow->setProperty("eyebrow", true);
    homeEyebrow->setObjectName(QStringLiteral("vortexHomeEyebrow"));
    auto* homeTitle = new QLabel(tr("Your next world awaits"), homeHeader);
    homeTitle->setObjectName(QStringLiteral("vortexHomeTitle"));
    homeTitle->setProperty("pageTitle", true);
    auto* homeSubtitle = new QLabel(tr("One place for every version, modpack and play style."), homeHeader);
    homeSubtitle->setObjectName(QStringLiteral("vortexHomeSubtitle"));
    homeSubtitle->setProperty("muted", true);
    homeHeaderLayout->addWidget(homeEyebrow);
    homeHeaderLayout->addWidget(homeTitle);
    homeHeaderLayout->addWidget(homeSubtitle);
    homeLayout->addWidget(homeHeader);

    auto* hero = new QFrame(homeContent);
    hero->setObjectName(QStringLiteral("vortexHero"));
    hero->setMinimumHeight(218);
    auto* heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(27, 22, 28, 22);
    heroLayout->setSpacing(24);
    auto* heroCopy = new QVBoxLayout();
    heroCopy->setContentsMargins(0, 0, 0, 0);
    heroCopy->setSpacing(7);
    auto* featuredLabel = new QLabel(tr("READY WHEN YOU ARE"), hero);
    featuredLabel->setObjectName(QStringLiteral("vortexHeroEyebrow"));
    featuredLabel->setProperty("eyebrow", true);
    m_dashboardInstanceName = new QLabel(tr("Choose an instance to get started"), hero);
    m_dashboardInstanceName->setObjectName(QStringLiteral("vortexHeroTitle"));
    m_dashboardInstanceName->setProperty("pageTitle", true);
    m_dashboardInstanceName->setWordWrap(true);
    m_dashboardInstanceDetail = new QLabel(tr("Build a clean vanilla profile or explore a Modrinth pack."), hero);
    m_dashboardInstanceDetail->setObjectName(QStringLiteral("vortexHeroDetail"));
    m_dashboardInstanceDetail->setProperty("muted", true);
    m_dashboardInstanceDetail->setWordWrap(true);
    auto* heroActions = new QHBoxLayout();
    heroActions->setContentsMargins(0, 9, 0, 0);
    heroActions->setSpacing(9);
    m_dashboardPlayButton = new QPushButton(QIcon::fromTheme("launch"), tr("Create instance"), hero);
    m_dashboardPlayButton->setObjectName(QStringLiteral("vortexPlayButton"));
    m_dashboardPlayButton->setProperty("primary", true);
    m_dashboardPlayButton->setMinimumHeight(42);
    m_dashboardPlayButton->setAccessibleName(tr("Launch the selected Minecraft instance, or create one"));
    m_dashboardEditButton = new QPushButton(QIcon::fromTheme("instance-settings"), tr("Manage instance"), hero);
    m_dashboardEditButton->setObjectName(QStringLiteral("vortexEditButton"));
    m_dashboardEditButton->setProperty("secondary", true);
    m_dashboardEditButton->setMinimumHeight(42);
    m_dashboardEditButton->setEnabled(false);
    heroActions->addWidget(m_dashboardPlayButton);
    heroActions->addWidget(m_dashboardEditButton);
    heroCopy->addWidget(featuredLabel);
    heroCopy->addWidget(m_dashboardInstanceName);
    heroCopy->addWidget(m_dashboardInstanceDetail);
    heroCopy->addLayout(heroActions);
    heroLayout->addLayout(heroCopy, 1);
    m_dashboardInstanceIcon = new QLabel(hero);
    m_dashboardInstanceIcon->setObjectName(QStringLiteral("vortexHeroIcon"));
    m_dashboardInstanceIcon->setFixedSize(132, 132);
    m_dashboardInstanceIcon->setScaledContents(true);
    m_dashboardInstanceIcon->setAlignment(Qt::AlignCenter);
    heroLayout->addWidget(m_dashboardInstanceIcon, 0, Qt::AlignVCenter | Qt::AlignRight);
    homeLayout->addWidget(hero);

    auto* metrics = new QWidget(homeContent);
    auto* metricsLayout = new QHBoxLayout(metrics);
    metricsLayout->setContentsMargins(0, 0, 0, 0);
    metricsLayout->setSpacing(12);
    auto makeMetric = [metrics](const QString& caption, QLabel** valueOut) {
        auto* card = new QFrame(metrics);
        card->setObjectName(QStringLiteral("vortexMetricCard"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(17, 14, 17, 14);
        cardLayout->setSpacing(4);
        auto* value = new QLabel(QStringLiteral("—"), card);
        value->setObjectName(QStringLiteral("vortexMetricValue"));
        value->setProperty("metricValue", true);
        auto* title = new QLabel(caption, card);
        title->setObjectName(QStringLiteral("vortexMetricCaption"));
        title->setProperty("eyebrow", true);
        cardLayout->addWidget(value);
        cardLayout->addWidget(title);
        *valueOut = value;
        return card;
    };
    metricsLayout->addWidget(makeMetric(tr("INSTANCES"), &m_dashboardInstanceCount), 1);
    metricsLayout->addWidget(makeMetric(tr("ACTIVE PROFILE"), &m_dashboardAccountValue), 1);
    metricsLayout->addWidget(makeMetric(tr("TOTAL PLAY TIME"), &m_dashboardPlayTime), 1);
    homeLayout->addWidget(metrics);

    auto* quickHeading = new QLabel(tr("Quick access"), homeContent);
    quickHeading->setObjectName(QStringLiteral("vortexSectionTitle"));
    quickHeading->setProperty("sectionTitle", true);
    homeLayout->addWidget(quickHeading);
    auto* quickActions = new QHBoxLayout();
    quickActions->setSpacing(12);
    auto makeQuickAction = [homeContent](const QString& title, const QString& detail, const QString& iconName) {
        auto* button = new QPushButton(QIcon::fromTheme(iconName), title + QStringLiteral("\n") + detail, homeContent);
        button->setObjectName(QStringLiteral("vortexQuickAction"));
        button->setProperty("secondary", true);
        button->setMinimumHeight(76);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setIconSize(QSize(22, 22));
        button->setAccessibleName(title + QStringLiteral(". ") + detail);
        return button;
    };
    auto* discoverButton = makeQuickAction(tr("Browse Modrinth packs"), tr("Create a pack-backed instance"), "modrinth");
    auto* javaButton = makeQuickAction(tr("Java & performance"), tr("Runtime, memory and tuning"), "java");
    auto* accountsButton = makeQuickAction(tr("Accounts"), tr("Microsoft or offline profiles"), "accounts");
    quickActions->addWidget(discoverButton, 1);
    quickActions->addWidget(javaButton, 1);
    quickActions->addWidget(accountsButton, 1);
    homeLayout->addLayout(quickActions);

    auto* recentHeading = new QWidget(homeContent);
    auto* recentHeadingLayout = new QHBoxLayout(recentHeading);
    recentHeadingLayout->setContentsMargins(0, 6, 0, 0);
    auto* recentTitle = new QLabel(tr("Recently played"), recentHeading);
    recentTitle->setObjectName(QStringLiteral("vortexSectionTitle"));
    recentTitle->setProperty("sectionTitle", true);
    recentHeadingLayout->addWidget(recentTitle);
    recentHeadingLayout->addStretch(1);
    auto* viewAllButton = new QPushButton(tr("View all instances"), recentHeading);
    viewAllButton->setObjectName(QStringLiteral("vortexTextButton"));
    viewAllButton->setProperty("textButton", true);
    recentHeadingLayout->addWidget(viewAllButton);
    homeLayout->addWidget(recentHeading);

    m_dashboardRecentHost = new QWidget(homeContent);
    m_dashboardRecentHost->setObjectName(QStringLiteral("vortexRecentHost"));
    m_dashboardRecentLayout = new QVBoxLayout(m_dashboardRecentHost);
    m_dashboardRecentLayout->setContentsMargins(0, 0, 0, 0);
    m_dashboardRecentLayout->setSpacing(7);
    m_dashboardRecentEmpty = new QLabel(tr("Your instances will appear here. Create one or import a Modrinth modpack to begin."),
                                        m_dashboardRecentHost);
    m_dashboardRecentEmpty->setObjectName(QStringLiteral("vortexRecentEmpty"));
    m_dashboardRecentEmpty->setProperty("muted", true);
    m_dashboardRecentEmpty->setWordWrap(true);
    m_dashboardRecentLayout->addWidget(m_dashboardRecentEmpty);
    homeLayout->addWidget(m_dashboardRecentHost);
    homeLayout->addStretch(1);

    QWidget* instanceContent = nullptr;
    m_instancesPage = new QWidget(m_shellPages);
    m_instancesPage->setObjectName(QStringLiteral("vortexInstancesPage"));
    m_instancesContent = m_instancesPage;
    auto* instancesLayout = new QVBoxLayout(m_instancesPage);
    instancesLayout->setContentsMargins(30, 27, 30, 24);
    instancesLayout->setSpacing(16);
    m_shellPages->addWidget(m_instancesPage);
    Q_UNUSED(instanceContent);

    auto* instanceHeader = new QWidget(m_instancesPage);
    auto* instanceHeaderLayout = new QHBoxLayout(instanceHeader);
    instanceHeaderLayout->setContentsMargins(0, 0, 0, 0);
    auto* instanceHeaderCopy = new QVBoxLayout();
    instanceHeaderCopy->setContentsMargins(0, 0, 0, 0);
    instanceHeaderCopy->setSpacing(3);
    auto* instanceTitle = new QLabel(tr("Your instances"), instanceHeader);
    instanceTitle->setObjectName(QStringLiteral("vortexInstancesTitle"));
    instanceTitle->setProperty("pageTitle", true);
    auto* instanceSubtitle = new QLabel(tr("Separate worlds, loaders and settings — ready to launch."), instanceHeader);
    instanceSubtitle->setObjectName(QStringLiteral("vortexInstancesSubtitle"));
    instanceSubtitle->setProperty("muted", true);
    instanceHeaderCopy->addWidget(instanceTitle);
    instanceHeaderCopy->addWidget(instanceSubtitle);
    instanceHeaderLayout->addLayout(instanceHeaderCopy, 1);
    auto* addInstanceButton = new QPushButton(QIcon::fromTheme("new"), tr("New instance"), instanceHeader);
    addInstanceButton->setObjectName(QStringLiteral("vortexAddInstanceButton"));
    addInstanceButton->setProperty("primary", true);
    addInstanceButton->setMinimumHeight(42);
    instanceHeaderLayout->addWidget(addInstanceButton, 0, Qt::AlignVCenter);
    instancesLayout->addWidget(instanceHeader);

    auto* instanceControls = new QWidget(m_instancesPage);
    auto* instanceControlsLayout = new QHBoxLayout(instanceControls);
    instanceControlsLayout->setContentsMargins(0, 0, 0, 0);
    instanceControlsLayout->setSpacing(8);
    m_instanceSearch = new QLineEdit(instanceControls);
    m_instanceSearch->setObjectName(QStringLiteral("vortexInstanceSearch"));
    m_instanceSearch->setClearButtonEnabled(true);
    m_instanceSearch->setPlaceholderText(tr("Search instances by name…"));
    m_instanceSearch->setAccessibleName(tr("Search Minecraft instances"));
    m_instanceSearch->setMinimumHeight(40);
    instanceControlsLayout->addWidget(m_instanceSearch, 1);
    auto makeActionButton = [instanceControls](QAction* action) {
        auto* button = new QToolButton(instanceControls);
        button->setDefaultAction(action);
        button->setObjectName(QStringLiteral("vortexActionButton"));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setMinimumHeight(40);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        return button;
    };
    instanceControlsLayout->addWidget(makeActionButton(ui->actionLaunchInstance));
    instanceControlsLayout->addWidget(makeActionButton(ui->actionEditInstance));
    instanceControlsLayout->addWidget(makeActionButton(ui->actionDeleteInstance));
    instancesLayout->addWidget(instanceControls);

    auto* listHint = new QLabel(tr("Double-click to launch. Right-click an instance for groups, export, copy and more."), m_instancesPage);
    listHint->setObjectName(QStringLiteral("vortexListHint"));
    listHint->setProperty("muted", true);
    instancesLayout->addWidget(listHint);
    m_instanceListLayout = instancesLayout;

    connect(m_navHome, &QToolButton::clicked, this, [this] { showShellPage(m_dashboardPage, m_navHome); });
    connect(m_navInstances, &QToolButton::clicked, this, [this] { showShellPage(m_instancesPage, m_navInstances); });
    connect(m_navMods, &QToolButton::clicked, this, [this] { openSelectedManager("mods", m_navMods); });
    connect(m_navResources, &QToolButton::clicked, this, [this] { openSelectedManager("resourcepacks", m_navResources); });
    connect(m_navShaders, &QToolButton::clicked, this, [this] { openSelectedManager("shaderpacks", m_navShaders); });
    connect(m_navServers, &QToolButton::clicked, this, [this] { openSelectedManager("servers", m_navServers); });
    connect(m_navProfile, &QToolButton::clicked, this, &MainWindow::on_actionManageAccounts_triggered);
    connect(m_navSettings, &QToolButton::clicked, this, &MainWindow::on_actionSettings_triggered);
    connect(createInstance, &QPushButton::clicked, this, [this] {
        showShellPage(m_instancesPage, m_navInstances);
        on_actionAddInstance_triggered();
    });
    connect(addInstanceButton, &QPushButton::clicked, createInstance, &QPushButton::click);
    connect(m_dashboardPlayButton, &QPushButton::clicked, this, [this] {
        if (m_selectedInstance)
            on_actionLaunchInstance_triggered();
        else
            on_actionAddInstance_triggered();
    });
    connect(m_dashboardEditButton, &QPushButton::clicked, this, &MainWindow::on_actionEditInstance_triggered);
    connect(discoverButton, &QPushButton::clicked, this, [this] {
        showShellPage(m_instancesPage, m_navInstances);
        on_actionAddInstance_triggered();
    });
    connect(javaButton, &QPushButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(this, "java"); });
    connect(accountsButton, &QPushButton::clicked, this, &MainWindow::on_actionManageAccounts_triggered);
    connect(viewAllButton, &QPushButton::clicked, this, [this] { showShellPage(m_instancesPage, m_navInstances); });

    sidebarLayout->activate();
    QTimer::singleShot(0, this, [this] { activateNavigationButton(m_navHome); });
}

void MainWindow::applyVortexStyle()
{
    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const QString background = dark ? QStringLiteral("#10141d") : QStringLiteral("#f3f5fa");
    const QString sidebar = dark ? QStringLiteral("#111722") : QStringLiteral("#ffffff");
    const QString surface = dark ? QStringLiteral("#171e2a") : QStringLiteral("#ffffff");
    const QString surfaceRaised = dark ? QStringLiteral("#1c2533") : QStringLiteral("#f8f9fd");
    const QString border = dark ? QStringLiteral("#293243") : QStringLiteral("#e2e6ef");
    const QString text = dark ? QStringLiteral("#eff3fb") : QStringLiteral("#1a2232");
    const QString muted = dark ? QStringLiteral("#9ba7ba") : QStringLiteral("#687386");
    const QString accent = dark ? QStringLiteral("#8e8aff") : QStringLiteral("#615ce8");
    const QString accentSoft = dark ? QStringLiteral("#292846") : QStringLiteral("#eeedff");
    const QString heroStart = dark ? QStringLiteral("#1b2937") : QStringLiteral("#eef3ff");
    const QString heroEnd = dark ? QStringLiteral("#22233a") : QStringLiteral("#e9faf6");
    const QString accentText = dark ? QStringLiteral("#10141d") : QStringLiteral("#ffffff");

    QString sheet = QStringLiteral(R"QSS(
        QWidget { color: %6; }
        QMainWindow, QDialog, QWizard { background: %1; color: %6; }
        QMainWindow#MainWindow, QWidget#centralWidget, QWidget#vortexWorkArea,
        QStackedWidget#vortexShellPages, QWidget#vortexInstancesPage { background: %1; color: %6; }
        QWidget#vortexSidebar { background: %2; border-right: 1px solid %5; }
        QWidget#vortexPageContent { background: transparent; }
        QScrollArea#vortexScrollArea, QScrollArea#vortexScrollArea QWidget#qt_scrollarea_viewport { background: %1; border: 0; }
        QLabel { color: %6; }
        QLabel#vortexBrandName { color: %6; font-size: 17px; font-weight: 800; letter-spacing: 2px; }
        QLabel#vortexBrandSub, QLabel#vortexBuildLabel, QLabel#vortexNavHeading,
        QLabel[eyebrow="true"] { color: %7; font-size: 9px; font-weight: 700; letter-spacing: 1.5px; }
        QLabel[pageTitle="true"] { color: %6; font-size: 28px; font-weight: 750; }
        QLabel#vortexHeroTitle { font-size: 27px; }
        QLabel[sectionTitle="true"] { color: %6; font-size: 17px; font-weight: 700; }
        QLabel[muted="true"] { color: %7; }
        QLabel[metricValue="true"] { color: %6; font-size: 20px; font-weight: 700; }
        QToolButton[navItem="true"] { text-align: left; color: %7; background: transparent; border: 0; border-radius: 9px; padding: 0 12px; font-size: 13px; font-weight: 600; }
        QToolButton[navItem="true"]:hover { color: %6; background: %4; }
        QToolButton[navItem="true"]:checked { color: %8; background: %9; }
        QWidget#vortexNavIndicator { background: %8; border-radius: 2px; }
        QFrame#vortexDivider { color: %5; background: %5; border: 0; max-height: 1px; margin: 8px 5px 3px 5px; }
        QFrame#vortexHero { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %10,stop:1 %11); border: 1px solid %5; border-radius: 16px; }
        QLabel#vortexHeroIcon { background: %4; border: 1px solid %5; border-radius: 18px; padding: 10px; }
        QFrame#vortexMetricCard, QWidget#vortexRecentHost { background: %3; border: 1px solid %5; border-radius: 12px; }
        QPushButton, QToolButton#vortexActionButton { color: %6; background: %3; border: 1px solid %5; border-radius: 9px; padding: 9px 13px; font-size: 12px; font-weight: 600; }
        QPushButton:hover, QToolButton#vortexActionButton:hover { background: %4; border-color: %8; }
        QPushButton:pressed, QToolButton#vortexActionButton:pressed { background: %9; }
        QPushButton:disabled, QToolButton#vortexActionButton:disabled { color: %7; background: %4; border-color: %5; }
        QPushButton[primary="true"] { color: %12; background: %8; border: 1px solid %8; font-weight: 700; padding: 10px 16px; }
        QPushButton[primary="true"]:hover { background: %13; border-color: %13; }
        QPushButton[primary="true"]:disabled { color: %7; background: %4; border-color: %5; }
        QPushButton[secondary="true"] { background: %3; }
        QPushButton[textButton="true"] { color: %8; background: transparent; border: 0; padding: 6px 4px; }
        QPushButton[textButton="true"]:hover { color: %6; background: %4; }
        QPushButton#vortexQuickAction { text-align: left; padding: 12px 14px; }
        QLineEdit { color: %6; background: %3; border: 1px solid %5; border-radius: 9px; padding: 9px 12px; selection-background-color: %8; }
        QLineEdit:focus { border: 1px solid %8; }
        QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit { color: %6; background: %3; border: 1px solid %5; border-radius: 7px; padding: 6px 8px; selection-background-color: %8; }
        QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus { border: 1px solid %8; }
        QComboBox QAbstractItemView { color: %6; background: %3; border: 1px solid %5; selection-background-color: %9; }
        QCheckBox, QRadioButton { color: %6; spacing: 7px; }
        QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid %5; border-radius: 4px; background: %3; }
        QCheckBox::indicator:checked { background: %8; border-color: %8; }
        QRadioButton::indicator { width: 15px; height: 15px; border: 1px solid %5; border-radius: 8px; background: %3; }
        QRadioButton::indicator:checked { background: %8; border-color: %8; }
        QTabWidget::pane { background: %3; border: 1px solid %5; border-radius: 9px; top: -1px; }
        QTabBar::tab { color: %7; background: %4; border: 1px solid %5; padding: 8px 13px; margin-right: 3px; border-top-left-radius: 7px; border-top-right-radius: 7px; }
        QTabBar::tab:selected { color: %8; background: %3; }
        QGroupBox { color: %6; background: transparent; border: 1px solid %5; border-radius: 9px; margin-top: 14px; padding: 10px 8px 8px 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; color: %7; }
        QHeaderView::section { color: %7; background: %4; border: 0; border-bottom: 1px solid %5; padding: 6px 8px; font-weight: 600; }
        QListView, QTreeView, QTableView, QAbstractItemView { color: %6; background: transparent; border: 0; outline: 0; selection-background-color: %9; selection-color: %6; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar::handle:vertical { background: %5; min-height: 28px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QMenuBar { color: %6; background: %2; border-bottom: 1px solid %5; padding: 3px 6px; }
        QMenuBar::item { background: transparent; padding: 6px 9px; border-radius: 5px; }
        QMenuBar::item:selected { background: %4; }
        QMenu { color: %6; background: %3; border: 1px solid %5; padding: 5px; }
        QMenu::item { padding: 6px 26px 6px 12px; border-radius: 5px; }
        QMenu::item:selected { background: %9; }
        QStatusBar { color: %7; background: %2; border-top: 1px solid %5; }
        QToolTip { color: %6; background: %3; border: 1px solid %5; padding: 5px; }
    )QSS");
    const QStringList colors = { background, sidebar, surface, surfaceRaised, border, text, muted, accent, accentSoft,
                                 heroStart, heroEnd, accentText, dark ? QStringLiteral("#a5a1ff") : QStringLiteral("#514bd4") };
    for (int token = colors.size(); token > 0; --token)
        sheet.replace(QStringLiteral("%") + QString::number(token), colors.at(token - 1));
    qApp->setStyleSheet(sheet);
}

void MainWindow::showShellPage(QWidget* page, QToolButton* activeNavigationButton)
{
    if (!m_shellPages || !page)
        return;
    m_shellPages->setCurrentWidget(page);
    activateNavigationButton(activeNavigationButton);
}

void MainWindow::activateNavigationButton(QToolButton* button)
{
    for (auto* nav : m_navigationButtons) {
        if (!nav)
            continue;
        nav->setChecked(nav == button);
    }
    if (!m_navIndicator || !m_sidebar || !button)
        return;

    const QPoint position = button->mapTo(m_sidebar, QPoint(0, 0));
    const QRect target(position.x(), position.y() + 9, 3, qMax(22, button->height() - 18));
    m_navIndicator->raise();
    if (!m_navIndicator->isVisible() || m_navIndicator->geometry().isEmpty()) {
        m_navIndicator->setGeometry(target);
        m_navIndicator->show();
        return;
    }
    if (m_navAnimation) {
        m_navAnimation->stop();
        m_navAnimation->deleteLater();
    }
    m_navAnimation = new QPropertyAnimation(m_navIndicator, "geometry", this);
    m_navAnimation->setDuration(125);
    m_navAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_navAnimation->setStartValue(m_navIndicator->geometry());
    m_navAnimation->setEndValue(target);
    auto* animation = m_navAnimation;
    connect(animation, &QPropertyAnimation::finished, this, [this, animation] {
        if (m_navAnimation == animation)
            m_navAnimation = nullptr;
        animation->deleteLater();
    });
    animation->start();
}

void MainWindow::openSelectedManager(const QString& pageId, QToolButton* activeNavigationButton)
{
    if (!m_selectedInstance) {
        showShellPage(m_instancesPage, m_navInstances);
        statusBar()->showMessage(tr("Select a Minecraft instance first."), 3500);
        return;
    }
    auto* minecraftInstance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
    if (!minecraftInstance || !m_selectedInstance->canEdit()) {
        statusBar()->showMessage(tr("This manager is only available for editable Minecraft instances."), 4500);
        return;
    }

    auto* window = APPLICATION->showInstanceWindow(m_selectedInstance, pageId);
    if (window) {
        activateNavigationButton(activeNavigationButton);
        statusBar()->showMessage(tr("Opened %1 for %2.").arg(pageId, m_selectedInstance->name()), 3000);
    }
}

void MainWindow::updateDashboard()
{
    if (!m_dashboardPage || !APPLICATION || !APPLICATION->instances())
        return;

    const int count = APPLICATION->instances()->count();
    if (m_dashboardInstanceCount)
        m_dashboardInstanceCount->setText(QString::number(count));

    BaseInstance* selected = m_selectedInstance;
    if (selected) {
        m_dashboardInstanceName->setText(selected->name());
        QString detail = selected->typeName();
        const QString status = selected->getStatusbarDescription();
        if (!status.isEmpty())
            detail += QStringLiteral("  ·  ") + status;
        m_dashboardInstanceDetail->setText(detail);
        const auto icon = APPLICATION->icons()->getIcon(selected->iconKey());
        m_dashboardInstanceIcon->setPixmap(icon.pixmap(132, 132));
        m_dashboardPlayButton->setText(selected->isRunning() ? tr("View running game") : tr("Play selected"));
        m_dashboardPlayButton->setIcon(QIcon::fromTheme("launch"));
        m_dashboardPlayButton->setEnabled(selected->isRunning() || selected->canLaunch());
        m_dashboardEditButton->setEnabled(selected->canEdit());
    } else {
        m_dashboardInstanceName->setText(count ? tr("Select an instance to get started") : tr("Your first world starts here"));
        m_dashboardInstanceDetail->setText(count ? tr("Choose an instance in your library, then launch or customize it.")
                                                 : tr("Create a clean vanilla profile or explore a Modrinth pack."));
        m_dashboardInstanceIcon->setPixmap(APPLICATION->logo().pixmap(132, 132));
        m_dashboardPlayButton->setText(count ? tr("Choose an instance") : tr("Create instance"));
        m_dashboardPlayButton->setIcon(QIcon::fromTheme(count ? "minecraft" : "new"));
        m_dashboardPlayButton->setEnabled(true);
        m_dashboardEditButton->setEnabled(false);
    }

    const auto defaultAccount = APPLICATION->accounts()->defaultAccount();
    QString accountText = tr("No profile");
    if (defaultAccount) {
        accountText = defaultAccount->displayName();
        accountText += defaultAccount->accountType() == AccountType::Offline ? tr(" · Offline") : tr(" · Microsoft");
    }
    if (m_dashboardAccountValue)
        m_dashboardAccountValue->setText(accountText);
    if (m_navProfile) {
        m_navProfile->setText(defaultAccount ? defaultAccount->displayName() : tr("Accounts"));
        const QPixmap accountFace = defaultAccount ? defaultAccount->getFace() : QPixmap();
        m_navProfile->setIcon(accountFace.isNull() ? QIcon::fromTheme("accounts") : QIcon(accountFace));
        m_navProfile->setToolTip(defaultAccount ? tr("Active profile: %1. Click to manage accounts.").arg(defaultAccount->displayName())
                                                : tr("No active profile. Click to manage accounts."));
    }

    const int totalPlayTime = APPLICATION->instances()->getTotalPlayTime();
    if (m_dashboardPlayTime) {
        m_dashboardPlayTime->setText(totalPlayTime > 0
                                         ? Time::prettifyDuration(totalPlayTime, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())
                                         : tr("Not yet played"));
    }

    const bool canManage = selected && selected->canEdit() && dynamic_cast<MinecraftInstance*>(selected);
    if (m_navMods)
        m_navMods->setEnabled(canManage);
    if (m_navResources)
        m_navResources->setEnabled(canManage);
    if (m_navShaders)
        m_navShaders->setEnabled(canManage);
    if (m_navServers)
        m_navServers->setEnabled(canManage);

    if (!m_dashboardRecentLayout)
        return;
    while (auto* item = m_dashboardRecentLayout->takeAt(0)) {
        if (auto* widget = item->widget()) {
            if (widget == m_dashboardRecentEmpty)
                widget->hide();
            else
                widget->deleteLater();
        }
        delete item;
    }

    QList<BaseInstance*> recent;
    recent.reserve(count);
    for (int i = 0; i < count; ++i)
        recent.append(APPLICATION->instances()->at(i));
    std::stable_sort(recent.begin(), recent.end(), [](const BaseInstance* a, const BaseInstance* b) {
        return a->lastLaunch() > b->lastLaunch();
    });

    if (recent.isEmpty()) {
        m_dashboardRecentEmpty->setText(tr("Your instances will appear here. Create one or import a Modrinth modpack to begin."));
        m_dashboardRecentLayout->addWidget(m_dashboardRecentEmpty);
        m_dashboardRecentEmpty->show();
    } else {
        const int displayCount = qMin(4, recent.size());
        for (int i = 0; i < displayCount; ++i) {
            BaseInstance* instance = recent.at(i);
            auto* row = new QPushButton(APPLICATION->icons()->getIcon(instance->iconKey()),
                                        QStringLiteral("%1    ·    %2").arg(instance->name(), instance->typeName()), m_dashboardRecentHost);
            row->setObjectName(QStringLiteral("vortexRecentButton"));
            row->setProperty("secondary", true);
            row->setMinimumHeight(42);
            row->setIconSize(QSize(25, 25));
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            row->setAccessibleName(tr("Select instance %1").arg(instance->name()));
            const QString instanceId = instance->id();
            connect(row, &QPushButton::clicked, this, [this, instanceId] {
                setSelectedInstanceById(instanceId);
                showShellPage(m_dashboardPage, m_navHome);
            });
            m_dashboardRecentLayout->addWidget(row);
        }
    }
}

MainWindow::~MainWindow() {}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->mainToolBar->toggleViewAction());

    filteredMenu->addAction(ui->actionToggleStatusBar);
    filteredMenu->addAction(ui->actionLockToolbars);

    return filteredMenu;
}
void MainWindow::setStatusBarVisibility(bool state)
{
    statusBar()->setVisible(state);
    APPLICATION->settings()->set("StatusBarVisible", state);
}
void MainWindow::lockToolbars(bool state)
{
    ui->mainToolBar->setMovable(!state);
    ui->instanceToolBar->setMovable(!state);
    ui->newsToolBar->setMovable(!state);
    APPLICATION->settings()->set("ToolbarsLocked", state);
}

void MainWindow::konamiTriggered()
{
    QString gradient =
        " stop:0 rgba(125, 0, 0, 255), stop:0.166 rgba(125, 125, 0, 255), stop:0.333 rgba(0, 125, 0, 255), stop:0.5 rgba(0, 125, 125, "
        "255), stop:0.666 rgba(0, 0, 125, 255), stop:0.833 rgba(125, 0, 125, 255), stop:1 rgba(125, 0, 0, 255));";
    QString stylesheet = "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0," + gradient;
    if (ui->mainToolBar->styleSheet() == stylesheet) {
        ui->mainToolBar->setStyleSheet("");
        ui->instanceToolBar->setStyleSheet("");
        ui->centralWidget->setStyleSheet("");
        ui->newsToolBar->setStyleSheet("");
        ui->statusBar->setStyleSheet("");
        qDebug() << "Super Secret Mode DEACTIVATED!";
    } else {
        ui->mainToolBar->setStyleSheet(stylesheet);
        ui->instanceToolBar->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1," + gradient);
        ui->centralWidget->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1," + gradient);
        ui->newsToolBar->setStyleSheet(stylesheet);
        ui->statusBar->setStyleSheet(stylesheet);
        qDebug() << "Super Secret Mode ACTIVATED!";
    }
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
    QList<QAction*> actions;

    QAction* actionSep = new QAction("", this);
    actionSep->setSeparator(true);

    bool onInstance = view->indexAt(pos).isValid();
    if (onInstance) {
        // reuse the file menu actions
        actions = ui->fileMenu->actions();

        // remove the add instance action, launcher settings action and close action
        actions.removeFirst();
        actions.removeLast();
        actions.removeLast();

        actions.prepend(ui->actionChangeInstIcon);
        actions.prepend(ui->actionRenameInstance);

        // add header
        actions.prepend(actionSep);
        QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
        actionVoid->setEnabled(false);
        actions.prepend(actionVoid);
    } else {
        auto group = view->groupNameAt(pos);

        QAction* actionVoid = new QAction(group.isNull() ? BuildConfig.LAUNCHER_DISPLAYNAME : group, this);
        actionVoid->setEnabled(false);

        QAction* actionCreateInstance = new QAction(tr("&Create instance"), this);
        actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
        if (!group.isNull()) {
            QVariantMap instance_action_data;
            instance_action_data["group"] = group;
            actionCreateInstance->setData(instance_action_data);
        }

        connect(actionCreateInstance, &QAction::triggered, this, &MainWindow::on_actionAddInstance_triggered);

        actions.prepend(actionSep);
        actions.prepend(actionVoid);
        actions.append(actionCreateInstance);
        if (!group.isNull()) {
            QAction* actionDeleteGroup = new QAction(tr("&Delete group"), this);
            connect(actionDeleteGroup, &QAction::triggered, this, [this, group] { deleteGroup(group); });
            actions.append(actionDeleteGroup);

            QAction* actionRenameGroup = new QAction(tr("&Rename group"), this);
            connect(actionRenameGroup, &QAction::triggered, this, [this, group] { renameGroup(group); });
            actions.append(actionRenameGroup);
        }
    }
    QMenu myMenu;
    myMenu.addActions(actions);
    /*
    if (onInstance)
        myMenu.setEnabled(m_selectedInstance->canLaunch());
    */
    myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateMainToolBar()
{
    // The shell replaces the old action toolbar; keep the classic menu as an accessible fallback.
    ui->menuBar->setVisible(true);
    ui->mainToolBar->hide();
    ui->instanceToolBar->hide();
    if (ui->newsToolBar->actions().size() <= 1 || BuildConfig.NEWS_RSS_URL.isEmpty())
        ui->newsToolBar->hide();
}

void MainWindow::updateLaunchButton()
{
    QMenu* launchMenu = ui->actionLaunchInstance->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    if (m_selectedInstance)
        m_selectedInstance->populateLaunchMenu(launchMenu);
    ui->actionLaunchInstance->setMenu(launchMenu);
}

void MainWindow::updateThemeMenu()
{
    QMenu* themeMenu = ui->actionChangeTheme->menu();

    if (themeMenu) {
        themeMenu->clear();
    } else {
        themeMenu = new QMenu(this);
    }

    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();

    QActionGroup* themesGroup = new QActionGroup(this);

    for (auto* theme : themes) {
        QAction* themeAction = themeMenu->addAction(theme->name());

        themeAction->setCheckable(true);
        if (APPLICATION->settings()->get("ApplicationTheme").toString() == theme->id()) {
            themeAction->setChecked(true);
        }
        themeAction->setActionGroup(themesGroup);

        connect(themeAction, &QAction::triggered, this, [this, theme]() {
            APPLICATION->themeManager()->setApplicationTheme(theme->id());
            APPLICATION->settings()->set("ApplicationTheme", theme->id());
            applyVortexStyle();
        });
    }

    ui->actionChangeTheme->setMenu(themeMenu);
}

void MainWindow::repopulateAccountsMenu()
{
    ui->accountsMenu->clear();

    // NOTE: this is done so the accounts button text is not set to the accounts menu title
    QMenu* accountsButtonMenu = ui->actionAccountsButton->menu();
    if (accountsButtonMenu) {
        accountsButtonMenu->clear();
    } else {
        accountsButtonMenu = new QMenu(this);
        ui->actionAccountsButton->setMenu(accountsButtonMenu);
    }

    auto accounts = APPLICATION->accounts();
    MinecraftAccountPtr defaultAccount = accounts->defaultAccount();

    QString active_profileId = "";
    if (defaultAccount) {
        // this can be called before accountMenuButton exists
        if (ui->actionAccountsButton) {
            auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
            ui->actionAccountsButton->setText(profileLabel);
        }
    }

    QActionGroup* accountsGroup = new QActionGroup(this);

    if (accounts->count() <= 0) {
        ui->actionNoAccountsAdded->setEnabled(false);
        ui->accountsMenu->addAction(ui->actionNoAccountsAdded);
    } else {
        // TODO: Nicer way to iterate?
        for (int i = 0; i < accounts->count(); i++) {
            MinecraftAccountPtr account = accounts->at(i);
            auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
            QAction* action = new QAction(profileLabel, this);
            action->setData(i);
            action->setCheckable(true);
            action->setActionGroup(accountsGroup);
            if (defaultAccount == account) {
                action->setChecked(true);
            }

            auto face = account->getFace();
            if (!face.isNull()) {
                action->setIcon(face);
            } else {
                action->setIcon(QIcon::fromTheme("noaccount"));
            }

            const int highestNumberKey = 9;
            if (i < highestNumberKey) {
                action->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i + 1)));
            }

            ui->accountsMenu->addAction(action);
            connect(action, &QAction::triggered, this, &MainWindow::changeActiveAccount);
        }
    }

    ui->accountsMenu->addSeparator();

    ui->actionNoDefaultAccount->setData(-1);
    ui->actionNoDefaultAccount->setChecked(!defaultAccount);
    ui->actionNoDefaultAccount->setActionGroup(accountsGroup);

    ui->accountsMenu->addAction(ui->actionNoDefaultAccount);

    connect(ui->actionNoDefaultAccount, &QAction::triggered, this, &MainWindow::changeActiveAccount);

    ui->accountsMenu->addSeparator();
    ui->accountsMenu->addAction(ui->actionManageAccounts);

    accountsButtonMenu->addActions(ui->accountsMenu->actions());
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
    if (!APPLICATION->updaterEnabled()) {
        return;
    }
    ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
    QAction* sAction = (QAction*)sender();

    // Profile's associated Mojang username
    if (sAction->data().typeId() != QMetaType::Int)
        return;

    QVariant action_data = sAction->data();
    bool valid = false;
    int index = action_data.toInt(&valid);
    if (!valid) {
        index = -1;
    }
    auto accounts = APPLICATION->accounts();
    accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
    defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
    repopulateAccountsMenu();

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

    // FIXME: this needs adjustment for MSA
    if (account && account->profileName() != "") {
        auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
        auto face = account->getFace();
        if (face.isNull()) {
            ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
        } else {
            ui->actionAccountsButton->setIcon(face);
        }
        updateDashboard();
        return;
    }

    // Set the icon to the "no account" icon.
    ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
    ui->actionAccountsButton->setText(tr("Accounts"));
    updateDashboard();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == view) {
        if (ev->type() == QEvent::KeyPress) {
            secretEventFilter->input(ev);
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                    /*
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activateInstance(m_selectedInstance);
                    return true;
                    */
                case Qt::Key_Delete:
                    on_actionDeleteInstance_triggered();
                    return true;
                case Qt::Key_F5:
                    refreshInstances();
                    return true;
                case Qt::Key_F2:
                    on_actionRenameInstance_triggered();
                    return true;
                default:
                    break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
    if (!m_newsChecker || !newsLabel)
        return;
    if (m_newsChecker->isLoadingNews()) {
        newsLabel->setText(tr("Loading news..."));
        newsLabel->setEnabled(false);
        ui->actionMoreNews->setVisible(false);
    } else {
        QList<NewsEntryPtr> entries = m_newsChecker->getNewsEntries();
        if (entries.length() > 0) {
            newsLabel->setText(entries[0]->title);
            newsLabel->setEnabled(true);
            ui->actionMoreNews->setVisible(true);
        } else {
            newsLabel->setText(tr("No news available."));
            newsLabel->setEnabled(false);
            ui->actionMoreNews->setVisible(false);
        }
    }
}

QList<int> stringToIntList(const QString& string)
{
    QStringList split = string.split(',', Qt::SkipEmptyParts);
    QList<int> out;
    for (int i = 0; i < split.size(); ++i) {
        out.append(split.at(i).toInt());
    }
    return out;
}
QString intListToString(const QList<int>& list)
{
    QStringList slist;
    for (int i = 0; i < list.size(); ++i) {
        slist.append(QString::number(list.at(i)));
    }
    return slist.join(',');
}

void MainWindow::onCatToggled(bool state)
{
    setCatBackground(state);
    APPLICATION->settings()->set("TheCat", state);
}

void MainWindow::setCatBackground(bool enabled)
{
    view->setPaintCat(enabled);
    view->viewport()->repaint();
}

void MainWindow::runModalTask(Task* task)
{
    connect(task, &Task::failed,
            [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task, &Task::succeeded, [this, task]() {
        QStringList warnings = task->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    connect(task, &Task::aborted, [this] {
        CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."), QMessageBox::Information)
            ->show();
    });
    ProgressDialog loadDialog(this);
    loadDialog.setSkipButton(true, tr("Abort"));
    loadDialog.execWithTask(task);
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));
    runModalTask(task.get());
}

void MainWindow::on_actionCopyInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
    if (!copyInstDlg.exec())
        return;

    auto copyTask = new InstanceCopyTask(m_selectedInstance, copyInstDlg.getChosenOptions());
    copyTask->setName(copyInstDlg.instName());
    copyTask->setGroup(copyInstDlg.instGroup());
    copyTask->setIcon(copyInstDlg.iconKey());
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(copyTask));
    runModalTask(task.get());
}

void MainWindow::addInstance(const QString& url, const QMap<QString, QString>& extra_info)
{
    QString groupName;
    do {
        QObject* obj = sender();
        if (!obj)
            break;
        QAction* action = qobject_cast<QAction*>(obj);
        if (!action)
            break;
        auto map = action->data().toMap();
        if (!map.contains("group"))
            break;
        groupName = map["group"].toString();
    } while (0);

    if (groupName.isEmpty()) {
        groupName = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    }

    NewInstanceDialog newInstDlg(groupName, url, extra_info, this);
    if (!newInstDlg.exec())
        return;

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", newInstDlg.instGroup());

    InstanceTask* creationTask = newInstDlg.extractTask();
    if (creationTask) {
        instanceFromInstanceTask(creationTask);
    }
}

void MainWindow::on_actionAddInstance_triggered()
{
    addInstance();
}

void MainWindow::processURLs(QList<QUrl> urls)
{
    // NOTE: This loop only processes one dropped file!
    for (auto& url : urls) {
        if (url.isEmpty() || url.toString().trimmed().isEmpty())
            continue;

        qDebug() << "Processing" << url;

        // The isLocalFile() check below doesn't work as intended without an explicit scheme.
        if (url.scheme().isEmpty())
            url.setScheme("file");

        ModPlatform::IndexedVersion version;
        QMap<QString, QString> extra_info;
        QUrl local_url;
        if (!url.isLocalFile()) {  // download the remote resource and identify

            const bool isExternalURLImport =
                (url.host().toLower() == "import") ||
                (url.path().startsWith("/import", Qt::CaseInsensitive));

            QUrl dl_url;
            if (url.scheme() == "curseforge" || (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && url.host() == "install")) {
                // need to find the download link for the modpack / resource
                // format of url curseforge://install?addonId=IDHERE&fileId=IDHERE
                // format of url binaryname://install?platform=curseforge&addonId=IDHERE&fileId=IDHERE
                QUrlQuery query(url);
                
                // check if this is a binaryname:// url
                if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) {
                    // check this is an curseforge platform request
                    if (query.queryItemValue("platform").toLower() != "curseforge") {
                        qDebug() << "Invalid mod distribution platform:" << query.queryItemValue("platform");
                        continue;
                    }
                }

                if (query.allQueryItemValues("addonId").isEmpty() || query.allQueryItemValues("fileId").isEmpty()) {
                    qDebug() << "Invalid curseforge link:" << url;
                    continue;
                }

                auto addonId = query.allQueryItemValues("addonId")[0];
                auto fileId = query.allQueryItemValues("fileId")[0];

                extra_info.insert("pack_id", addonId);
                extra_info.insert("pack_version_id", fileId);

                auto api = FlameAPI();
                auto [job, array] = api.getFile(addonId, fileId);

                connect(job.get(), &Task::failed, this,
                        [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
                connect(job.get(), &Task::succeeded, this, [this, array, addonId, fileId, &dl_url, &version] {
                    qDebug() << "Returned CFURL Json:\n" << array->toStdString().c_str();
                    auto doc = Json::requireDocument(*array);
                    auto data = doc.object()["data"].toObject();
                    // No way to find out if it's a mod or a modpack before here
                    // And also we need to check if it ends with .zip, instead of any better way
                    version = FlameMod::loadIndexedPackVersion(data);
                    auto fileName = version.fileName;

                    // Have to use ensureString then use QUrl to get proper url encoding
                    dl_url = QUrl(version.downloadUrl);
                    if (!dl_url.isValid()) {
                        CustomMessageBox::selectable(
                            this, tr("Error"),
                            tr("The modpack, mod, or resource %1 is blocked for third-parties! Please download it manually.").arg(fileName),
                            QMessageBox::Critical)
                            ->show();
                        return;
                    }

                    QFileInfo dl_file(dl_url.fileName());
                });

                {  // drop stack
                    ProgressDialog dlUrlDialod(this);
                    dlUrlDialod.setSkipButton(true, tr("Abort"));
                    dlUrlDialod.execWithTask(job.get());
                }

            } else if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && !isExternalURLImport) {
                QVariantMap receivedData;
                const QUrlQuery query(url.query());
                const auto items = query.queryItems();
                for (auto it = items.begin(), end = items.end(); it != end; ++it)
                    receivedData.insert(it->first, it->second);
                emit APPLICATION->oauthReplyRecieved(receivedData);
                continue;
            } else if ((url.scheme() == "prismlauncher" || url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME)
                        && isExternalURLImport) {
                // PrismLauncher URL protocol modpack import
                // works for any prism fork
                // preferred import format: prismlauncher://import?url=ENCODED
                const auto host = url.host().toLower();
                const auto path = url.path();

                QString encodedTarget;

                {
                    QUrlQuery query(url);
                    const auto values = query.allQueryItemValues("url");
                    if (!values.isEmpty()) {
                        encodedTarget = values.first();
                    }
                }

                // alternative import format: prismlauncher://import/ENCODED
                if (encodedTarget.isEmpty()) {

                    QString p = path;

                    if (p.startsWith("/import/", Qt::CaseInsensitive)) {
                        p = p.mid(QString("/import/").size());
                    } else if (host == "import" && p.startsWith("/")) {
                        p = p.mid(1);
                    }

                    if (!p.isEmpty() && p != "/import") {
                        encodedTarget = p;
                    }
                }

                if (encodedTarget.isEmpty()) {
                    CustomMessageBox::selectable(
                        this,
                        tr("Error"),
                        tr("Invalid import link: missing 'url' parameter."),
                        QMessageBox::Critical
                    )->show();
                    continue;
                }

                const QString decodedStr = QUrl::fromPercentEncoding(encodedTarget.toUtf8()).trimmed();

                QUrl target = QUrl::fromUserInput(decodedStr);

                // Validate: only allow http(s)
                if (!target.isValid() || (target.scheme() != "https" && target.scheme() != "http")) {
                    CustomMessageBox::selectable(
                        this,
                        tr("Error"),
                        tr("Invalid import link: URL must be http(s)."),
                        QMessageBox::Critical
                    )->show();
                    continue;
                }

                const auto res = QMessageBox::question(
                    this,
                    tr("Install modpack"),
                    tr("Do you want to download and import a modpack from:\n%1\n\nURL:\n%2")
                        .arg(target.host(), target.toString()),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes
                );
                if (res != QMessageBox::Yes) {
                    continue;
                }

                dl_url = target;
            } else {
                dl_url = url;
            }

            if (!dl_url.isValid()) {
                continue;  // no valid url to download this resource
            }

            const QString path = dl_url.host() + '/' + dl_url.path();
            auto entry = APPLICATION->metacache()->resolveEntry("general", path);
            entry->setStale(true);
            auto dl_job = unique_qobject_ptr<NetJob>(new NetJob(tr("Modpack download"), APPLICATION->network()));
            dl_job->addNetAction(Net::ApiDownload::makeCached(dl_url, entry));
            auto archivePath = entry->getFullPath();

            bool dl_success = false;
            connect(dl_job.get(), &Task::failed, this,
                    [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            connect(dl_job.get(), &Task::succeeded, this, [&dl_success] { dl_success = true; });

            {  // drop stack
                ProgressDialog dlUrlDialod(this);
                dlUrlDialod.setSkipButton(true, tr("Abort"));
                dlUrlDialod.execWithTask(dl_job.get());
            }

            if (!dl_success) {
                continue;  // no local file to identify
            }
            local_url = QUrl::fromLocalFile(archivePath);

        } else {
            local_url = url;
        }

        auto localFileName = QDir::toNativeSeparators(local_url.toLocalFile());
        QFileInfo localFileInfo(localFileName);

        if (localFileName.isEmpty() || !localFileInfo.exists()) {
            qDebug() << "Ignoring invalid path" << localFileName;
            continue;
        }

        auto type = ResourceUtils::identify(localFileInfo);

        if (ModPlatform::ResourceTypeUtils::VALID_RESOURCES.count(type) == 0) {  // probably instance/modpack
            addInstance(localFileName, extra_info);
            continue;
        }

        if (APPLICATION->instances()->count() <= 0) {
            CustomMessageBox::selectable(this, tr("No instance!"),
                                         tr("No instance available to add the resource to.\nPlease create a new instance before "
                                            "attempting to install this resource again."),
                                         QMessageBox::Critical)
                ->show();
            continue;
        }
        ImportResourceDialog dlg(localFileName, type, this);

        if (dlg.exec() != QDialog::Accepted)
            continue;

        qDebug() << "Adding resource" << localFileName << "to" << dlg.selectedInstanceKey;

        auto inst = APPLICATION->instances()->getInstanceById(dlg.selectedInstanceKey);
        auto minecraftInst = dynamic_cast<MinecraftInstance*>(inst);

        switch (type) {
            case ModPlatform::ResourceType::ResourcePack:
                minecraftInst->resourcePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::TexturePack:
                minecraftInst->texturePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::DataPack:
                qWarning() << "Importing of Data Packs not supported at this time. Ignoring" << localFileName;
                break;
            case ModPlatform::ResourceType::Mod:
                minecraftInst->loaderModList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::ShaderPack:
                minecraftInst->shaderPackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::World:
                minecraftInst->worldList()->installWorld(localFileInfo);
                break;
            case ModPlatform::ResourceType::Unknown:
            default:
                qDebug() << "Can't Identify" << localFileName << "Ignoring it.";
                break;
        }
    }
}

void MainWindow::on_actionREDDIT_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionMATRIX_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.MATRIX_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
    if (!m_selectedInstance)
        return;

    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_selectedInstance->iconKey());
    if (dlg.result() == QDialog::Accepted) {
        m_selectedInstance->setIconKey(dlg.selectedIconKey);
        auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
        ui->actionChangeInstIcon->setIcon(icon);
        changeIconButton->setIcon(icon);
    }
}

void MainWindow::iconUpdated(QString icon)
{
    if (icon == m_currentInstIcon) {
        auto new_icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
        ui->actionChangeInstIcon->setIcon(new_icon);
        changeIconButton->setIcon(new_icon);
    }
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
    m_currentInstIcon = new_icon;
    auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
    ui->actionChangeInstIcon->setIcon(icon);
    changeIconButton->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
    if (id.isNull())
        return;
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        QModelIndex selectionIndex = proxymodel->mapFromSource(index);
        view->selectionModel()->setCurrentIndex(selectionIndex, QItemSelectionModel::ClearAndSelect);
        updateStatusCenter();
    }
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
    if (!m_selectedInstance)
        return;

    InstanceId instId = m_selectedInstance->id();
    QString src(APPLICATION->instances()->getInstanceGroup(instId));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(src);
    bool ok = false;
    QString dst = QInputDialog::getItem(this, tr("Group name"), tr("Enter a new group name."), groups, index, true, &ok);
    dst = dst.simplified();

    if (ok) {
        APPLICATION->instances()->setInstanceGroup(instId, dst);
    }
}

void MainWindow::deleteGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    const int reply = QMessageBox::question(this, tr("Delete group"), tr("Are you sure you want to delete the group '%1'?").arg(group),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        APPLICATION->instances()->deleteGroup(group);
}

void MainWindow::renameGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    QString name = QInputDialog::getText(this, tr("Rename group"), tr("Enter a new group name."), QLineEdit::Normal, group);
    name = name.simplified();
    if (name.isNull() || name == group)
        return;

    const bool empty = name.isEmpty();
    const bool duplicate = APPLICATION->instances()->getGroups().contains(name, Qt::CaseInsensitive) && group.toLower() != name.toLower();

    if (empty || duplicate) {
        QMessageBox::warning(this, tr("Cannot rename group"), empty ? tr("Cannot set empty name.") : tr("Group already exists. :/"));
        return;
    }

    APPLICATION->instances()->renameGroup(group, name);
}

void MainWindow::undoTrashInstance()
{
    if (!APPLICATION->instances()->undoTrashInstance())
        QMessageBox::warning(
            this, tr("Failed to undo trashing instance"),
            tr("Some instances and shortcuts could not be restored.\nPlease check your trashbin to manually restore them."));
    ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
    DesktopServices::openPath(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
    QString str = APPLICATION->settings()->get("InstanceDir").toString();
    DesktopServices::openPath(str);
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewCatPackFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getCatPacksFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
    DesktopServices::openPath("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->javaPath(), true);
}

void MainWindow::refreshInstances()
{
    APPLICATION->instances()->loadList();
}

void MainWindow::checkForUpdates()
{
    if (APPLICATION->updaterEnabled()) {
        APPLICATION->triggerUpdateCheck();
    } else {
        qWarning() << "Updater not set up. Cannot check for updates.";
    }
}

void MainWindow::on_actionSettings_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "global-settings");
}

void MainWindow::globalSettingsClosed()
{
    // FIXME: quick HACK to make this work. improve, optimize.
    APPLICATION->instances()->loadList();
    proxymodel->invalidate();
    proxymodel->sort(0);
    updateMainToolBar();
    updateLaunchButton();
    updateThemeMenu();
    updateStatusCenter();
    applyVortexStyle();
    updateDashboard();
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but Prism Launcher exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    update();
}

void MainWindow::on_actionEditInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    if (m_selectedInstance->canEdit()) {
        APPLICATION->showInstanceWindow(m_selectedInstance);
    } else {
        CustomMessageBox::selectable(this, tr("Instance not editable"),
                                     tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."),
                                     QMessageBox::Critical)
            ->show();
    }
}

void MainWindow::on_actionManageAccounts_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "accounts");
}

void MainWindow::on_actionReportBug_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionClearMetadata_triggered()
{
    // This if contains side effects!
    if (!APPLICATION->metacache()->evictAll()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Metadata cache clear Failed!\nTo clear the metadata cache manually, press Folders -> View "
                                        "Launcher Root Folder, and after closing the launcher delete the folder named \"meta\"\n"),
                                     QMessageBox::Warning)
            ->show();
    }

    APPLICATION->metacache()->SaveNow();
}

#ifdef Q_OS_MAC
void MainWindow::on_actionAddToPATH_triggered()
{
    auto binaryPath = APPLICATION->applicationFilePath();
    auto targetPath = QString("/usr/local/bin/%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    qDebug() << "Symlinking" << binaryPath << "to" << targetPath;

    QStringList args;
    args << "-e";
    args << QString("do shell script \"mkdir -p /usr/local/bin && ln -sf '%1' '%2'\" with administrator privileges")
                .arg(binaryPath, targetPath);
    auto outcome = QProcess::execute("/usr/bin/osascript", args);
    if (!outcome) {
        QMessageBox::information(this, tr("Successfully added %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 tr("%1 was successfully added to your PATH. You can now start it by running `%2`.")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.LAUNCHER_APP_BINARY_NAME));
    } else {
        QMessageBox::critical(this, tr("Failed to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                              tr("An error occurred while trying to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}
#endif

void MainWindow::on_actionOpenWiki_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.WIKI_URL));
}

void MainWindow::on_actionMoreNews_triggered()
{
    if (!m_newsChecker || BuildConfig.NEWS_OPEN_URL.isEmpty())
        return;
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.exec();
}

void MainWindow::newsButtonClicked()
{
    if (!m_newsChecker)
        return;
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.toggleArticleList();
    news_dialog.exec();
}

void MainWindow::onCatChanged(int)
{
    setCatBackground(APPLICATION->settings()->get("TheCat").toBool());
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
    if (!m_selectedInstance) {
        return;
    }

    if (m_selectedInstance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Cannot Delete Running Instance"),
                                     tr("The selected instance is currently running and cannot be deleted. Please stop the instance before "
                                        "attempting to delete it."),
                                     QMessageBox::Warning, QMessageBox::Ok)
            ->exec();
        return;
    }
    auto id = m_selectedInstance->id();

    QString shortcutStr;
    auto shortcuts = m_selectedInstance->shortcuts();
    if (!shortcuts.isEmpty())
        shortcutStr = tr(" and its %n registered shortcut(s)", "", shortcuts.size());
    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\"%2.\n"
                                                    "This may be permanent and will completely delete the instance.\n\n"
                                                    "Are you sure?")
                                                     .arg(m_selectedInstance->name(), shortcutStr),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!checkLinkedInstances(id, this, tr("Deleting")))
        return;

    if (APPLICATION->instances()->trashInstance(id)) {
        ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
    } else {
        APPLICATION->instances()->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    selectionBad();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
    if (m_selectedInstance) {
        ExportInstanceDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceMrPack_triggered()
{
    if (m_selectedInstance) {
        auto instance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
        if (instance != nullptr) {
            ExportPackDialog dlg(instance, this);
            dlg.exec();
        }
    }
}

void MainWindow::on_actionExportInstanceFlamePack_triggered()
{
    if (m_selectedInstance) {
        auto instance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
        if (instance) {
            if (auto cmp = instance->getPackProfile()->getComponent("net.minecraft");
                cmp && cmp->getVersionFile() && cmp->getVersionFile()->type == "snapshot") {
                QMessageBox msgBox(this);
                msgBox.setText("Snapshots are currently not supported by CurseForge modpacks.");
                msgBox.exec();
                return;
            }
            ExportPackDialog dlg(instance, this, ModPlatform::ResourceProvider::FLAME);
            dlg.exec();
        }
    }
}

void MainWindow::on_actionRenameInstance_triggered()
{
    if (m_selectedInstance) {
        view->edit(view->currentIndex());
    }
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
    if (m_selectedInstance) {
        QString str = m_selectedInstance->instanceRoot();
        DesktopServices::openPath(QFileInfo(str));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    instanceToolbarSetting->set(QString::fromUtf8(ui->instanceToolBar->getVisibilityState().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
    if (!index.isValid())
        return;
    QString id = index.data(InstanceList::InstanceIDRole).toString();
    BaseInstance* inst = APPLICATION->instances()->getInstanceById(id);
    if (!inst)
        return;

    activateInstance(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
    if (m_selectedInstance && !m_selectedInstance->isRunning()) {
        APPLICATION->launch(m_selectedInstance);
    }
}

void MainWindow::activateInstance(BaseInstance* instance)
{
    APPLICATION->launch(instance);
}

void MainWindow::on_actionKillInstance_triggered()
{
    if (m_selectedInstance && m_selectedInstance->isRunning()) {
        APPLICATION->kill(m_selectedInstance);
    }
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
    if (!m_selectedInstance)
        return;

    CreateShortcutDialog shortcutDlg(m_selectedInstance, this);
    if (!shortcutDlg.exec())
        return;
    shortcutDlg.createShortcut();
}

void MainWindow::taskEnd()
{
    QObject* sender = QObject::sender();
    if (sender == m_versionLoadTask)
        m_versionLoadTask = NULL;

    sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
    connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
    connect(task, &Task::failed, this, &MainWindow::taskEnd);
    task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
    if (m_selectedInstance) {
        disconnect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        disconnect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    }
    QString id = current.data(InstanceList::InstanceIDRole).toString();
    m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
    if (m_selectedInstance) {
        ui->instanceToolBar->setEnabled(true);
        setInstanceActionsEnabled(true);
        ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch());

        ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
        ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
        renameButton->setText(m_selectedInstance->name());
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
        updateStatusCenter();
        updateInstanceToolIcon(m_selectedInstance->iconKey());

        updateLaunchButton();

        APPLICATION->settings()->set("SelectedInstance", m_selectedInstance->id());

        connect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        connect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
        updateDashboard();
    } else {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
}

void MainWindow::instanceSelectRequest(QString id)
{
    setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto current = view->selectionModel()->currentIndex();
    QItemSelection test(topLeft, bottomRight);
    if (test.contains(current)) {
        instanceChanged(current, current);
    }
}

void MainWindow::selectionBad()
{
    // start by reseting everything...
    m_selectedInstance = nullptr;
    m_statusLeft->setText(tr("No instance selected"));

    statusBar()->clearMessage();
    ui->instanceToolBar->setEnabled(false);
    setInstanceActionsEnabled(false);
    updateLaunchButton();
    renameButton->setText(tr("Rename Instance"));
    updateInstanceToolIcon("grass");

    // ...and then see if we can enable the previously selected instance
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());
    updateDashboard();
}

void MainWindow::checkInstancePathForProblems()
{
    QString instanceFolder = APPLICATION->settings()->get("InstanceDir").toString();
    if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'!\' and this is known to cause Java problems!"));
        warning.setInformativeText(tr("You have now two options: <br/>"
                                      " - change the instance folder in the settings <br/>"
                                      " - move this installation of %1 to a different folder")
                                       .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
    auto tempFolderText =
        tr("This is a problem: <br/>"
           " - The launcher will likely be deleted without warning by the operating system <br/>"
           " - close the launcher now and extract it to a real location, not a temporary folder");
    QString pathfoldername = QDir(instanceFolder).absolutePath();
    if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'Rar$\' - that means you haven't extracted the launcher archive!"));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    } else if (pathfoldername.startsWith(QDir::tempPath()) || pathfoldername.contains("/TempState/")) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder is in a temporary folder: \'%1\'!").arg(QDir::tempPath()));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
}

void MainWindow::updateStatusCenter()
{
    m_statusCenter->setVisible(APPLICATION->settings()->get("ShowGlobalGameTime").toBool());

    int timePlayed = APPLICATION->instances()->getTotalPlayTime();
    if (timePlayed > 0) {
        m_statusCenter->setText(
            tr("Total playtime: %1")
                .arg(Time::prettifyDuration(timePlayed, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
}
// "Instance actions" are actions that require an instance to be selected (i.e. "new instance" is not here)
// Actions that also require other conditions (e.g. a running instance) won't be changed.
void MainWindow::setInstanceActionsEnabled(bool enabled)
{
    ui->actionEditInstance->setEnabled(enabled);
    ui->actionChangeInstGroup->setEnabled(enabled);
    ui->actionViewSelectedInstFolder->setEnabled(enabled);
    ui->actionExportInstance->setEnabled(enabled);
    ui->actionDeleteInstance->setEnabled(enabled);
    ui->actionCopyInstance->setEnabled(enabled);
    ui->actionCreateInstanceShortcut->setEnabled(enabled);
}

void MainWindow::refreshCurrentInstance()
{
    auto current = view->selectionModel()->currentIndex();
    instanceChanged(current, current);
}
