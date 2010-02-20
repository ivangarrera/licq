// -*- c-basic-offset: 2 -*-
/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2007-2009 Licq developers
 *
 * Licq is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Licq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Licq; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "systemmenu.h"

#include "config.h"

#include <QList>

#include <licq_icqd.h>
#include <licq_log.h>
#include <licq/pluginmanager.h>
#include <licq_user.h>

#include "config/contactlist.h"
#include "config/general.h"
#include "config/iconmanager.h"
#include "config/shortcuts.h"

#include "contactlist/contactlist.h"

#include "dialogs/adduserdlg.h"
#include "dialogs/authuserdlg.h"
#include "dialogs/awaymsgdlg.h"
#include "dialogs/editgrpdlg.h"
#include "dialogs/historydlg.h"
#include "dialogs/logwindow.h"
#include "dialogs/ownermanagerdlg.h"
#include "dialogs/plugindlg.h"
#include "dialogs/randomchatdlg.h"
#include "dialogs/reqauthdlg.h"
#include "dialogs/searchuserdlg.h"
#include "dialogs/securitydlg.h"
#include "dialogs/gpgkeymanager.h"

#include "helpers/licqstrings.h"
#include "settings/settingsdlg.h"

#include "licqgui.h"
#include "mainwin.h"

using namespace LicqQtGui;
using namespace LicqQtGui::SystemMenuPrivate;
/* TRANSLATOR LicqQtGui::SystemMenu */
/* TRANSLATOR LicqQtGui::SystemMenuPrivate::OwnerData */

SystemMenu::SystemMenu(QWidget* parent)
  : QMenu(parent)
{
  QAction* a;

  // Sub menu Debug
  myDebugMenu = new QMenu(tr("Debug Level"));
  connect(myDebugMenu, SIGNAL(triggered(QAction*)), SLOT(changeDebug(QAction*)));
  connect(myDebugMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowDebugMenu()));
#define ADD_DEBUG(text, data, checkable) \
    a = myDebugMenu->addAction(text); \
    a->setCheckable(checkable); \
    a->setData(data);
  ADD_DEBUG(tr("Status Info"), L_INFO, true)
  ADD_DEBUG(tr("Unknown Packets"), L_UNKNOWN, true)
  ADD_DEBUG(tr("Errors"), L_ERROR, true)
  ADD_DEBUG(tr("Warnings"), L_WARN, true)
  ADD_DEBUG(tr("Packets"), L_PACKET, true)
  myDebugMenu->addSeparator();
  ADD_DEBUG(tr("Set All"), L_ALL, false)
  ADD_DEBUG(tr("Clear All"), L_NONE, false)
