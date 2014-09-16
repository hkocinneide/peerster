#include "peer.hh"
#include "chatdialog.hh"

Peer::Peer(QHostAddress ip, quint16 port)
{
  ipAddress = ip;
  udpPortNumber = port;
  timer = new QTimer();
  connect(timer, SIGNAL(timeout()), this, SLOT(responseTimeout()));
}

void Peer::responseTimeout()
{
  qDebug() << ChatDialog::dialog->address << ": Oops we timed out!";
  timer->stop();
  ChatDialog::dialog->rumorMonger(waitMsg);
}

