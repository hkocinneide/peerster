#include "qinclude.hh"

class Peer;
class TextEntryBox;
class NetSocket;

extern NetSocket sock;

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
  QHash<QString, Peer*> *neighbors;
  QString address;

  static ChatDialog *dialog;
  void processConnection(QString);
  void rumorMonger(QVariantMap*);

public slots:
	void gotReturnPressed();
  void gotReadyRead();
  void antiEntropy();
  void routeRumor();
  void gotNewConnection();
  void peerActivated(QListWidgetItem *item);

private:
	QTextEdit *textview;
  QListWidget *peerlist;
  QDialog *privatechat;
	TextEntryBox *textline;
  TextEntryBox *newConnection;
  QString originName;
  qint32 randNum;
  quint32 count;
  QHash<QString, QList<QVariantMap*>*> *seenMessages;
  QHash<QString, QPair<QHostAddress, quint16>*> *routingTable;
  QVariantMap *wantList;
  QTimer *antiEntropyTimer;
  QTimer *routingTimer;

  Peer *getRandomPeer();
  void processDatagram(QByteArray datagram, QHostAddress sender, quint16 senderPort);
  QString checkAddNeighbor(QHostAddress,quint16);
  void rumorMonger(QVariantMap*, Peer*);
  bool receiveMessage(QVariantMap*);
  void sendResponse(QString);
  void sendResponse(Peer*);
  void sendVariantMap(Peer*, QVariantMap*);
  QVariantMap *makeMessage(QString text, QString origin, quint32 count);
  void updateRoutingTable(QString origin, QHostAddress sender, quint16 senderPort);
};