#undef ADD_DEBUG

  // Sub menu System Functions
  myOwnerAdmMenu = new QMenu(tr("S&ystem Functions"));
  myOwnerAdmMenu->addAction(tr("&View System Messages..."), LicqGui::instance(), SLOT(showAllOwnerEvents()));
  myOwnerAdmMenu->addSeparator();
  myOwnerAdmSeparator = myOwnerAdmMenu->addSeparator();
  myAccountManagerAction = myOwnerAdmMenu->addAction(tr("&Account Manager..."), this, SLOT(showOwnerManagerDlg()));
  myIcqSecurityAction = myOwnerAdmMenu->addAction(tr("ICQ &Security Options..."), this, SLOT(showSecurityDlg()));
  myIcqRandomChatGroupAction = myOwnerAdmMenu->addAction(tr("ICQ &Random Chat Group..."), this, SLOT(showRandomChatGroupDlg()));
  myOwnerAdmMenu->addSeparator();
  myOwnerAdmMenu->addMenu(myDebugMenu);

  // Sub menu User Functions
  myUserAdmMenu = new QMenu(tr("User &Functions"));
  myUserAdmMenu->addAction(tr("&Add User..."), this, SLOT(showAddUserDlg()));
  myUserSearchAction = myUserAdmMenu->addAction(tr("S&earch for User..."), this, SLOT(showSearchUserDlg()));
  myUserAutorizeAction = myUserAdmMenu->addAction(tr("A&uthorize User..."), this, SLOT(showAuthUserDlg()));
  myUserReqAutorizeAction = myUserAdmMenu->addAction(tr("Re&quest Authorization..."), this, SLOT(showReqAuthDlg()));
  myIcqRandomChatAction = myUserAdmMenu->addAction(tr("ICQ R&andom Chat..."), this, SLOT(showRandomChatSearchDlg()));
  myUserAdmMenu->addSeparator();
  myUserPopupAllAction = myUserAdmMenu->addAction(tr("&Popup All Messages..."), LicqGui::instance(), SLOT(showAllEvents()));
  myEditGroupsAction = myUserAdmMenu->addAction(tr("Edit &Groups..."), this, SLOT(showEditGrpDlg()));
  myUserAdmMenu->addSeparator();
  myUserAdmMenu->addAction(tr("Update All Users"), this, SLOT(updateAllUsers()));
  myUserAdmMenu->addAction(tr("Update Current Group"), this, SLOT(updateAllUsersInGroup()));
  myRedrawContactListAction = myUserAdmMenu->addAction(tr("&Redraw User Window"), LicqGui::instance()->contactList(), SLOT(reloadAll()));
  myUserAdmMenu->addAction(tr("&Save All Users"), this, SLOT(saveAllUsers()));

  // Sub menu Follow Me
  myFollowMeMenu = new QMenu(tr("ICQ Phone \"Follow Me\""));
  myFollowMeActions = new QActionGroup(this);
  connect(myFollowMeActions, SIGNAL(triggered(QAction*)), SLOT(setFollowMeStatus(QAction*)));
#define ADD_PFM(text, data) \
    a = myFollowMeActions->addAction(text); \
    a->setCheckable(true); \
    a->setData(static_cast<unsigned int>(data)); \
    myFollowMeMenu->addAction(a);
  ADD_PFM(tr("Don't Show"), ICQ_PLUGIN_STATUSxINACTIVE);
  ADD_PFM(tr("Available"), ICQ_PLUGIN_STATUSxACTIVE);
  ADD_PFM(tr("Busy"), ICQ_PLUGIN_STATUSxBUSY);
#undef ADD_PFM

  // Sub menu Status
  myStatusMenu = new QMenu(tr("&Status"));
  myStatusActions = new QActionGroup(this);
  connect(myStatusActions, SIGNAL(triggered(QAction*)), SLOT(setMainStatus(QAction*)));
  myStatusSeparator = myStatusMenu->addSeparator();
  myIcqFollowMeAction = myStatusMenu->addMenu(myFollowMeMenu);
  myIcqFollowMeSeparator = myStatusMenu->addSeparator();
#define ADD_MAINSTATUS(var, text, data) \
    var = myStatusActions->addAction(text); \
    var->setData(data); \
    myStatusMenu->addAction(var);
  ADD_MAINSTATUS(myStatusOnlineAction,
      LicqStrings::getStatus(ICQ_STATUS_ONLINE, false),
      ICQ_STATUS_ONLINE)
  ADD_MAINSTATUS(myStatusAwayAction,
      LicqStrings::getStatus(ICQ_STATUS_AWAY, false),
      ICQ_STATUS_AWAY)
  ADD_MAINSTATUS(myStatusNotAvailableAction,
      LicqStrings::getStatus(ICQ_STATUS_NA, false),
      ICQ_STATUS_NA)
  ADD_MAINSTATUS(myStatusOccupiedAction,
      LicqStrings::getStatus(ICQ_STATUS_OCCUPIED, false),
      ICQ_STATUS_OCCUPIED)
  ADD_MAINSTATUS(myStatusDoNotDisturbAction,
      LicqStrings::getStatus(ICQ_STATUS_DND, false),
      ICQ_STATUS_DND)
  ADD_MAINSTATUS(myStatusFreeForChatAction,
      LicqStrings::getStatus(ICQ_STATUS_FREEFORCHAT, false),
      ICQ_STATUS_FREEFORCHAT)
  ADD_MAINSTATUS(myStatusOfflineAction,
      LicqStrings::getStatus(ICQ_STATUS_OFFLINE, false),
      ICQ_STATUS_OFFLINE)
