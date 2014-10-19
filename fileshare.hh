#include "qinclude.hh"

#define BLOCKSIZE 8000

class ChatDialog;

class SharedFile
{
public:
  QString filename;
  QByteArray *metahash;
  QByteArray *blocklist;
  QList<QByteArray*> *data;

  SharedFile(QString, QByteArray*, QByteArray*, QList<QByteArray*>*);
};

class FileShare : public QObject
{
  Q_OBJECT

public:
  QList<SharedFile*> *sharedFiles;
  QMap<QByteArray, QByteArray*> *blocks;
  bool waitingOnBlockList;
  QByteArray *blockWaitingOn;
  bool constructingFile;
  QString waitingOnFileName;

  FileShare(ChatDialog *dialog);
  void requestButtonPressed(QString node, QString hash);
  void blockReply(QString origin, QByteArray blockRequest);
  void sendBlockRequest(QString, QHostAddress, quint16, QByteArray);
  void receiveBlockList(QString, QByteArray);
  void receiveBlock(QString, QByteArray);
  void requestBlock(QString, QByteArray);

public slots:
  void buttonPressed();
  void resendRequest();

private:
  ChatDialog *dialog;
  QList<QByteArray*> *blocksToRequest;
  QByteArray *fileBeingConstructed;
  QString destWaitingOn;
  quint16 portWaitingOn;
  QHostAddress ipWaitingOn;
  QTimer *resendRequestTimer;

  void writeConstructingFileToDisk();
  QList<QByteArray*> *processFile(QFile*, int*);
  QByteArray *constructBlockListHash(QList<QByteArray*> *blocklist);
};
