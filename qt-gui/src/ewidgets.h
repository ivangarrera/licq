#ifndef EWIDGETS_H
#define EWIDGETS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <qlabel.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qvalidator.h>


bool QueryUser(QWidget *, QString, QString, QString);
void InformUser(QWidget *q, QString);


class CELabel : public QLabel
{
  Q_OBJECT
public:
  CELabel(bool _bTransparent, QPopupMenu *m = NULL, QWidget *parent = 0, char *name = 0);
  void setBold(bool);
  void setItalic(bool);
  void setNamedFgColor(char *);
  void setNamedBgColor(char *);
public slots:
  void polish();
protected:
  void resizeEvent (QResizeEvent *);
  virtual void mouseDoubleClickEvent(QMouseEvent *);
  virtual void mouseReleaseEvent(QMouseEvent *e);
  bool m_bTransparent;
  QPopupMenu *mnuPopUp;
signals:
  void doubleClicked();
};


class CEButton : public QPushButton
{
  Q_OBJECT
public:
  CEButton(QPixmap *, QPixmap *, QPixmap *, QWidget *parent = 0, char *name = 0);
  CEButton(const char *, QWidget *parent = 0, char *name = 0);
  void setNamedFgColor(char *);
  void setNamedBgColor(char *);
  ~CEButton(void);
public slots:
  void polish();
protected:
  QPixmap *pmUpFocus, *pmUpNoFocus, *pmDown,
          *pmCurrent;
  // overloaded drawButton to remove typical button 3D effect
  virtual void drawButton(QPainter *);
  virtual void mouseReleaseEvent(QMouseEvent *e);
  virtual void mousePressEvent(QMouseEvent *e);
  virtual void enterEvent (QEvent *);
  virtual void leaveEvent (QEvent *);
};


class CEComboBox : public QComboBox
{
//   Q_OBJECT
public:
   CEComboBox (bool _bAppearEnabledAlways, QWidget *parent = 0, char *name = 0);
   void setNamedFgColor(char *);
   void setNamedBgColor(char *);
protected:
   bool m_bAppearEnabledAlways;
};




//=====CInfoField===============================================================
class CInfoField : public QLineEdit
{
  Q_OBJECT
public:
  CInfoField(int x, int y, int, int, int, QString title, bool isReadOnly, QWidget *parent);
  void setData(const char *data);
  void setData(QString data);
  void setData(const unsigned long data);
  void setGeometry(int, int, int, int, int);
  void move(int, int);
public slots:
  void setEnabled(bool);
protected:
  bool m_bReadOnly;
  QLabel *lblTitle;
  void keyPressEvent( QKeyEvent *e );
  void mouseReleaseEvent(QMouseEvent *e);
};

#endif