#undef ADD_MAINSTATUS
  myStatusMenu->addSeparator();
  myStatusInvisibleAction = myStatusMenu->addAction(
      LicqStrings::getStatus(ICQ_STATUS_FxPRIVATE, false),
      this, SLOT(toggleMainInvisibleStatus()));
  myStatusInvisibleAction->setCheckable(true);

  // Sub menu Group
  myGroupMenu = new QMenu(tr("&Group"));
  myUserGroupActions = new QActionGroup(this);
  connect(myGroupMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowGroupMenu()));
  connect(myUserGroupActions, SIGNAL(triggered(QAction*)), SLOT(setCurrentGroup(QAction*)));
  // Special entry for threaded view of all users
  a = myUserGroupActions->addAction(tr("All Groups (Threaded)"));
  a->setData(ContactListModel::SystemGroupOffset - 1);
  a->setCheckable(true);
  myGroupMenu->addAction(a);
#define ADD_SYSTEMGROUP(group) \
    a = myUserGroupActions->addAction(LicqStrings::getSystemGroupName(group)); \
    a->setData(ContactListModel::SystemGroupOffset + group); \
    a->setCheckable(true); \
    myGroupMenu->addAction(a);
  ADD_SYSTEMGROUP(GROUP_ALL_USERS);
  myGroupMenu->addSeparator();
  myGroupSeparator = myGroupMenu->addSeparator();
  for (int i = 1; i < NUM_GROUPS_SYSTEM_ALL; ++i)
  {
    ADD_SYSTEMGROUP(i);
  }
#undef ADD_SYSTEMGROUP

  // Sub menu Help
  myHelpMenu = new QMenu(tr("&Help"));
  myHelpMenu->addAction(tr("&Hints..."), gMainWindow, SLOT(showHints()));
  myHelpMenu->addAction(tr("&About..."), gMainWindow, SLOT(showAboutBox()));
  myHelpMenu->addAction(tr("&Statistics..."), gMainWindow, SLOT(showStats()));

  // System menu
  addMenu(myOwnerAdmMenu);
  addMenu(myUserAdmMenu);
  addMenu(myStatusMenu);
  addMenu(myGroupMenu);
  mySetArAction = addAction(tr("Set &Auto Response..."), gMainWindow, SLOT(showAwayMsgDlg()));
  addSeparator();
  myLogWinAction = addAction(tr("&Network Window..."), LicqGui::instance()->logWindow(), SLOT(show()));
  myMiniModeAction = addAction(tr("&Mini Mode"), Config::General::instance(), SLOT(setMiniMode(bool)));
  myMiniModeAction->setCheckable(true);
  myShowOfflineAction = addAction(tr("Show Offline &Users"), Config::ContactList::instance(), SLOT(setShowOffline(bool)));
  myShowOfflineAction->setCheckable(true);
  myShowEmptyGroupsAction = addAction(tr("Sh&ow Empty Groups"), Config::ContactList::instance(), SLOT(setShowEmptyGroups(bool)));
  myShowEmptyGroupsAction->setCheckable(true);
  myOptionsAction = addAction(tr("S&ettings..."), this, SLOT(showSettingsDlg()));
  myPluginManagerAction = addAction(tr("&Plugin Manager..."), this, SLOT(showPluginDlg()));
  myKeyManagerAction = addAction(tr("GPG &Key Manager..."), this, SLOT(showGPGKeyManager()));
  if (!gLicqDaemon->haveGpgSupport())
    myKeyManagerAction->setVisible(false);
  addSeparator();
  mySaveOptionsAction = addAction(tr("Sa&ve Settings"), LicqGui::instance(), SLOT(saveConfig()));
  addMenu(myHelpMenu);
  myShutdownAction = addAction(tr("E&xit"), gMainWindow, SLOT(slot_shutdown()));

  // The following shortcuts aren't shown in the menu but were
  // placed here to be groupped with other system actions.
  myPopupMessageAction = new QAction("Popup Next Message", gMainWindow);
  gMainWindow->addAction(myPopupMessageAction);
  connect(myPopupMessageAction, SIGNAL(triggered()), LicqGui::instance(), SLOT(showNextEvent()));
  myHideMainwinAction = new QAction("Hide Mainwindow", gMainWindow);
  gMainWindow->addAction(myHideMainwinAction);
  connect(myHideMainwinAction, SIGNAL(triggered()), gMainWindow, SLOT(hide()));
  myShowHeaderAction = new QAction("Show Column Headers", gMainWindow);
  gMainWindow->addAction(myShowHeaderAction);
  connect(myShowHeaderAction, SIGNAL(triggered()), Config::ContactList::instance(), SLOT(toggleShowHeader()));

  updateGroups();
  updateIcons();
  updateShortcuts();

  connect(IconManager::instance(), SIGNAL(iconsChanged()), SLOT(updateIcons()));
  connect(Config::Shortcuts::instance(), SIGNAL(shortcutsChanged()), SLOT(updateShortcuts()));

  connect(this, SIGNAL(aboutToShow()), SLOT(aboutToShowMenu()));
  connect(myFollowMeMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowFollowMeMenu()));

  // Sub menus are hidden until we got at least two owners
  myStatusSeparator->setVisible(false);

  // Hide ICQ specific menus until we actually get an ICQ owner
  setIcqEntriesVisible(false);
}

