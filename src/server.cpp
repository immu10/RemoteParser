#include "server.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>



Server::Server(QObject *parent) {
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &Server::onNewConnection);
    
}
void Server::startListening(int port){
        if (tcpServer->listen(QHostAddress::Any,port)){
            qDebug() << "Server is listening on port " << port;
        } else {
            qDebug() << "Failed to start server: " << tcpServer->errorString();
        }
    }   


void Server::onNewConnection() {
    QTcpSocket *socket = tcpServer->nextPendingConnection();
    emit clientConnected(socket->peerAddress().toString());
    qDebug() << "Client connected:" << socket->peerAddress().toString();

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QByteArray data = socket->readAll();
        QString message = QString::fromUtf8(data);
        qDebug() << "Received:" << message;
        emit activityLogged("Received request: " + message); 


        if (message.startsWith("LIST:")) {
            QString path = message.mid(5); // everything after "LIST:"
            QString response = listDirectory(path);
            socket->write(response.toUtf8());
            emit activityLogged("Sent: " + path);
        }
    });
}

QString Server::listDirectory(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        return "ERROR:Directory not found";
    }

    QStringList entries = dir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    QString response;

    for (const QString &entry : entries) {
        QFileInfo info(dir.absoluteFilePath(entry));
        if (info.isDir()) {
            response += "DIR:" + entry + "\n";
        } else {
            response += "FILE:" + entry + "\n";
        }
    }

    return response;
}