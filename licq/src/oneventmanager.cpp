/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2010-2014 Licq developers <licq-dev@googlegroups.com>
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

#include "oneventmanager.h"

#include <licq/contactlist/owner.h>
#include <licq/contactlist/user.h>
#include <licq/contactlist/usermanager.h>
#include <licq/inifile.h>
#include <licq/thread/mutexlocker.h>

#include <boost/foreach.hpp>
#include <cstdlib> // system, atoi
#include <ctime> // time
#include <sstream>

#include "daemon.h"

using namespace LicqDaemon;
using Licq::UserId;
using Licq::gUserManager;
using std::list;
using std::map;
using std::string;

const char* const Licq::OnEventData::Default = "default";

OnEventData::OnEventData(const string& iniSection, bool isGlobal) :
  myIniSection(iniSection),
  myIsGlobal(isGlobal),
  myHasChanged(false)
{
  // Empty
}

void OnEventData::loadDefaults()
{
  myEnabled = EnabledDefault;
  myAlwaysOnlineNotify = -1;
  myCommand = Default;
  for (int i = 0; i < NumOnEventTypes; ++i)
    myParameters[i] = Default;
}

void OnEventData::load(Licq::IniFile& conf)
{
  string soundDir;
  if (myIsGlobal)
    soundDir = gDaemon.shareDir() + "sounds/icq/";

  conf.get("Enable", myEnabled, myIsGlobal ? EnabledOnline : EnabledDefault);
  conf.get("AlwaysOnlineNotify", myAlwaysOnlineNotify, myIsGlobal ? 1 : -1);
  conf.get("Command", myCommand, myIsGlobal ? "play" : Default);
  conf.get("Message", myParameters[OnEventMessage], myIsGlobal ? (soundDir + "Message.wav") : Default);
  conf.get("Url", myParameters[OnEventUrl], myIsGlobal ? (soundDir + "URL.wav") : Default);
  conf.get("Chat", myParameters[OnEventChat], myIsGlobal ? (soundDir + "Chat.wav") : Default);
  conf.get("File", myParameters[OnEventFile], myIsGlobal ? (soundDir + "File.wav") : Default);
  conf.get("OnlineNotify", myParameters[OnEventOnline], myIsGlobal ? (soundDir + "Online.wav") : Default);
  conf.get("SysMsg", myParameters[OnEventSysMsg], myIsGlobal ? (soundDir + "System.wav") : Default);
  conf.get("MsgSent", myParameters[OnEventMsgSent], myIsGlobal ? (soundDir + "Message.wav") : Default);
}

void OnEventData::setEnabled(int enabled)
{
  if (enabled == myEnabled)
    return;

  myEnabled = enabled;
  myHasChanged = true;
}

void OnEventData::setAlwaysOnlineNotify(int alwaysOnlineNotify)
{
  if (alwaysOnlineNotify == myAlwaysOnlineNotify)
    return;

  myAlwaysOnlineNotify = alwaysOnlineNotify;
  myHasChanged = true;
}

void OnEventData::setCommand(const std::string& command)
{
  if (command == myCommand)
    return;

  myCommand = command;
  myHasChanged = true;
}

void OnEventData::setParameter(int event, const std::string& parameter)
{
  if (parameter == myParameters[event])
    return;

  myParameters[event] = parameter;
  myHasChanged = true;
}

void OnEventData::save(Licq::IniFile& conf) const
{
  myHasChanged = false;

  bool allDefault = (myEnabled == EnabledDefault && myAlwaysOnlineNotify == -1
      && myCommand == Default);
  for (int i = 0; i < NumOnEventTypes; ++i)
    if (myParameters[i] != Default)
      allDefault = false;

  if (allDefault)
  {
    // Everything is set to default, no point in having this section in file
    conf.removeSection(myIniSection);
    return;
  }

  conf.setSection(myIniSection);

  if (myUserId.isValid())
  {
    conf.set("Protocol", Licq::protocolId_toString(myUserId.protocolId()));
    conf.set("Owner", myUserId.ownerId().accountId());
    conf.set("User", myUserId.accountId());
  }
  conf.set("Enable", myEnabled);
  conf.set("AlwaysOnlineNotify", myAlwaysOnlineNotify);
  conf.set("Command", myCommand);
  conf.set("Message", myParameters[OnEventMessage]);
  conf.set("Url", myParameters[OnEventUrl]);
  conf.set("Chat", myParameters[OnEventChat]);
  conf.set("File", myParameters[OnEventFile]);
  conf.set("OnlineNotify", myParameters[OnEventOnline]);
  conf.set("SysMsg", myParameters[OnEventSysMsg]);
  conf.set("MsgSent", myParameters[OnEventMsgSent]);
}