SystemMenu::~SystemMenu()
{
}

void SystemMenu::setIcqEntriesVisible(bool visible)
{
  myHasIcqOwner = visible;
  myIcqFollowMeAction->setVisible(visible);
  myIcqFollowMeSeparator->setVisible(visible);
  myIcqSecurityAction->setVisible(visible);
  myIcqRandomChatGroupAction->setVisible(visible);
  myIcqRandomChatAction->setVisible(visible);
}

void SystemMenu::updateIcons()
{
  IconManager* iconman = IconManager::instance();

  myKeyManagerAction->setIcon(iconman->getIcon(IconManager::GpgKeyIcon));

  myUserSearchAction->setIcon(iconman->getIcon(IconManager::SearchIcon));
  myUserAutorizeAction->setIcon(iconman->getIcon(IconManager::AuthorizeMessageIcon));
  myUserReqAutorizeAction->setIcon(iconman->getIcon(IconManager::ReqAuthorizeMessageIcon));

  myStatusOnlineAction->setIcon(iconman->iconForStatus(ICQ_STATUS_ONLINE));
  myStatusAwayAction->setIcon(iconman->iconForStatus(ICQ_STATUS_AWAY));
  myStatusNotAvailableAction->setIcon(iconman->iconForStatus(ICQ_STATUS_NA));
  myStatusOccupiedAction->setIcon(iconman->iconForStatus(ICQ_STATUS_OCCUPIED));
  myStatusDoNotDisturbAction->setIcon(iconman->iconForStatus(ICQ_STATUS_DND));
  myStatusFreeForChatAction->setIcon(iconman->iconForStatus(ICQ_STATUS_FREEFORCHAT));
  myStatusOfflineAction->setIcon(iconman->iconForStatus(ICQ_STATUS_OFFLINE));
  myStatusInvisibleAction->setIcon(iconman->iconForStatus(ICQ_STATUS_FxPRIVATE));

  foreach (OwnerData* data, myOwnerData.values())
    data->updateIcons();
}

void SystemMenu::updateGroups()
{
  QAction* a;

  // Clear old groups but leave system groups as they never change
  foreach (a, myUserGroupActions->actions())
    if (a->data().toInt() < ContactListModel::SystemGroupOffset-1)
      delete a;

  FOR_EACH_GROUP_START_SORTED(LOCK_R)
  {
    QString name = QString::fromLocal8Bit(pGroup->name().c_str());

    a = myUserGroupActions->addAction(name);
    a->setData(pGroup->id());
    a->setCheckable(true);

    myGroupMenu->insertAction(myGroupSeparator, a);
  }
  FOR_EACH_GROUP_END
}

