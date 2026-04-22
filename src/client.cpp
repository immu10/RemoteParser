#include <client.h>
#include <QDebug>

Client::Client(QObject *parent) : QObject(parent) {
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(socket, &QTcpSocket::readyRead, this, &Client::onReadyRead);
}
void Client::connectToServer(const QString &host, int port) {
    socket->connectToHost(host, port);
}

void Client::onConnected() {
    qDebug() << "Connected to server!";
    sendRequest("C:/Users");
}

void Client::sendRequest(const QString &path) {
    QString message = "LIST:" + path;
    socket->write(message.toUtf8());
    emit requestSent();
}

void Client::onReadyRead() {
    QByteArray data = socket->readAll();
    QString message = QString::fromUtf8(data);
    
    QStringList entries = message.split("\n", Qt::SkipEmptyParts);
    emit directoryListed(entries);
}