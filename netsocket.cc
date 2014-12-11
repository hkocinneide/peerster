#include <unistd.h>

#include "netsocket.hh"

// If we're in the Zoo, make sure we respect other peoples' ports
#define ZOO true

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four Peerster instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
  if (ZOO)
  {
    myPortMin = 32768 + (getuid() % 4096)*4;
    myPortMax = myPortMin + 3;
  }
  else
  {
    myPortMin = 32768;
    myPortMax = myPortMin + 32;
  }
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