void SystemMenu::updateShortcuts()
{
  Config::Shortcuts* shortcuts = Config::Shortcuts::instance();

  mySetArAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinSetAutoResponse));
  myLogWinAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinNetworkLog));
  myMiniModeAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinToggleMiniMode));
  myShowOfflineAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinToggleShowOffline));
  myShowEmptyGroupsAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinToggleEmptyGroups));
  myOptionsAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinSettings));
  myShutdownAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinExit));
  myHideMainwinAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinHide));

  myStatusOnlineAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusOnline));
  myStatusAwayAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusAway));
  myStatusNotAvailableAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusNotAvailable));
  myStatusOccupiedAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusOccupied));
  myStatusDoNotDisturbAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusDoNotDisturb));
  myStatusFreeForChatAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusFreeForChat));
  myStatusOfflineAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusOffline));
  myStatusInvisibleAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinStatusInvisible));

  myAccountManagerAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinAccountManager));
  myPopupMessageAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinPopupMessage));
  myUserPopupAllAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinPopupAllMessages));
  myEditGroupsAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinEditGroups));
  myRedrawContactListAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinRedrawContactList));
  myShowHeaderAction->setShortcut(shortcuts->getShortcut(Config::Shortcuts::MainwinToggleShowHeader));
}

void SystemMenu::addOwner(const UserId& userId)
{
  if (myOwnerData.count(userId) > 0)
    return;

  // Make we actually have a plugin protocol loaded for the owner,
  //   otherwise there is no point in including it in the menus.
  unsigned long ppid = LicqUser::getUserProtocolId(userId);
  Licq::ProtocolPlugin::Ptr protocol = Licq::gPluginManager.getProtocolPlugin(ppid);
  if (protocol.get() == NULL)
    return;

  OwnerData* newOwner = new OwnerData(ppid, protocol->getName(),
      protocol->getSendFunctions(), this);
  QMenu* ownerAdmin = newOwner->getOwnerAdmMenu();
  QMenu* ownerStatus = newOwner->getStatusMenu();
  myOwnerAdmMenu->insertMenu(myOwnerAdmSeparator, ownerAdmin);
  myStatusMenu->insertMenu(myStatusSeparator, ownerStatus);

  if (myOwnerData.size() < 1)
  {
    // There are no other owners, hide the sub menus
    ownerStatus->menuAction()->setVisible(false);
    ownerAdmin->menuAction()->setVisible(false);

    foreach (QAction* a, ownerAdmin->actions())
      myOwnerAdmMenu->insertAction(myOwnerAdmSeparator, a);
  }

  if (myOwnerData.size() == 1)
  {
    // Adding the second owner, show the sub menus for the first owner as well
    OwnerData* firstOwner = myOwnerData.begin().value();
    firstOwner->getStatusMenu()->menuAction()->setVisible(true);
    myStatusSeparator->setVisible(true);

    QMenu* firstOwnerAdm = firstOwner->getOwnerAdmMenu();
    firstOwnerAdm->menuAction()->setVisible(true);
    foreach (QAction* a, firstOwnerAdm->actions())
      myOwnerAdmMenu->removeAction(a);
  }

  if (ppid == LICQ_PPID)
    setIcqEntriesVisible(true);

  myOwnerData.insert(userId, newOwner);
}

void SystemMenu::removeOwner(const UserId& userId)
{
  OwnerData* data = myOwnerData.take(userId);
  if (data == NULL)
    return;

  delete data;

  unsigned long ppid = LicqUser::getUserProtocolId(userId);
  if (ppid == LICQ_PPID)
    setIcqEntriesVisible(false);

  if (myOwnerData.size() == 1)
  {
    // Only one owner left, hide the sub menus
    OwnerData* lastOwner = myOwnerData.begin().value();
    lastOwner->getStatusMenu()->menuAction()->setVisible(false);
    myStatusSeparator->setVisible(false);

    QMenu* lastOwnerAdm = lastOwner->getOwnerAdmMenu();
    lastOwnerAdm->menuAction()->setVisible(false);
    foreach (QAction* a, lastOwnerAdm->actions())
      myOwnerAdmMenu->insertAction(myOwnerAdmSeparator, a);
  }
}

