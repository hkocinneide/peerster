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

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed();
  void gotReadyRead();

private:
	QTextEdit *textview;
	TextEntryBox *textline;
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

class Peer
{
  Q_OBJECT

public:
  QString dnsHostName;
  QHostAddress ipAddress;
  quint16 udpPortNumber;

}

#endif // PEERSTER_MAIN_HH
