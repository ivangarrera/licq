#ifndef MLEDIT_H
#define MLEDIT_H

#include <qmultilineedit.h>

class MLEditWrap : public QMultiLineEdit
{
  Q_OBJECT
public:
  MLEditWrap (bool wordWrap, QWidget* parent=0, bool handlequotes = false, const char *name=0);
  void appendNoNewLine(QString s);
  void GotoEnd(void);

  static QFont* editFont;
public slots:
  void clear();

protected:
  bool m_bDoQuotes;
  virtual void keyPressEvent (QKeyEvent *);
  void paintCell(QPainter* p, int row, int col);

signals:
  void signal_CtrlEnterPressed();
};

#endif
