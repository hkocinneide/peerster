#include "qinclude.hh"

class Peer : public QObject
{
  Q_OBJECT

public:
  // Fields
  QHostAddress ipAddress;
  quint16 udpPortNumber;
  QTimer *timer;
  QVariantMap *waitMsg;
  // Constructors
  Peer(QString name);
  Peer(QHostAddress ip, quint16 port);

public slots:
  void responseTimeout();
};
