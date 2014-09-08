
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

NetSocket sock;

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
  textline->setMaximumHeight(40);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
  connect(&sock, SIGNAL(readyRead()), this, SLOT(gotReadyRead()));
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
  QString s = textline->toPlainText();
  QVariant variant = QVariant(s);
  QVariantMap *map = new QVariantMap();
  map->insert("ChatText", variant);

  QByteArray data;
  QDataStream *serializer = new QDataStream(&data, QIODevice::WriteOnly);

  *serializer << *map;

  delete serializer;

  for (int i = sock.myPortMin; i <= sock.myPortMax; i++)
  {
    sock.writeDatagram(data, QHostAddress(QHostAddress::LocalHost), i); 
  }


	// Clear the textline to get ready for the next input message.
	textline->clear();
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

    QDataStream *stream = new QDataStream(&datagram, QIODevice::ReadOnly);
    QVariantMap map;
    *stream >> map;

    textview->append(map.value("ChatText").toString());
  }
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
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	if (!sock.bind())
		exit(1);

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

