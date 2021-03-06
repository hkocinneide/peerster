#include <unistd.h>

#include "chatdialog.hh"
#include "peer.hh"
#include "textentrybox.hh"
#include "netsocket.hh"
#include "privatedialog.hh"
#include "fileshare.hh"
#include "searcher.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("Peerster");

  // Set the private chat window, hidden at first
  privatechat = new QDialog(this);
  privatechat->hide();
  privatechat->setWindowTitle("Private Chat");
  privateChatTable = new QHash<QString, PrivateDialog*>();

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

  // New connection box
  QGroupBox *groupBox = new QGroupBox(tr("Add New Connection"));

  newConnection = new TextEntryBox();
  newConnection->setMaximumHeight(35);

  QVBoxLayout *l = new QVBoxLayout();
  l->addWidget(newConnection);
  groupBox->setLayout(l);

  peerlist = new QListWidget(this);

  // Instantiate fileshare
  fileshare = new FileShare(this);

  // Make button for sharing files
  QPushButton *fileShareButton = new QPushButton("Share File", this);
  fileShareButton->setMaximumHeight(35);

  // Make fields for sharing files
  // blockRequestNode = new TextEntryBox(this);
  // blockRequestNode->setMaximumHeight(35);
  // blockRequestHash = new TextEntryBox(this);
  // blockRequestHash->setMaximumHeight(35);
  // blockRequestButton = new QPushButton("Request File", this);

  // Search logic
  QGroupBox *searchBoxOutline = new QGroupBox(tr("Search for files"));
  searchBox = new TextEntryBox();
  searchBox->setMaximumHeight(35);
  QVBoxLayout *m = new QVBoxLayout();
  m->addWidget(searchBox);
  searchBoxOutline->setLayout(m);

  searchList = new QListWidget(this);

  searcher = new Searcher(this, searchBox, searchList);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QGridLayout *layout = new QGridLayout();
	layout->addWidget(textview, 0, 0);
	layout->addWidget(textline, 1, 0);
  layout->addWidget(groupBox, 2, 0);
  layout->addWidget(peerlist, 0, 1, 2, 1);
  layout->addWidget(fileShareButton, 2, 1);
  // layout->addWidget(blockRequestNode, 0, 3);
  // layout->addWidget(blockRequestHash, 1, 3);
  // layout->addWidget(blockRequestButton, 2, 3);
  layout->addWidget(searchList, 0, 3, 2, 1);
  layout->addWidget(searchBoxOutline, 2, 3);
	setLayout(layout);

  // Initialize our origin name
  qsrand((uint)((13*sock.currentPort) + (2*getpid()) + getuid()));
  randNum = qrand();
  originName = "Hugh-" + QString::number(randNum);
  seenMessages = new QHash<QString, QList<QVariantMap*>*>();
  wantList = new QVariantMap();
  count = 1;

  // Initialize our routing table
  routingTable = new QHash<QString, QPair<QPair<QHostAddress, quint16>*, quint32>*>();

  // Initialize the timer
  antiEntropyTimer = new QTimer();
  connect(antiEntropyTimer, SIGNAL(timeout()), this, SLOT(antiEntropy()));
  antiEntropyTimer->start(10000);

  // Initialize the routing timer
  routingTimer = new QTimer();
  connect(routingTimer, SIGNAL(timeout()), this, SLOT(routeRumor()));
  routingTimer->start(60000);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
          this, SLOT(gotReturnPressed()));
  connect(newConnection, SIGNAL(returnPressed()),
          this, SLOT(gotNewConnection()));
  connect(&sock, SIGNAL(readyRead()),
          this, SLOT(gotReadyRead()));
  connect(peerlist, SIGNAL(itemActivated(QListWidgetItem *)),
          this, SLOT(peerActivated(QListWidgetItem *)));
  connect(fileShareButton, SIGNAL(clicked()),
          fileshare, SLOT(buttonPressed()));
  // connect(blockRequestButton, SIGNAL(clicked()),
  //         this, SLOT(requestButtonPressed()));
}

void ChatDialog::requestButtonPressed()
{
  // QString node = blockRequestNode->toPlainText();
  // QString hash = blockRequestHash->toPlainText();
  // blockRequestNode->clear();
  // blockRequestHash->clear();
  // fileshare->requestButtonPressed(node, hash);
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
    qDebug() << address << ": Ew, I don't want this message, I want"
             << wantList->value(originName).toUInt();
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
  QVariantMap *map = new QVariantMap();
  if (s != NULL)
  {
    QVariant varText = QVariant(s);
    map->insert("ChatText", varText);
  }
  QVariant varOrigin = QVariant(origin);
  QVariant varSeqNo = QVariant(c);
  map->insert("Origin", varOrigin);
  map->insert("SeqNo", varSeqNo);
  return map;
}

