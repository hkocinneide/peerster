/* Currently interviewing in SF so apologies for the barebones
 * repository. This is obviously a framework for peerster, but
 * I plan on using the same basic qt features to get my chord
 * implementation running.
 *
 * -Hugh
 */

#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("Chord");

	textview = new QTextEdit(this);
	textview->setReadOnly(true);
	textline = new QLineEdit(this);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
}

void ChatDialog::gotReturnPressed()
{
	textview->append(textline->text());

	textline->clear();
}

NetSocket::NetSocket()
{
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}

bool NetSocket::bind()
{
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

int main(int argc, char **argv)
{
	QApplication app(argc,argv);

	ChatDialog dialog;
	dialog.show();

	NetSocket sock;
	if (!sock.bind())
		exit(1);

	return app.exec();
}