bool SystemMenu::getInvisibleStatus(const UserId& userId) const
{
  OwnerData* data = myOwnerData.value(userId);
  if (data == NULL)
    return getInvisibleStatus();

  return data->getInvisibleStatus();
}

void SystemMenu::aboutToShowMenu()
{
  myMiniModeAction->setChecked(Config::General::instance()->miniMode());
  myShowOfflineAction->setChecked(Config::ContactList::instance()->showOffline());
  myShowEmptyGroupsAction->setChecked(Config::ContactList::instance()->showEmptyGroups());
}

void SystemMenu::aboutToShowFollowMeMenu()
{
   const ICQOwner* o = gUserManager.FetchOwner(LICQ_PPID, LOCK_R);
   if (o == NULL)
     return;

  int status = o->PhoneFollowMeStatus();

  foreach (QAction* a, myFollowMeActions->actions())
    if (a->data().toInt() == status)
      a->setChecked(true);

  gUserManager.DropOwner(o);
}

void SystemMenu::aboutToShowGroupMenu()
{
  int gid = Config::ContactList::instance()->groupId();
  if (Config::ContactList::instance()->groupType() == GROUPS_SYSTEM)
  {
    if (gid == GROUP_ALL_USERS && Config::ContactList::instance()->threadView())
      gid = -1;
    gid += ContactListModel::SystemGroupOffset;
  }

  foreach (QAction* a, myUserGroupActions->actions())
    if (a->data().toInt() == gid)
      a->setChecked(true);
}

void SystemMenu::aboutToShowDebugMenu()
{
  int logTypes = gOldLog.ServiceLogTypes(S_STDERR);

  foreach (QAction* a, myDebugMenu->actions())
    if (a->isCheckable())
      a->setChecked((a->data().toUInt() & logTypes) != 0);
}

void SystemMenu::changeDebug(QAction* action)
{
  int level = action->data().toUInt();

  if (level == L_ALL || level == L_NONE)
  {
    gOldLog.ModifyService(S_STDERR, level);
    return;
  }

  if (action->isChecked())
    gOldLog.AddLogTypeToService(S_STDERR, level);
  else
    gOldLog.RemoveLogTypeFromService(S_STDERR, level);
}

void SystemMenu::setCurrentGroup(QAction* action)
{
  int id = action->data().toInt();

  if (id == ContactListModel::SystemGroupOffset - 1)
    Config::ContactList::instance()->setGroup(GROUPS_SYSTEM, GROUP_ALL_USERS, true);
  else if (id < ContactListModel::SystemGroupOffset)
    Config::ContactList::instance()->setGroup(GROUPS_USER, id);
  else
    Config::ContactList::instance()->setGroup(GROUPS_SYSTEM, id - ContactListModel::SystemGroupOffset);
}

void SystemMenu::setFollowMeStatus(QAction* action)
{
  int id = action->data().toUInt();

  gLicqDaemon->icqSetPhoneFollowMeStatus(id);
}

void SystemMenu::setMainStatus(QAction* action)
{
  unsigned long status = action->data().toUInt();
  bool withMsg = false;
  if (status != ICQ_STATUS_OFFLINE && status != ICQ_STATUS_ONLINE)
  {
    // Only popup away message dialog if we have at least one owner with away message support
    foreach (OwnerData* data, myOwnerData.values())
      if (data->useAwayMessage())
        withMsg = true;
  }
  bool invisible = (myStatusInvisibleAction != NULL && myStatusInvisibleAction->isChecked());

  if (withMsg)
    AwayMsgDlg::showAwayMsgDlg(status, true, 0, invisible);
  else
    LicqGui::instance()->changeStatus(status, invisible);
}

