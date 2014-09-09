#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QKeyEvent>
#include <QDataStream>

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

class Peer : public QObject
{
  Q_OBJECT

public:
  Peer(QString name);
  Peer(QHostAddress ip, quint16 port);
  QHostAddress ipAddress;
  quint16 udpPortNumber;
};

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
  QHash<QString, Peer*> *neighbors;
  QString address;
  void processConnection(QString);

public slots:
	void gotReturnPressed();
  void gotReadyRead();
  void responseTimeout();
  void antiEntropy();
  void gotNewConnection();

private:
	QTextEdit *textview;
	TextEntryBox *textline;
  TextEntryBox *newConnection;
  QString originName;
  qint32 randNum;
  quint32 count;
  QHash<QString, QList<QVariantMap*>*> *seenMessages;
  QVariantMap *wantList;
  QTimer *timer;
  QTimer *antiEntropyTimer;
  QVariantMap *waitMsg;

  Peer *getRandomPeer();
  void processDatagram(QByteArray datagram, QHostAddress sender, quint16 senderPort);
  QString checkAddNeighbor(QHostAddress,quint16);
  void rumorMonger(QVariantMap*);
  void rumorMonger(QVariantMap*, Peer*);
  bool receiveMessage(QVariantMap*);
  void sendResponse(QString);
  void sendResponse(Peer*);
  void sendVariantMap(Peer*, QVariantMap*);
  QVariantMap *makeMessage(QString text, QString origin, quint32 count);
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
  int myPortMin, myPortMax;
  quint16 currentPort;
	NetSocket();

	// Bind this socket to a Peerster-specific default port.
	bool bind();
};

#endif // PEERSTER_MAIN_HH
