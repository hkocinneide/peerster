#include "main.hh"

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

