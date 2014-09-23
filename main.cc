#include <unistd.h>
#include <stdlib.h>

#include "main.hh"
#include "peer.hh"
#include "chatdialog.hh"
#include "textentrybox.hh"
#include "privatedialog.hh"

QString stringifyHostPort(QHostAddress ipAddress, quint16 udpPort)
{
  return ipAddress.toString() + "@" + QString::number(udpPort);
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
  ChatDialog::dialog = new ChatDialog();
  ChatDialog::dialog->show();

	// Create a UDP network socket
	if (!sock.bind())
		exit(1);

  // Letting our app know where it is
  ChatDialog::dialog->address = stringifyHostPort(QHostAddress(QHostAddress::LocalHost), sock.currentPort);

  // Add our neighbors
  ChatDialog::dialog->neighbors = new QHash<QString, Peer*>();
  QHostAddress localHost = QHostAddress(QHostAddress::LocalHost);
  for (quint16 i = sock.myPortMin; i <= sock.myPortMax; i++)
  {
    if (i == sock.currentPort)
      continue;
    quint16 neighborPort = i;
    Peer *p = new Peer(localHost, neighborPort);
    ChatDialog::dialog->neighbors->insert(stringifyHostPort(localHost, neighborPort), p);
  }

  // Set noforward
  ChatDialog::dialog->noforward = false;
  
  // Add neighbors from the command line
  QStringList args = QCoreApplication::arguments();
  for(int i = 1; i < args.length(); i++)
  {
    if (args.at(i) == "-noforward")
    {
      qDebug () << "Making this a noforward node";
      ChatDialog::dialog->noforward = true;
    }
    else // We have a connection
    {
      ChatDialog::dialog->processConnection(args.at(i));
    }
  }

  // Have the new ChatDialog send a "Hey! I'm here!" message
  
  ChatDialog::dialog->routeRumor();

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