void OnEventData::merge(const Licq::OnEventData* data)
{
  if (myEnabled == EnabledDefault)
    myEnabled = data->enabled();
  if (myAlwaysOnlineNotify == -1)
    myAlwaysOnlineNotify = data->alwaysOnlineNotify();
  if (myCommand == Default)
    myCommand = data->command();
  for (int i = 0; i < NumOnEventTypes; ++i)
    if (myParameters[i] == Default)
      myParameters[i] = data->parameter(i);
}


// Declare global OnEventManager (internal for daemon)
LicqDaemon::OnEventManager LicqDaemon::gOnEventManager;

// Declare global Licq::OnEventManager to refer to the internal OnEventManager
Licq::OnEventManager& Licq::gOnEventManager(LicqDaemon::gOnEventManager);


OnEventManager::OnEventManager()
  : myGlobalData("global", true)
{
  // Empty
}

OnEventManager::~OnEventManager()
{
  // Delete data for users
  for (map<UserId, OnEventData*>::iterator i = myUserData.begin(); i != myUserData.end(); ++i)
    delete i->second;
  myUserData.clear();

  // Delete data for groups
  for (map<int, OnEventData*>::iterator i = myGroupData.begin(); i != myGroupData.end(); ++i)
    delete i->second;
  myGroupData.clear();
}

void OnEventManager::initialize()
{
  Licq::IniFile conf("onevent.conf");
  conf.loadFile();

  // Global configuration
  conf.setSection("global");
  myGlobalData.load(conf);

  // Groups configuration
  list<string> sections;
  conf.getSections(sections, "Group.");
  BOOST_FOREACH(const string& section, sections)
  {
    conf.setSection(section);

    int groupId = atoi(section.substr(6).c_str());
    if (groupId <= 0)
      continue;
    OnEventData* data = new OnEventData(section);
    data->load(conf);
    myGroupData[groupId] = data;
  }

  // Users configuration
  conf.getSections(sections, "User.");
  BOOST_FOREACH(const string& section, sections)
  {
    conf.setSection(section);

    // User id in section header is not reliable as IniFile drops unallowed characters
    // Read user id from parameters in section instead
    string ppidStr, ownerIdStr, userIdStr;
    conf.get("Protocol", ppidStr);
    conf.get("Owner", ownerIdStr);
    conf.get("User", userIdStr);
    unsigned long protocolId = Licq::protocolId_fromString(ppidStr);
    if (protocolId == 0 || ownerIdStr.empty() || userIdStr.empty())
      continue;
    UserId userId(UserId(protocolId, ownerIdStr), userIdStr);

    OnEventData* data = new OnEventData(section);
    data->load(conf);
    data->setUserId(userId);
    myUserData[userId] = data;
  }
}


Licq::OnEventData* OnEventManager::lockGlobal()
{
  myGlobalData.lockWrite();
  return &myGlobalData;
}

Licq::OnEventData* OnEventManager::lockGroup(int groupId, bool create)
{
  Licq::MutexLocker lock(myDataMutex);

  OnEventData* data;
  if (myGroupData.count(groupId) == 0)
  {
    if (create == false)
      return NULL;

    std::ostringstream section;
    section << "Group." << groupId;
    data = new OnEventData(section.str());
    data->loadDefaults();
    myGroupData[groupId] = data;
  }
  else
  {
    data = myGroupData[groupId];
  }

  data->lockWrite();
  return data;
}

Licq::OnEventData* OnEventManager::lockUser(const UserId& userId, bool create)
{
  Licq::MutexLocker lock(myDataMutex);

  OnEventData* data;
  if (myUserData.count(userId) == 0)
  {
    if (create == false)
      return NULL;

    data = new OnEventData("User." + userId.ownerId().toString() + "." + userId.accountId());
    data->loadDefaults();
    data->setUserId(userId);
    myUserData[userId] = data;
  }
  else
  {
    data = myUserData[userId];
  }

  data->lockWrite();
  return data;
}

