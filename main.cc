
#include <unistd.h>
#include <stdlib.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QHostInfo>
#include <QTimer>
#include <QGroupBox>

#include "main.hh"

NetSocket sock;
ChatDialog *dialog;

QString stringifyHostPort(QHostAddress ipAddress, quint16 udpPort);

Peer::Peer(QHostAddress ip, quint16 port)
{
  ipAddress = ip;
  udpPortNumber = port;
  timer = new QTimer();
  connect(timer, SIGNAL(timeout()), this, SLOT(responseTimeout()));
}

ChatDialog::ChatDialog()
{
	setWindowTitle("Peerster");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new TextEntryBox(this);
  textline->setFocus();
  textline->setMaximumHeight(50);

  QGroupBox *groupBox = new QGroupBox(tr("Add New Connection"));

  newConnection = new TextEntryBox();
  newConnection->setMaximumHeight(35);

  QVBoxLayout *l = new QVBoxLayout();
  l->addWidget(newConnection);
  groupBox->setLayout(l);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
  layout->addWidget(groupBox);
	setLayout(layout);

  // Initialize our origin name
  qsrand((uint)((13*sock.currentPort) + (2*getpid()) + getuid()));
  randNum = qrand();
  originName = "Hugh-" + QString::number(randNum);
  seenMessages = new QHash<QString, QList<QVariantMap*>*>();
  wantList = new QVariantMap();
  count = 1;

  // Initialize the timer
  antiEntropyTimer = new QTimer();
  connect(antiEntropyTimer, SIGNAL(timeout()), this, SLOT(antiEntropy()));
  antiEntropyTimer->start(10000);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
          this, SLOT(gotReturnPressed()));
  connect(newConnection, SIGNAL(returnPressed()),
          this, SLOT(gotNewConnection()));
  connect(&sock, SIGNAL(readyRead()),
          this, SLOT(gotReadyRead()));
}

void ChatDialog::gotNewConnection()
{
  QString s = newConnection->toPlainText();
  newConnection->clear();
  processConnection(s);
}

void ChatDialog::processConnection(QString connection)
{
  QStringList strlst = connection.split(":");
  if (strlst.count() != 2)
    return;
  QString hostname = strlst[0];
  QString port = strlst[1];
  bool ok;
  int udpport = strlst[1].toInt(&ok);
  if (!ok || udpport > 65535 || udpport < 0)
    return;
  QHostAddress address(hostname);
  if (QAbstractSocket::IPv4Protocol == address.protocol())
  {
    Peer *p = new Peer(address, udpport);
    QString text = stringifyHostPort(address, udpport);
    if (!neighbors->contains(text))
    {
      qDebug() << "New neighbor" << text;
      neighbors->insert(text, p);
    }
  }
  else
  {
    QHostInfo hostInfo = QHostInfo::fromName(hostname);
    if (hostInfo.error() == QHostInfo::NoError)
    {
      qDebug() << "Whoopee looked up an IP Adress";
      if (!hostInfo.addresses().isEmpty())
      {
        QHostAddress a = hostInfo.addresses().first();
        QString text = stringifyHostPort(a, udpport);
        Peer *p = new Peer(a, udpport);
        if (!neighbors->contains(text))
        {
          qDebug() << "New neighbor" << text;
          neighbors->insert(text, p);
        }
      }
    }
  }
}

Peer *ChatDialog::getRandomPeer()
{
  QHash<QString, Peer*>::iterator i;
  for (i = neighbors->begin(); i != neighbors->end(); i++)
  {
    if (qrand() % neighbors->count() == 0)
    {
      return i.value();
    }
  }
  i--;
  return i.value();
}

