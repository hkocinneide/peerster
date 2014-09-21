#include "privatedialog.hh"
#include "textentrybox.hh"
#include "chatdialog.hh"

PrivateDialog::PrivateDialog(QString dest)
{
  if (!ChatDialog::dialog->routingTable->contains(dest))
  {
    qDebug() << "WHAT IS HAPPENING TO THE ROUTING TABLE";
  }
  QPair<QHostAddress, quint16> *p = ChatDialog::dialog->routingTable->value(dest);
  ipaddress = p->first;
  port = p->second;
  destination = dest;
  textview = new QTextEdit();
  textview->setReadOnly(true);
  textline = new TextEntryBox();
  textline->setMaximumHeight(50);
  QGridLayout *l = new QGridLayout();
  l->addWidget(textview, 0, 0);
  l->addWidget(textline, 1, 0);
  setLayout(l);
  setWindowTitle("Chat with " + destination);

  connect(textline, SIGNAL(returnPressed()),
          this, SLOT(gotReturnPressed()));
}

void PrivateDialog::gotReturnPressed()
{
  QString s = textline->toPlainText();

  textview->append("Me: " + s);
  ChatDialog::dialog->sendVariantMap(ipaddress, port, makePrivateMessage(s));
  textline->clear();
}

void PrivateDialog::showMessage(QString s)
{
  textview->append(s);
}

QVariantMap *PrivateDialog::makePrivateMessage(QString chattext)
{
  QVariantMap *msg = new QVariantMap();
  msg->insert("Dest", destination);
  msg->insert("ChatText", chattext);
  msg->insert("HopLimit", HOPLIMIT);
  msg->insert("Origin", ChatDialog::dialog->originName);
  return msg;
}
