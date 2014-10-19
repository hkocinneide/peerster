#include "qinclude.hh"

class PrivateDialog;
class Peer;
class TextEntryBox;
class NetSocket;
class FileShare;
class Searcher;

extern NetSocket sock;

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
  QHash<QString, Peer*> *neighbors;
  QString address;
  QHash<QString, PrivateDialog*> *privateChatTable;
  QHash<QString, QPair<QPair<QHostAddress, quint16>*,quint32>*> *routingTable;
  QString originName;
  bool noforward;

  static ChatDialog *dialog;
  void processConnection(QString);
  void rumorMonger(QVariantMap*);
  void sendVariantMap(QHostAddress, quint16, QVariantMap *);
  void sendVariantMap(Peer*, QVariantMap*);
  Peer *getRandomPeer();
  FileShare *fileshare;

public slots:
	void gotReturnPressed();
  void gotReadyRead();
  void antiEntropy();
  void routeRumor();
  void gotNewConnection();
  void peerActivated(QListWidgetItem *item);
  void requestButtonPressed();

private:
	QTextEdit *textview;
  QListWidget *peerlist;
  QListWidget *searchList;
  QDialog *privatechat;
	TextEntryBox *textline;
  TextEntryBox *newConnection;
  TextEntryBox *blockRequestNode;
  TextEntryBox *blockRequestHash;
  TextEntryBox *searchBox;
  QPushButton *blockRequestButton;
  qint32 randNum;
  quint32 count;
  QHash<QString, QList<QVariantMap*>*> *seenMessages;
  QDialog *activeDialog;
  QVariantMap *wantList;
  Searcher *searcher;
  QTimer *antiEntropyTimer;
  QTimer *routingTimer;

  void processDatagram(QByteArray datagram, QHostAddress sender, quint16 senderPort);
  QString checkAddNeighbor(QHostAddress,quint16);
  void rumorMonger(QVariantMap*, Peer*);
  void rumorMongerAllPeers(QVariantMap*);
  bool receiveMessage(QVariantMap*);
  void sendResponse(QString);
  void sendResponse(Peer*);
  QVariantMap *makeMessage(QString text, QString origin, quint32 count);
  void updateRoutingTable(QString origin, QHostAddress sender, quint16 senderPort, quint32 seqNo, bool direct);
  QGridLayout *makeNewLayout();
};