void SystemMenu::toggleMainInvisibleStatus()
{
  LicqGui::instance()->changeStatus(ICQ_STATUS_FxPRIVATE, myStatusInvisibleAction->isChecked());
}

void SystemMenu::updateAllUsers()
{
  gLicqDaemon->UpdateAllUsers();
}

void SystemMenu::updateAllUsersInGroup()
{
  gLicqDaemon->UpdateAllUsersInGroup
  (
      Config::ContactList::instance()->groupType(),
      Config::ContactList::instance()->groupId()
  );
}

void SystemMenu::saveAllUsers()
{
   gUserManager.SaveAllUsers();
}

void SystemMenu::showOwnerManagerDlg()
{
  OwnerManagerDlg::showOwnerManagerDlg();
}

void SystemMenu::showSecurityDlg()
{
  new SecurityDlg();
}

void SystemMenu::showRandomChatGroupDlg()
{
  new SetRandomChatGroupDlg();
}

void SystemMenu::showAddUserDlg()
{
  new AddUserDlg();
}

void SystemMenu::showSearchUserDlg()
{
  new SearchUserDlg();
}

void SystemMenu::showAuthUserDlg()
{
  new AuthUserDlg(USERID_NONE, true);
}

void SystemMenu::showReqAuthDlg()
{
  new ReqAuthDlg();
}

void SystemMenu::showEditGrpDlg()
{
  new EditGrpDlg();
}

void SystemMenu::showRandomChatSearchDlg()
{
  new RandomChatDlg();
}

void SystemMenu::showSettingsDlg()
{
  SettingsDlg::show();
}

void SystemMenu::showPluginDlg()
{
  PluginDlg::showPluginDlg();
}

void SystemMenu::showGPGKeyManager()
{
  new GPGKeyManager();
}


OwnerData::OwnerData(unsigned long ppid, const QString& protoName,
    unsigned long sendFunctions, SystemMenu* parent)
  : QObject(parent),
    myPpid(ppid)
{
  myId = gUserManager.OwnerId(ppid).c_str();
  myUseAwayMessage = ((sendFunctions & PP_SEND_STATUSxMSG) != 0);

  // System sub menu
  myOwnerAdmMenu = new QMenu(protoName);
  myOwnerAdmInfoAction = myOwnerAdmMenu->addAction(tr("&Info..."), this, SLOT(viewInfo()));
  myOwnerAdmHistoryAction = myOwnerAdmMenu->addAction(tr("View &History..."), this, SLOT(viewHistory()));

  // Status sub menu
  myStatusMenu = new QMenu(protoName);
  myStatusActions = new QActionGroup(this);
  connect(myStatusActions, SIGNAL(triggered(QAction*)), SLOT(setStatus(QAction*)));
 #define ADD_STATUS(var, text, data, cond) \
    if (cond) \
    { \
      var = myStatusActions->addAction(text); \
      var->setData(data); \
      var->setCheckable(true); \
      myStatusMenu->addAction(var); \
    } \
    else \
    { \
      var = NULL; \
    }
  ADD_STATUS(myStatusOnlineAction,
      LicqStrings::getStatus(ICQ_STATUS_ONLINE, false),
      ICQ_STATUS_ONLINE,
      true);
  ADD_STATUS(myStatusAwayAction,
      LicqStrings::getStatus(ICQ_STATUS_AWAY, false),
      ICQ_STATUS_AWAY,
      true);
  ADD_STATUS(myStatusNotAvailableAction,
      LicqStrings::getStatus(ICQ_STATUS_NA, false),
      ICQ_STATUS_NA,
      myPpid != MSN_PPID);
  ADD_STATUS(myStatusOccupiedAction,
      LicqStrings::getStatus(ICQ_STATUS_OCCUPIED, false),
      ICQ_STATUS_OCCUPIED,
      myPpid != JABBER_PPID);
  ADD_STATUS(myStatusDoNotDisturbAction,
      LicqStrings::getStatus(ICQ_STATUS_DND, false),
      ICQ_STATUS_DND,
      myPpid != MSN_PPID);
  ADD_STATUS(myStatusFreeForChatAction,
      LicqStrings::getStatus(ICQ_STATUS_FREEFORCHAT, false),
      ICQ_STATUS_FREEFORCHAT,
      myPpid != MSN_PPID);
  ADD_STATUS(myStatusOfflineAction,
      LicqStrings::getStatus(ICQ_STATUS_OFFLINE, false),
      ICQ_STATUS_OFFLINE,
      true);
  if (myPpid != JABBER_PPID)
  {
    myStatusMenu->addSeparator();
    myStatusInvisibleAction = myStatusMenu->addAction(
        LicqStrings::getStatus(ICQ_STATUS_FxPRIVATE, false),
        this, SLOT(toggleInvisibleStatus()));
    myStatusInvisibleAction->setCheckable(true);
  }
  else
  {
    myStatusInvisibleAction = NULL;
  }
#undef ADD_STATUS

  connect(myStatusMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowStatusMenu()));

  updateIcons();
}

