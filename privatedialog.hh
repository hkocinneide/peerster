#include "qinclude.hh"

#define HOPLIMIT ((quint32)10)

class TextEntryBox;

class PrivateDialog: public QDialog
{
  Q_OBJECT

public:
  TextEntryBox *textline;
  QTextEdit *textview;
  QString destination;
  QHostAddress ipaddress;
  quint16 port;

  PrivateDialog(QString);
  void showMessage(QString);
  QVariantMap *makePrivateMessage(QString dest);


public slots:
  void gotReturnPressed();
};
