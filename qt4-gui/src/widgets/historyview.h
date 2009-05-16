/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2007 Licq developers
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

#ifndef HISTORYVIEW_H
#define HISTORYVIEW_H

#include <licq_constants.h>

#include "mlview.h"

#include <licq_types.h>

class CUserEvent;
class LicqEvent;

namespace LicqQtGui
{
class HistoryView : public MLView
{
  Q_OBJECT

public:
  static QStringList getStyleNames(bool includeHistoryStyles = false);

  /**
   * Constructor
   *
   * @param historyMode True to use history style setting, false to use chat style settings
   * @param userId User to display chat history for
   * @param parent Parent widget
   */
  HistoryView(bool historyMode = false, const UserId& userId = USERID_NONE, QWidget* parent = 0);
  virtual ~HistoryView();

  void setHistoryConfig(unsigned short msgStyle, QString dateFormat,
      bool extraSpacing, bool reverse);
  void setChatConfig(unsigned short msgStyle, QString dateFormat,
      bool extraSpacing, bool appendLineBreak, bool showNotices);
  void setColors(QString back, QString rcv, QString snt,
      QString rcvHist = QString(), QString sntHist = QString(),
      QString notice = QString());
  void setReverse(bool reverse);

  /**
   * Change user this history is associated with
   *
   * @param userId New user
   */
  void setOwner(const UserId& userId);

  void updateContent();
  void clear();
  void addMsg(direction dir, bool fromHistory, QString eventDescription, QDateTime date,
    bool isDirect, bool isMultiRec, bool isUrgent, bool isEncrypted,
    QString contactName, QString messageText, QString anchor = QString());
  void addNotice(QDateTime dateTime, QString messageText);

  virtual QSize sizeHint() const;

public slots:
  void addMsg(const CUserEvent* event, const UserId& userId = USERID_NONE);
  void addMsg(const LicqEvent* event);
  void setColors();

signals:
  void messageAdded();

private:
  void internalAddMsg(QString s);

  UserId myUserId;
  unsigned short myMsgStyle;
  QString myDateFormat;
  bool myExtraSpacing;
  bool myReverse;
  bool myAppendLineBreak;
  bool myUseBuffer;
  bool myShowNotices;
  QString myColorRcvHistory;
  QString myColorSntHistory;
  QString myColorRcv;
  QString myColorSnt;
  QString myColorNotice;
  QString myBuffer;
};

} // namespace LicqQtGui

#endif