void OnEventManager::unlock(const Licq::OnEventData* data, bool save)
{
  if (data == NULL)
    return;

  const OnEventData* realData = dynamic_cast<const OnEventData*>(data);

  if (save && realData->hasChanged())
  {
    Licq::IniFile conf("onevent.conf");
    conf.loadFile();
    realData->save(conf);
    conf.writeFile();
  }

  realData->unlockWrite();
}

Licq::OnEventData* OnEventManager::getEffectiveUser(const Licq::User* user)
{
  OnEventData* data = new OnEventData("");
  data->loadDefaults();

  if (user != NULL)
  {
    // Merge with user specific settings
    Licq::OnEventData* userData = lockUser(user->id(), false);
    if (userData != NULL)
    {
      data->merge(userData);
      unlock(userData);
    }

    // Merge with groups settings
    BOOST_FOREACH(int groupId, user->GetGroups())
    {
      Licq::OnEventData* groupData = lockGroup(groupId, false);
      if (groupData == NULL)
        continue;
      data->merge(groupData);
      unlock(groupData);
    }
  }

  // Finally merge with global settings
  Licq::OnEventData* globalData = lockGlobal();
  data->merge(globalData);
  unlock(globalData);

  return data;
}

Licq::OnEventData* OnEventManager::getEffectiveGroup(int groupId)
{
  OnEventData* data = new OnEventData("");
  data->loadDefaults();

  if (groupId != 0)
  {
    // Merge with groups settings
    Licq::OnEventData* groupData = lockGroup(groupId, false);
    if (groupData != NULL)
    {
      data->merge(groupData);
      unlock(groupData);
    }
  }

  // Finally merge with global settings
  Licq::OnEventData* globalData = lockGlobal();
  data->merge(globalData);
  unlock(globalData);

  return data;
}

void OnEventManager::dropEffective(Licq::OnEventData* data)
{
  if (data == NULL)
    return;

  delete dynamic_cast<OnEventData*>(data);
}

void OnEventManager::performOnEvent(OnEventData::OnEventType event, const Licq::User* user)
{
  bool justOnline = false;
  int statusLevel = OnEventData::EnabledAlways;

  if (user != NULL)
  {
    if (user->onEventsBlocked())
      return;

    if (event == OnEventData::OnEventOnline)
    {
      if (!user->OnlineNotify())
        return;

      // We cannot always differentiate from users going online and users
      //   already online when we sign on. Make a guess based on online since.
      if (user->OnlineSince()+60 < time(NULL))
        justOnline = true;
    }

    // Check if current status is reason to block on event action
    unsigned ownerStatus;
    if (user->isUser())
    {
      // Get owner for this user
      Licq::OwnerReadGuard o(user->id().ownerId());
      ownerStatus = o->status();
    }
    else
    {
      // This already is an owner
      ownerStatus = user->status();
    }

    // Get enable level needed to enable on events for current status
    if (ownerStatus & Licq::User::DoNotDisturbStatus)
      statusLevel = OnEventData::EnabledAlways;
    else if (ownerStatus & Licq::User::OccupiedStatus)
      statusLevel = OnEventData::EnabledOccupied;
    else if (ownerStatus & Licq::User::NotAvailableStatus)
      statusLevel = OnEventData::EnabledNotAvailable;
    else if (ownerStatus & Licq::User::AwayStatus)
      statusLevel = OnEventData::EnabledAway;
    else
      statusLevel = OnEventData::EnabledOnline;

  }

  Licq::OnEventData* data = getEffectiveUser(user);
  string param;

  // Check if we should do anything at all
  if (data->enabled() < statusLevel)
    goto SkipPerformOnEvent;

  // Ignore due to just going online?
  if (justOnline && data->alwaysOnlineNotify() == 0)
    goto SkipPerformOnEvent;

  // Get parameter
  param = data->parameter(event);
  if (user != NULL)
    param = user->usprintf(param, Licq::User::usprintf_quoteall);

  if (!param.empty())
  {
    string fullCmd = data->command() + " " + param + " &";
    system(fullCmd.c_str());
  }

SkipPerformOnEvent:
  // Cleanup
  dropEffective(data);
}
