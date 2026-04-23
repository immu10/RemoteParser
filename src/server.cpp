#include "server.h"


#include <QSslServer>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QCoreApplication>






Server::Server(QObject *parent): QObject(parent) {
    sslServer = new QSslServer(this);
    connect(sslServer, &QSslServer::pendingConnectionAvailable, this, &Server::onNewConnection);
    
}

void Server::startListening(int port) {
    QSslConfiguration sslConfig;
    QString basePath = QCoreApplication::applicationDirPath();


    QFile certFile(basePath + "/server.crt");
    if (!certFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open cert file:" << certFile.fileName();
        return;
    }

    // certFile.open(QIODevice::ReadOnly);  
    QSslCertificate certificate(&certFile);
    certFile.close();

    
    QFile keyFile(basePath + "/server.key");
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open key file:" << keyFile.fileName();
        return;
    }

    // keyFile.open(QIODevice::ReadOnly);
    QSslKey privateKey(&keyFile, QSsl::Rsa);
    keyFile.close();
    
    sslConfig.setLocalCertificate(certificate);
    sslConfig.setPrivateKey(privateKey);
    
    sslServer->setSslConfiguration(sslConfig);
    
    if (sslServer->listen(QHostAddress::Any, port)) {
        qDebug() << "Server is listening on port" << port;
    } else {
        qDebug() << "Failed to start server";
    }

}


void Server::onNewConnection() {
    // QSslSocket *socket = qobject_cast<QSslSocket*>(sslServer->nextPendingConnection());

    //testing
    QTcpSocket *tcpSocket = sslServer->nextPendingConnection();
    QSslSocket *socket = qobject_cast<QSslSocket*>(tcpSocket);
    if (!socket) {
        qDebug() << "Socket type:" << tcpSocket->metaObject()->className();
        return;
    }

    if (!socket) {
        qDebug() << "Failed to get socket";
        return;
    }
    connect(socket, &QSslSocket::sslErrors, this, [socket](const QList<QSslError> &errors) {
        qDebug() << "SSL errors on server:" << errors;
        socket->ignoreSslErrors();
    });



    emit clientConnected(socket->peerAddress().toString());
    qDebug() << "Client connected:" << socket->peerAddress().toString();

    connect(socket, &QSslSocket::readyRead, this, [this, socket]() {
        QByteArray data = socket->readAll();
        QString message = QString::fromUtf8(data);
        qDebug() << "Received:" << message;
        emit activityLogged("Received request: " + message); 
        handleRequest(socket, message);

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

void Server::handleRequest(QSslSocket *socket, const QString &message) {

    if (message.startsWith("LIST:")) {
            QString path = message.mid(5); 
            qDebug() << "Listing path:" << path;
            QString response = listDirectory(path);
            socket->write(response.toUtf8());
            emit activityLogged("Sent: " + path);
        }
    else if (message.startsWith("DELETE:")) {
        QString path = message.mid(7);
        QDir dir;
        bool success = dir.remove(path);  // for files
        if (!success) success = dir.rmdir(path);  // for empty dirs
        socket->write(success ? "OK:Deleted" : "ERROR:Delete failed");
        }
    
    else if (message.startsWith("RENAME:")) {
        QString paths = message.mid(7);
        QStringList parts = paths.split("|");
        QString oldPath = parts[0];
        QString newPath = parts[1];
        bool success = QFile::rename(oldPath, newPath);
        socket->write(success ? "OK:Renamed" : "ERROR:Rename failed");
    }
    else if (message.startsWith("MKDIR:")) {
        QString path = message.mid(6);
        QDir dir;
        bool success = dir.mkdir(path);
        socket->write(success ? "OK:Folder created" : "ERROR:Failed to create folder");
        emit activityLogged("mkdir: " + path);
    }
    else if (message == "DRIVES") {
        QFileInfoList drives = QDir::drives();
        QString response;
        for (const QFileInfo &drive : drives) {
            response += "DIR:" + drive.absolutePath() + "\n";
        }
        socket->write(response.toUtf8());
    }
}