bool ChatDialog::receiveMessage(QVariantMap *msg)
{
  QString originName = msg->value("Origin").toString();
  quint32 seqNo = msg->value("SeqNo").toUInt();
  if (seqNo == 0)
  {
    qDebug() << "ERROR We shouldn't be getting a SeqNo of 0 ERROR";
    return false;
  }
  // Update want vector
  if (!wantList->contains(originName))
  {
    (*wantList)[originName] = 1;
  }
  // If the want list is tracking the remote peer, but this isn't the message we want, return
  if (seqNo != wantList->value(originName).toUInt())
  {
    qDebug() << address << ": Ew, I don't want this message, I want" << wantList->value(originName).toUInt();
    return false;
  }
  else
  {
    (*wantList)[originName] = QVariant((quint32)seqNo + 1);
  }

  qDebug() << address << ": I was looking for message #" << seqNo << " and got it!";

  // Update message vector
  QList<QVariantMap*> *msgVector;
  if (seenMessages->contains(originName))
  {
    msgVector = seenMessages->value(originName);
  }
  else
  {
    msgVector = new QList<QVariantMap*>();
    (*seenMessages)[originName] = msgVector;
  }
  *msgVector << msg;
  return true;
}

QVariantMap *ChatDialog::makeMessage(QString s, QString origin, quint32 c)
{
  QVariant varText = QVariant(s);
  QVariant varOrigin = QVariant(origin);
  QVariant varSeqNo = QVariant(c);
  QVariantMap *map = new QVariantMap();
  map->insert("ChatText", varText);
  map->insert("Origin", varOrigin);
  map->insert("SeqNo", varSeqNo);
  return map;
}

