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
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QCryptographicHash>

QString stringifyHostPort(QHostAddress, quint16);
