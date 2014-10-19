#include "fileshare.hh"
#include "chatdialog.hh"
#include "privatedialog.hh"

FileShare::FileShare(ChatDialog *parent)
{
  dialog = parent;
  sharedFiles = new QList<SharedFile*>();
  blocks = new QMap<QByteArray, QByteArray*>();
  blockWaitingOn = NULL;
  waitingOnBlockList = false;
  constructingFile = false;
  resendRequestTimer = new QTimer();
  connect(resendRequestTimer, SIGNAL(timeout()),
          this, SLOT(resendRequest()));
}

SharedFile::SharedFile(QString fname, QByteArray *mhash, QByteArray *blist, QList<QByteArray*> *d)
{
  filename = fname;
  metahash = mhash;
  blocklist = blist;
  data = d;
}

QList<QByteArray*> *FileShare::processFile(QFile *file, int *ret)
{
  QList<QByteArray*> *blockList = new QList<QByteArray*>();
  QDataStream data(file);
  char s[BLOCKSIZE];
  int readsize;
  *ret = 0;
  while ((readsize = data.readRawData(s, BLOCKSIZE)) > 0)
  {
    *ret += readsize;
    QByteArray *block = new QByteArray(s, readsize);
    blockList->append(block);
  }
  qDebug() << "Processed file. Size:" << *ret;
  return blockList;
}

QByteArray *FileShare::constructBlockListHash(QList<QByteArray*> *blockList)
{
  QByteArray *blockHash = new QByteArray();
  for (int i = 0; i < blockList->length(); i++)
  {
    QByteArray *block = blockList->at(i);
    QByteArray h = QCryptographicHash::hash(*block, QCryptographicHash::Sha1);
    blockHash->append(h);
    blocks->insert(h, block);
  }
  return blockHash;
}

void FileShare::requestButtonPressed(QString node, QString hash)
{
  waitingOnBlockList = true;
  QByteArray nba;
  nba.append(hash);
  qDebug() << "And the hash we're looking for is:" << hash;
  QByteArray hashBytes = QByteArray::fromHex(nba);

  requestBlock(node, hashBytes);
}

void FileShare::requestBlock(QString node, QByteArray hash)
{
  if (dialog->routingTable->contains(node))
  {
    QPair<QHostAddress, quint16> *address = dialog->routingTable->value(node)->first;
    QHostAddress ip = address->first;
    quint16 port = address->second;
    qDebug() << "Found the node!";
    sendBlockRequest(node, ip, port, hash);
  }
  else
  {
    qDebug() << "Didn't find the node...";
  }
}

void FileShare::receiveBlockList(QString from, QByteArray fileblocklist)
{
  waitingOnBlockList = false;
  qDebug() << "Getting a block list";
  blocksToRequest = new QList<QByteArray *>();
  int size = fileblocklist.size();
  if (size % 20 != 0 || size == 0)
  {
    qDebug() << "Oh that's weird, didn't get a proper-sized block list";
    // Then pass it to the normal funciton
  }
  int pos = 0;
  while (pos < size)
  {
    QByteArray *a = new QByteArray(fileblocklist.mid(pos, 20));
    blocksToRequest->append(a);
    pos += 20;
  }
  if (pos != size)
  {
    qDebug() << "Something wicked happened while receiving block list";
  }
  else
  {
    qDebug() << "Got the list! Length:" << blocksToRequest->length();
  }
  if (!blocksToRequest->isEmpty())
  {
    fileBeingConstructed = new QByteArray();
    constructingFile = true;
    requestBlock(from, *(blocksToRequest->takeFirst()));
  }
  else
    qDebug() << "LOL WUT";
}

void FileShare::writeConstructingFileToDisk()
{
  QStringList sl = waitingOnFileName.split("/");
  QString fn = sl.last();
  qDebug() << "Creating file";
  if (!QDir("downloads").exists())
  {
    qDebug() << "Making downloads folder";
    QDir().mkdir("downloads");
  }
  QFile file("downloads/" + fn);
  if (!file.open(QIODevice::WriteOnly))
  {
    qDebug() << "Could not open the file";
  }
  file.write(*fileBeingConstructed);
  file.close();
}

void FileShare::receiveBlock(QString from, QByteArray fileblock)
{
  qDebug() << "Got a block!";
  fileBeingConstructed->append(fileblock);
  if (!blocksToRequest->isEmpty())
  {
    requestBlock(from, *(blocksToRequest->takeFirst()));
  }
  else
  {
    resendRequestTimer->stop();
    writeConstructingFileToDisk();
  }
}

void FileShare::blockReply(QString org, QByteArray blockRequest)
{
  qDebug() << "Got a block request";
  if (blocks->contains(blockRequest))
  {
    qDebug() << "We have a block to respond with";
    QVariantMap response;
    response.insert("Dest", QVariant(org));
    response.insert("Origin", QVariant(dialog->originName));
    response.insert("BlockReply", QVariant(blockRequest));
    response.insert("Data", QVariant(*(blocks->value(blockRequest))));
    response.insert("HopLimit", QVariant(HOPLIMIT));
    if (dialog->routingTable->contains(org))
    {
      qDebug() << "Sending the response";
      QPair<QHostAddress, quint16> *address = dialog->routingTable->value(org)->first;
      QHostAddress ipaddress = address->first;
      quint16 port = address->second;
      dialog->sendVariantMap(ipaddress, port, &response);
    }
  }
  else
  {
    qDebug() << "We don't have a block to respond with";
  }
}

void FileShare::resendRequest()
{
  qDebug() << "Resending request";
  sendBlockRequest(destWaitingOn, ipWaitingOn, portWaitingOn, *blockWaitingOn);
}

void FileShare::sendBlockRequest(QString dest, QHostAddress ip, quint16 port, QByteArray hash)
{
  if (blockWaitingOn !=  NULL)
  {
    delete blockWaitingOn;
  }
  blockWaitingOn = new QByteArray(hash);
  destWaitingOn = dest;
  ipWaitingOn = ip;
  portWaitingOn = port;
  resendRequestTimer->start(2000);
  QVariantMap request;
  request.insert("Dest", QVariant(dest));
  request.insert("Origin", QVariant(dialog->originName));
  request.insert("HopLimit", QVariant(HOPLIMIT));
  request.insert("BlockRequest", QVariant(hash));
  dialog->sendVariantMap(ip, port, &request);
}

void FileShare::buttonPressed()
{
  QFileDialog dialog;
  QStringList files;
  QByteArray *blockHash;
  int filesize;
  dialog.setFileMode(QFileDialog::ExistingFiles);
  if (dialog.exec())
  {
    files = dialog.selectedFiles();
  }
  for (int i = 0; i < files.size(); i++)
  {
    QString filename = files.at(i);
    QFile file(filename);
    if(file.open(QIODevice::ReadOnly))
    {
      qDebug() << "Opened a file successfully!";
      QList<QByteArray*> *blockList = processFile(&file, &filesize);
      blockHash = constructBlockListHash(blockList);
      QByteArray *metaHash = new QByteArray(QCryptographicHash::hash(*blockHash, QCryptographicHash::Sha1));
      qDebug() << "Size of block list hash:" << blockHash->size();
      qDebug() << "Meta-hash:" << QString(metaHash->toHex());
      blocks->insert(*metaHash, blockHash);
      SharedFile *sf = new SharedFile(filename, metaHash, blockHash, blockList);
      sharedFiles->append(sf);
    }
    else
    {
      qDebug() << "Failed to open a file";
    }
  }
}
