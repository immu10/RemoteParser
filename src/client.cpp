#include <client.h>
#include <QDebug>

Client::Client(QObject *parent) : QObject(parent) {
    socket = new QSslSocket(this);
    connect(socket, &QSslSocket::connected, this, &Client::onConnected);
    connect(socket, &QSslSocket::readyRead, this, &Client::onReadyRead);
    connect(socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
    qDebug() << "SSL errors:" << errors;
    socket->ignoreSslErrors();
});

}
void Client::connectToServer(const QString &host, int port) {
    socket->connectToHostEncrypted(host, port);
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