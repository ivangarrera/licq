#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <qlistview.h>
#include <qtooltip.h>

#include "licq_constants.h"

class CUserEvent;
class MsgView;

//=====CMsgViewTips===============================================================================

class CMsgViewTips : public QToolTip
{
public:
  CMsgViewTips(MsgView * parent);
  virtual ~CMsgViewTips() {};

protected:
  virtual void maybeTip(const QPoint&);
};



//=====UserViewItem================================================================================
class MsgViewItem : public QListViewItem
{
public:
  MsgViewItem (CUserEvent *, QListView *);
  ~MsgViewItem(void);
  void MarkRead();

  CUserEvent *msg;
  int m_nEventId;

protected:
  virtual void paintCell(QPainter *, const QColorGroup &, int column, int width, int align);
  virtual void paintFocus(QPainter *, const QColorGroup &cg, const QRect &r) {}

  void SetEventLine();

friend class CMsgViewTips;
friend class MsgView;
};


//=====UserList====================================================================================
class MsgView : public QListView
{
public:
  MsgView (QWidget *parent = 0);
  CUserEvent *currentMsg(void);
  QSize sizeHint() const;
protected:
  CMsgViewTips *tips;

  virtual void resizeEvent(QResizeEvent *e);
  void SetEventLines();
};


#endif
