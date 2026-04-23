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
    
    sendRequest("LIST:C:/Users");
}

void Client::sendRequest(const QString &request) {
    // QString message = "LIST:" + path;
    socket->write(request.toUtf8());
    emit requestSent();
}

void Client::onReadyRead() {
    QByteArray data = socket->readAll();
    QString message = QString::fromUtf8(data);
    qDebug() << "Raw response:" << message;

    if (message.startsWith("OK:")) {
        emit operationSuccess(message.mid(3));
    } else if (message.startsWith("ERROR:")) {
        emit operationError(message.mid(6));
    } else {
        QStringList entries = message.split("\n", Qt::SkipEmptyParts);
        emit directoryListed(entries);
    }
    
    
}