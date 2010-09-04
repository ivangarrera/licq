/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2010 Licq developers
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

#ifndef LICQDAEMON_ONEVENTMANAGER_H
#define LICQDAEMON_ONEVENTMANAGER_H

#include <licq/oneventmanager.h>

#include <map>

#include <licq/thread/lockable.h>
#include <licq/thread/mutex.h>
#include <licq/userid.h>

namespace Licq
{
class IniFile;
}

namespace LicqDaemon
{

class OnEventData : public Licq::OnEventData, public Licq::Lockable
{
public:
  /**
   * Constructor
   *
   * @param iniSection Section in config file for this data
   * @param isGlobal True if this is the global section
   */
  OnEventData(const std::string& iniSection, bool isGlobal = false);

  /**
   * Set user id for this data
   *
   * @param userId Id of user for this data
   */
  void setUserId(const Licq::UserId& userId)
  { myUserId = userId; }

  /**
   * Load default values for all parameters
   */
  void loadDefaults();

  /**
   * Load values from configuration file
   * Mutex must already be locked
   *
   * @param conf File to get parameters from
   */
  void load(Licq::IniFile& conf);

  /**
   * Save data to configuration file
   * Mutex must already be locked
   *
   * @param conf File to write configuration to
   */
  void save(Licq::IniFile& conf) const;

  /**
   * Merge with another data object
   * Any default values are replaced with data for the other object
   *
   * @param data Object to merge with
   */
  void merge(const Licq::OnEventData* data);

private:
  std::string myIniSection;
  bool myIsGlobal;
  Licq::UserId myUserId;
};

class OnEventManager : public Licq::OnEventManager
{
public:
  OnEventManager();
  ~OnEventManager();

  /**
   * Initialize the on event manager
   */
  void initialize();

  // From Licq::OnEventManager
  Licq::OnEventData* lockGlobal();
  Licq::OnEventData* lockGroup(int groupId, bool create = false);
  Licq::OnEventData* lockUser(const Licq::UserId& userId, bool create = false);
  void unlock(const Licq::OnEventData* data, bool save = false);
  Licq::OnEventData* getEffectiveUser(const Licq::User* user);
  void dropEffectiveUser(Licq::OnEventData* data);
  void performOnEvent(OnEventData::OnEventType event, const Licq::User* user);

private:
  OnEventData myGlobalData;
  std::map<int, OnEventData*> myGroupData;
  std::map<Licq::UserId, OnEventData*> myUserData;
  Licq::Mutex myDataMutex;
};

extern OnEventManager gOnEventManager;

} // namespace Licq

#endif