void ChatDialog::peerActivated(QListWidgetItem *item)
{
  if (privateChatTable->contains(item->text()))
  {
    PrivateDialog *chat = privateChatTable->value(item->text());
    chat->show();
    chat->raise();
    chat->activateWindow();
  }
  else
  {
    qDebug() << "ERROR switching layouts ERROR";
  }
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
  textview->append("Me (" + originName + "): " + s);

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::sendVariantMap(QHostAddress ipaddress, quint16 port, QVariantMap *msg)
{
  // Write to data stream
  QByteArray data;
  QDataStream *serializer = new QDataStream(&data, QIODevice::WriteOnly);

  *serializer << *msg;
  delete serializer;

  sock.writeDatagram(data, ipaddress, port); 
}

void ChatDialog::sendVariantMap(Peer *peer, QVariantMap *msg)
{
  sendVariantMap(peer->ipAddress, peer->udpPortNumber, msg);
}

void ChatDialog::rumorMongerAllPeers(QVariantMap *msg)
{
  QHash<QString, Peer*>::iterator i;
  for (i = neighbors->begin(); i != neighbors->end(); i++)
  {
    rumorMonger(msg, i.value());
  }
}

void ChatDialog::rumorMonger(QVariantMap *msg)
{
  rumorMonger(msg, getRandomPeer());
}

void ChatDialog::rumorMonger(QVariantMap *msg, Peer *peer)
{
  if (!noforward || !msg->contains("ChatText"))
  {
    sendVariantMap(peer, msg);
    peer->timer->start(2000);
    peer->waitMsg = msg;
  }
}

void ChatDialog::routeRumor()
{
  QVariantMap *msg = makeMessage(NULL, originName, count);
  if (receiveMessage(msg))
  {
    count++;
  }
  rumorMongerAllPeers(msg);
}

void ChatDialog::antiEntropy()
{
  sendResponse(getRandomPeer());
}

void ChatDialog::updateRoutingTable(QString origin, QHostAddress sender, quint16 senderPort, quint32 seqNo, bool direct)
{
  qDebug() << "Updating Routing Table";
  quint32 oldSeqNo = 0;
  bool updateRoutingFrame = (!routingTable->contains(origin) && origin != originName);
  if (routingTable->contains(origin))
  {
    oldSeqNo = routingTable->value(origin)->second;
  }
  if (seqNo < oldSeqNo)
  {
    qDebug() << "Old message, not adding to routing table";
    return;
  }
  else if (seqNo > oldSeqNo || (seqNo == oldSeqNo && direct))
  {
    qDebug() << "Refreshing the routing table";
    QPair<QHostAddress, quint16> *route = new QPair<QHostAddress, quint16>(sender, senderPort);
    QPair<QPair<QHostAddress,quint16>*, quint32> *entry = new QPair<QPair<QHostAddress, quint16>*, quint32>(route, seqNo);
    routingTable->insert(origin, entry);
  }

  if (updateRoutingFrame) {
    qDebug() << "Adding new entry to the routing table" << sender << senderPort;
    // Make a layout for the new potential private messages
    PrivateDialog *pcdialog = new PrivateDialog(origin);
    privateChatTable->insert(origin, pcdialog);

    new QListWidgetItem(origin, peerlist);
    peerlist->repaint();
  }
}

void ChatDialog::processDatagram(QByteArray datagram, QHostAddress sender, quint16 senderPort)
{
  QString origin = checkAddNeighbor(sender, senderPort);

  QDataStream *stream = new QDataStream(&datagram, QIODevice::ReadOnly);
  QVariantMap map;
  *stream >> map;
  delete stream;
  QVariantMap *mapRef = new QVariantMap(map);

  if (map.contains("Origin") && map.contains("SeqNo"))
  {
    // Deal with LastIP and LastPort
    if (mapRef->contains("LastIP") && mapRef->contains("LastPort"))
    {
      quint32 lIP = mapRef->value("LastIP").toUInt();
      quint16 lPort = mapRef->value("LastPort").toUInt();
      checkAddNeighbor(QHostAddress(lIP), lPort);
      updateRoutingTable(map.value("Origin").toString(), QHostAddress(lIP), lPort, map.value("SeqNo").toUInt(), true);
    }
    else
    {
      updateRoutingTable(map.value("Origin").toString(), sender, senderPort, map.value("SeqNo").toUInt(), false);
    }
    QVariant lastIP = QVariant(sender.toIPv4Address());
    QVariant lastPort = QVariant(senderPort);
    (*mapRef)["LastIP"] = lastIP;
    (*mapRef)["LastPort"] = lastPort;

    bool rcvMsgFlag = receiveMessage(mapRef);
    sendResponse(origin);
    if (rcvMsgFlag)
    {
      if (map.contains("ChatText"))
      {
        rumorMonger(mapRef);
        QString text = map.value("Origin").toString() + "(" +
                       map.value("SeqNo").toString() + "): " +
                       map.value("ChatText").toString();
        textview->append(text);
      }
      else
      {
        rumorMongerAllPeers(mapRef);
      }
    }
  }
  else if (map.contains("Want"))
  {
    qDebug() << address << ": Got want from" << origin;
    if (!neighbors->contains(origin))
    {
      qDebug() << "ERROR Why don't we have this peer? ERROR";
    }
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
          if (seenMessages->contains(i.key()))
          {
            rumorMonger(seenMessages->value(i.key())->at(theyWant - 1), peer);
          }
          else
          {
            qDebug() << "Our seenMessages doesn't have" << i.key();
          }
          return;
        }
      }
      else
      {
        qDebug() << address << ":" << origin << "didn't even know this guy existed!";
        if (seenMessages->contains(i.key()))
        {
          rumorMonger(seenMessages->value(i.key())->at(0), peer);
        }
        else
        {
          qDebug() << "They haven't seen this guy but we don't have him in our seenMessages";
        }
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
      sendResponse(getRandomPeer());
    }
  }
  else if (map.contains("Dest") && map.contains("Origin")
        && map.contains("HopLimit"))
  {
    if (!noforward)
    {
      qDebug() << address << ": Got a private message";
      QString dest = map.value("Dest").toString();
      QString org  = map.value("Origin").toString();
      // If we are the intended recipient
      if (originName == dest)
      {
        if (privateChatTable->contains(org))
        {
          if (map.contains("ChatText"))
          {
            PrivateDialog *pcdialog = privateChatTable->value(org);
            pcdialog->showMessage(org + ": " + map.value("ChatText").toString());
          }
          else if (map.contains("BlockRequest"))
          {
            fileshare->blockReply(org, map.value("BlockRequest").toByteArray());
          }
          else if (map.contains("BlockReply") && map.contains("Data"))
          {
            QByteArray receivedHash = map.value("BlockReply").toByteArray();
            QByteArray checkHash(QCryptographicHash::hash(map.value("Data").toByteArray(), QCryptographicHash::Sha1));
            if (checkHash != receivedHash)
            {
              qDebug() << "Not the right block";
            }
            if (receivedHash != *fileshare->blockWaitingOn)
            {
              qDebug() << "Not the one we're looking for";
            }
            if (fileshare->waitingOnBlockList)
            {
              fileshare->receiveBlockList(org, map.value("Data").toByteArray());
            }
            else
            {
              fileshare->receiveBlock(org, map.value("Data").toByteArray());
            }
          }
          else if (map.contains("SearchReply") && map.contains("MatchNames") && map.contains("MatchIDs"))
          {
            searcher->searchResult(map);
          }
        }
        else if (org == originName)
        {
          qDebug() << "Getting message from ourselves!";
        }
        else
        {
          qDebug() << address << ": ERROR??? DIDN'T FIND IN PRIVATE CHAT TABLE";
        }
      }
      // We are not the intended recipient
      else
      {
        quint32 hoplimit = map.value("HopLimit").toUInt();
        hoplimit--;
        // If HopLimit isn't zero, forward the message
        if (hoplimit != 0)
        {
          qDebug() << address << ": Attempting to forward the message";
          QString dest = map.value("Dest").toString();
          map["HopLimit"] = hoplimit;
          if (routingTable->contains(dest))
          {
            qDebug() << address << ": Found this guy in our routing table";
            QPair<QHostAddress, quint16> *destination = routingTable->value(dest)->first;
            sendVariantMap(destination->first, destination->second, &map);
          }
          else
          {
            qDebug() << address << ": Couldn't find this guy in our routing table";
          }
        }
        else
        {
          qDebug() << address << ": Hoplimit is 0, throwing out message";
        }
      }
    }
  }
  else if (map.contains("Origin") && map.contains("Search") && map.contains("Budget"))
  {
    searcher->receiveSearch(map);
  }
}

void ChatDialog::sendResponse(QString origin)
{
  if (!neighbors->contains(origin))
  {
    qDebug() << "ERROR Can't send response to this origin??? ERROR";
  }
  sendResponse(neighbors->value(origin));
}

void ChatDialog::sendResponse(Peer *peer)
{
  qDebug() << "Sending want from" << address << "to"
           << stringifyHostPort(peer->ipAddress, peer->udpPortNumber);
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

ChatDialog *ChatDialog::dialog = NULL;