OwnerData::~OwnerData()
{
  delete myOwnerAdmMenu;
  delete myStatusMenu;
}

void OwnerData::updateIcons()
{
  IconManager* iconman = IconManager::instance();

  myOwnerAdmInfoAction->setIcon(iconman->getIcon(IconManager::InfoIcon));
  myOwnerAdmHistoryAction->setIcon(iconman->getIcon(IconManager::HistoryIcon));

#define SET_ICON(action, status) \
  if (action != NULL) \
    action->setIcon(iconman->iconForStatus(status, myId, myPpid))

  SET_ICON(myStatusOnlineAction, ICQ_STATUS_ONLINE);
  SET_ICON(myStatusAwayAction, ICQ_STATUS_AWAY);
  SET_ICON(myStatusNotAvailableAction, ICQ_STATUS_NA);
  SET_ICON(myStatusOccupiedAction, ICQ_STATUS_OCCUPIED);
  SET_ICON(myStatusDoNotDisturbAction, ICQ_STATUS_DND);
  SET_ICON(myStatusFreeForChatAction, ICQ_STATUS_FREEFORCHAT);
  SET_ICON(myStatusOfflineAction, ICQ_STATUS_OFFLINE);
  SET_ICON(myStatusInvisibleAction, ICQ_STATUS_FxPRIVATE);
#undef SET_ICON
}

void OwnerData::aboutToShowStatusMenu()
{
  const ICQOwner* o = gUserManager.FetchOwner(myPpid, LOCK_R);
  if (o == NULL)
    return;

  int status = o->Status();

  // Update protocol status
  foreach (QAction* a, myStatusActions->actions())
  {
    if (a->data().toInt() == status)
      a->setChecked(true);
  }

  if (myStatusInvisibleAction != NULL && status != ICQ_STATUS_OFFLINE)
    myStatusInvisibleAction->setChecked(o->StatusInvisible());

  gUserManager.DropOwner(o);
}

void OwnerData::viewInfo()
{
  LicqGui::instance()->showInfoDialog(mnuUserGeneral, gUserManager.ownerUserId(myPpid));
}

void OwnerData::viewHistory()
{
  new HistoryDlg(gUserManager.ownerUserId(myPpid));
}

void OwnerData::setStatus(QAction* action)
{
  int status = action->data().toInt();
  bool withMsg = (status != ICQ_STATUS_OFFLINE && status != ICQ_STATUS_ONLINE && myUseAwayMessage);
  bool invisible = (myStatusInvisibleAction != NULL && myStatusInvisibleAction->isChecked());

  if (withMsg)
    AwayMsgDlg::showAwayMsgDlg(status, true, myPpid, invisible);
  else
    LicqGui::instance()->changeStatus(status, myPpid, invisible);
}

void OwnerData::toggleInvisibleStatus()
{
  LicqGui::instance()->changeStatus(ICQ_STATUS_FxPRIVATE, myPpid, myStatusInvisibleAction->isChecked());
}
