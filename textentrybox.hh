#include "main.hh"

class TextEntryBox : public QTextEdit
{
  Q_OBJECT

public:
  TextEntryBox(QWidget *parent = 0);

protected:
  void keyPressEvent(QKeyEvent *e);

signals:
  void returnPressed();
};
