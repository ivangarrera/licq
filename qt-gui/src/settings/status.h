/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2007-2012 Licq developers <licq-dev@googlegroups.com>
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

#ifndef SETTINGS_STATUS_H
#define SETTINGS_STATUS_H

#include <config.h>

#include <QObject>

class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace LicqQtGui
{
class MLEdit;
class SettingsDlg;
class SpecialSpinBox;

namespace Settings
{
class Status : public QObject
{
  Q_OBJECT

public:
  Status(SettingsDlg* parent);
  virtual ~Status() {}

  void load();
  void apply();

private slots:
  void sarMsgChanged(int msg);
  void sarGroupChanged(int group);
  void saveSar();
  void showSarHints();

private:
  /**
   * Setup the status page.
   *
   * @return a widget with the status settings
   */
  QWidget* createPageStatus(QWidget* parent);

  /**
   * Setup the response messages page.
   *
   * @return a widget with the response message settings
   */
  QWidget* createPageRespMsg(QWidget* parent);

  void buildAutoStatusCombos(bool firstTime);

  // Widgets for status settings
  QGroupBox* myGeneralBox;
  QVBoxLayout* myGeneralLayout;
  QGroupBox* myAutoAwayBox;
  QGridLayout* myAutoAwayLayout;
  QLabel* myAutoAwayLabel;
  QLabel* myAutoNaLabel;
  QLabel* myAutoOfflineLabel;
  SpecialSpinBox* myAutoAwaySpin;
  SpecialSpinBox* myAutoNaSpin;
  SpecialSpinBox* myAutoOfflineSpin;
  QComboBox* myAutoAwayMessCombo;
  QComboBox* myAutoNaMessCombo;

  // Widgets for response message settings
  QGroupBox* myDefRespMsgBox;
  QGridLayout* myDefRespMsgLayout;
  QLabel* mySarGroupLabel;
  QLabel* mySarMsgLabel;
  QComboBox* mySarGroupCombo;
  QComboBox* mySarMsgCombo;
  MLEdit* mySartextEdit;
  QPushButton* mySarsaveButton;
  QPushButton* mySarhintsButton;
};

} // namespace Settings
} // namespace LicqQtGui

#endif
