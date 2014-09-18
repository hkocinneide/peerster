#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QKeyEvent>
#include <QDataStream>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QHostInfo>
#include <QTimer>
#include <QGroupBox>

QString stringifyHostPort(QHostAddress, quint16);