void ChatDialog::gotReturnPressed()
{
  QString s = textline->toPlainText();
  QVariantMap *map = makeMessage(s, originName, count);

  if (receiveMessage(map))
  {
    count++;
  }

  rumorMonger(map);

  // Add to window
  textview->append("Me: " + s);

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::sendVariantMap(Peer *peer, QVariantMap *msg)
{
  // Write to data stream
  QByteArray data;
  QDataStream *serializer = new QDataStream(&data, QIODevice::WriteOnly);

  *serializer << *msg;
  delete serializer;

  sock.writeDatagram(data, peer->ipAddress, peer->udpPortNumber); 
}

void ChatDialog::rumorMonger(QVariantMap *msg)
{
  rumorMonger(msg, getRandomPeer());
}

void ChatDialog::rumorMonger(QVariantMap *msg, Peer *peer)
{
  sendVariantMap(peer, msg);
  peer->timer->start(2000);
  peer->waitMsg = msg;
}

void ChatDialog::antiEntropy()
{
  sendResponse(getRandomPeer());
}

void Peer::responseTimeout()
{
  qDebug() << dialog->address << ": Oops we timed out!";
  timer->stop();
  dialog->rumorMonger(waitMsg);
}

void ChatDialog::processDatagram(QByteArray datagram, QHostAddress sender, quint16 senderPort)
{
  QString origin = checkAddNeighbor(sender, senderPort);

  QDataStream *stream = new QDataStream(&datagram, QIODevice::ReadOnly);
  QVariantMap map;
  *stream >> map;
  delete stream;
  QVariantMap *mapRef = new QVariantMap(map);

  if (map.contains("Origin") && map.contains("SeqNo") && map.contains("ChatText"))
  {
    bool rcvMsgFlag = receiveMessage(mapRef);
    sendResponse(origin);
    if (rcvMsgFlag)
    {
      rumorMonger(mapRef);
      QString text = map.value("Origin").toString() + "(" + map.value("SeqNo").toString() + "): "
        + map.value("ChatText").toString();

      textview->append(text);
    }
  }
  else if (map.contains("Want"))
  {
    qDebug() << address << ": Got want from" << origin;
    Peer *peer = neighbors->value(origin);
    peer->timer->stop();

    QVariantMap collection;
    QVariantMap wantItem = map.value("Want").toMap();
    QVariantMap::iterator i;
    for (i = wantList->begin(); i != wantList->end(); i++)
    {
      if (wantItem.contains(i.key()))
      {
        quint32 theyWant = wantItem.value(i.key()).toUInt();
        if (theyWant < i.value().toUInt())
        {
          qDebug() << address << ":" << origin << "doesn't have the latest gossip!";
          rumorMonger(seenMessages->value(i.key())->at(theyWant - 1), peer);
          return;
        }
      }
      else
      {
        qDebug() << address << ":" << origin << "didn't even know this guy existed!";
        rumorMonger(seenMessages->value(i.key())->at(0), peer);
        return;
      }
    }
    for (i = wantItem.begin(); i != wantItem.end(); i++)
    {
      if (wantList->contains(i.key()))
      {
        quint32 weWant = wantList->value(i.key()).toUInt();
        if (weWant < i.value().toUInt())
        {
          qDebug() << address << ":" << origin << "is giving us new info!";
          sendResponse(origin);
          return;
        }
      }
      else
      {
        qDebug() << address << ":" << origin << "knows somebody we dont :(";
        sendResponse(origin);
        return;
      }
    }
    // If we're here that means that the two lists are equal
    qDebug() << address << ":" << origin << "and I have the same info now";
    if (qrand() % 2 == 0)
    {
      qDebug() << address << ":" << "Flipped a coin and it came up heads~";
      sendResponse(getRandomPeer());
    }
    qDebug() << address << ":" << "Coin came up tails D:";
  }
}

void ChatDialog::sendResponse(QString origin)
{
  sendResponse(neighbors->value(origin));
}

void ChatDialog::sendResponse(Peer *peer)
{
  qDebug() << "Sending want from" << address << "to" << peer->ipAddress.toString() + "?" + QString::number(peer->udpPortNumber);
  QVariantMap map;
  map.insert("Want", QVariant(*wantList));
  sendVariantMap(peer, &map);
}
  

QString ChatDialog::checkAddNeighbor(QHostAddress sender, quint16 senderPort)
{
  Peer *remotePeer;
  QString text = stringifyHostPort(sender, senderPort);
  if (neighbors->contains(text))
  {
    remotePeer = neighbors->value(text);
  }
  else
  {
    qDebug() << address << ": OH! A Challenger appears!";
    remotePeer = new Peer(sender, senderPort);
    (*neighbors)[text] = remotePeer;
  }
  return text;
}

void ChatDialog::gotReadyRead()
{
  while (sock.hasPendingDatagrams())
  {
    QByteArray datagram;
    datagram.resize(sock.pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;

    sock.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
    processDatagram(datagram, sender, senderPort);
  }
}

QString stringifyHostPort(QHostAddress ipAddress, quint16 udpPort)
{
  return ipAddress.toString() + "?" + QString::number(udpPort);
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four Peerster instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
  currentPort = -1;
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
      currentPort = p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

TextEntryBox::TextEntryBox(QWidget *parent) : QTextEdit(parent)
{
}

void TextEntryBox::keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
  {
    emit returnPressed();
  }
  else
  {
    QTextEdit::keyPressEvent(e);
  }
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
  dialog = new ChatDialog();
	dialog->show();

	// Create a UDP network socket
	if (!sock.bind())
		exit(1);

  // Letting our app know where it is
  dialog->address = stringifyHostPort(QHostAddress(QHostAddress::LocalHost), sock.currentPort);

  // Add our neighbors
  dialog->neighbors = new QHash<QString, Peer*>();
  QHostAddress localHost = QHostAddress(QHostAddress::LocalHost);
  for (quint16 i = sock.myPortMin; i <= sock.myPortMax; i++)
  {
    if (i == sock.currentPort)
      continue;
    quint16 neighborPort = i;
    Peer *p = new Peer(localHost, neighborPort);
    dialog->neighbors->insert(stringifyHostPort(localHost, neighborPort), p);
  }
  
  // Add neighbors from the command line
  QStringList args = QCoreApplication::arguments();
  for(int i = 1; i < args.length(); i++)
  {
    dialog->processConnection(args.at(i));
  }

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

