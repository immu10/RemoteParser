#include "server.h"


#include <QSslServer>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPointer>
#include <QUuid>
#include <private/qzipwriter_p.h>

static constexpr qint64 SendChunkSize = 256 * 1024;
static constexpr qint64 MaxQueuedBytes = 4 * 1024 * 1024;
static constexpr qint64 HashChunkSize = 1 * 1024 * 1024;

class HashingTempFile : public QTemporaryFile {
public:
    HashingTempFile(const QString &templ) : QTemporaryFile(templ) {
        setAutoRemove(false);
    }
    QByteArray hash() { return hasher.result(); }
protected:
    qint64 writeData(const char *data, qint64 len) override {
        hasher.addData(QByteArrayView(data, len));
        return QTemporaryFile::writeData(data, len);
    }
private:
    QCryptographicHash hasher{QCryptographicHash::Sha256};
};

struct ZipResult {
    QString tempPath;
    qint64 size = 0;
    QString hash;
    bool ok = false;
    QString error;
};

static void addDirToZip(QZipWriter &zip, const QString &dirPath, const QString &prefix) {
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    for (const QFileInfo &info : entries) {
        QString rel = prefix.isEmpty() ? info.fileName() : prefix + "/" + info.fileName();
        if (info.isDir()) {
            addDirToZip(zip, info.absoluteFilePath(), rel);
        } else {
            QFile f(info.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                zip.addFile(rel, f.readAll());
                f.close();
            }
        }
    }
}

class PendingHandshake : public QObject {
public:
    PendingHandshake(QSslSocket *s, Server *srv) : QObject(s), socket(s), server(srv) {
        readConn = connect(socket, &QSslSocket::readyRead, this, &PendingHandshake::onReadyRead);
    }
private:
    void onReadyRead() {
        buffer.append(socket->readAll());
        int newlineIdx = buffer.indexOf('\n');
        if (newlineIdx == -1) return;
        QString line = QString::fromUtf8(buffer.left(newlineIdx)).trimmed();
        QObject::disconnect(readConn);

        if (line == "HELLO:CONTROL") {
            server->createSessionWithControl(socket);
        } else if (line.startsWith("HELLO:DATA:")) {
            QString id = line.mid(11);
            server->attachDataSocket(socket, id);
        } else {
            qDebug() << "Bad handshake:" << line;
            socket->disconnectFromHost();
        }
        deleteLater();
    }
    QByteArray buffer;
    QSslSocket *socket;
    Server *server;
    QMetaObject::Connection readConn;
};


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
    QSslCertificate certificate(&certFile);
    certFile.close();

    QFile keyFile(basePath + "/server.key");
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open key file:" << keyFile.fileName();
        return;
    }
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
    QTcpSocket *tcpSocket = sslServer->nextPendingConnection();
    QSslSocket *socket = qobject_cast<QSslSocket*>(tcpSocket);
    if (!socket) {
        qDebug() << "Socket type:" << (tcpSocket ? tcpSocket->metaObject()->className() : "null");
        return;
    }

    connect(socket, &QSslSocket::sslErrors, this, [socket](const QList<QSslError> &errors) {
        qDebug() << "SSL errors on server:" << errors;
        socket->ignoreSslErrors();
    });

    emit clientConnected(socket->peerAddress().toString());
    qDebug() << "Client connected:" << socket->peerAddress().toString();

    new PendingHandshake(socket, this);
}

void Server::createSessionWithControl(QSslSocket *socket) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *session = new Session(socket, id, this);
    sessions.insert(id, session);
    socket->write(("SESSION:" + id + "\n").toUtf8());
    logActivity("Session opened: " + id);
}

void Server::attachDataSocket(QSslSocket *socket, const QString &id) {
    Session *session = sessions.value(id);
    if (!session) {
        socket->write("ERROR:Unknown session\n");
        socket->disconnectFromHost();
        return;
    }
    session->attachData(socket);
    socket->write("DATAREADY\n");
    logActivity("Data channel attached: " + id);
}

void Server::removeSession(const QString &id) {
    sessions.remove(id);
}

void Server::logActivity(const QString &msg) {
    emit activityLogged(msg);
}


Session::Session(QSslSocket *control, const QString &id, Server *server)
    : QObject(server), id(id), control(control), server(server) {
    control->setParent(this);
    connect(control, &QSslSocket::readyRead, this, &Session::onControlReadyRead);
    connect(control, &QSslSocket::disconnected, this, &Session::onSocketDisconnected);
}

Session::~Session() {}

void Session::attachData(QSslSocket *d) {
    data = d;
    data->setParent(this);
    connect(data, &QSslSocket::disconnected, this, &Session::onSocketDisconnected);
}

void Session::onSocketDisconnected() {
    destroySelf();
}

void Session::destroySelf() {
    if (dying) return;
    dying = true;
    server->removeSession(id);
    server->logActivity("Session ended: " + id);
    deleteLater();
}

void Session::onControlReadyRead() {
    QByteArray bytes = control->readAll();
    QString message = QString::fromUtf8(bytes);
    server->logActivity("Received: " + message);
    handleCommand(message);
}

QString Session::listDirectory(const QString &path) {
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
    if (response.isEmpty()) response = "\n";
    return response;
}

void Session::handleCommand(const QString &message) {
    if (message.startsWith("LIST:")) {
        QString path = message.mid(5);
        QString response = listDirectory(path);
        control->write(response.toUtf8());
        server->logActivity("Sent list: " + path);
    }
    else if (message.startsWith("DELETE:")) {
        QString path = message.mid(7);
        QDir dir;
        bool success = dir.remove(path);
        if (!success) success = dir.rmdir(path);
        control->write(success ? "OK:Deleted" : "ERROR:Delete failed");
    }
    else if (message.startsWith("RENAME:")) {
        QString paths = message.mid(7);
        QStringList parts = paths.split("|");
        bool success = QFile::rename(parts[0], parts[1]);
        control->write(success ? "OK:Renamed" : "ERROR:Rename failed");
    }
    else if (message.startsWith("MKDIR:")) {
        QString path = message.mid(6);
        QDir dir;
        bool success = dir.mkdir(path);
        control->write(success ? "OK:Folder created" : "ERROR:Failed to create folder");
        server->logActivity("mkdir: " + path);
    }
    else if (message == "DRIVES") {
        QFileInfoList drives = QDir::drives();
        QString response;
        for (const QFileInfo &drive : drives) {
            response += "DIR:" + drive.absolutePath() + "\n";
        }
        control->write(response.toUtf8());
    }
    else if (message.startsWith("DOWNLOAD:")) {
        QString path = message.mid(9).trimmed();
        if (!data) {
            control->write("ERROR:Data channel not ready");
            return;
        }
        enqueueDownload(path);
    }
}

void Session::enqueueDownload(const QString &path) {
    downloadQueue.enqueue(path);
    if (transferActive) {
        int position = downloadQueue.size();
        control->write(("INFO:Queued (#" + QString::number(position) + " in line)\n").toUtf8());
    } else {
        startNextDownload();
    }
}

void Session::startNextDownload() {
    if (dying) return;
    if (downloadQueue.isEmpty()) {
        transferActive = false;
        return;
    }

    QString path = downloadQueue.dequeue();
    QFileInfo info(path);
    if (!info.exists()) {
        control->write("ERROR:File not found");
        startNextDownload();
        return;
    }

    transferActive = true;

    if (info.isDir()) {
        control->write("INFO:Preparing folder...\n");
        server->logActivity("Preparing folder: " + path);

        QPointer<Session> guardedSession(this);
        QFuture<ZipResult> future = QtConcurrent::run([path]() -> ZipResult {
            ZipResult r;
            HashingTempFile hf(QDir::tempPath() + "/fileparser_XXXXXX.zip");
            if (!hf.open()) {
                r.error = "Cannot create temp file";
                return r;
            }
            r.tempPath = hf.fileName();
            {
                QZipWriter zip(&hf);
                addDirToZip(zip, path, QString());
                zip.close();
            }
            r.size = hf.size();
            r.hash = hf.hash().toHex();
            hf.close();
            r.ok = true;
            return r;
        });

        auto *watcher = new QFutureWatcher<ZipResult>(this);
        connect(watcher, &QFutureWatcher<ZipResult>::finished, this, [this, watcher, guardedSession, path]() {
            ZipResult r = watcher->result();
            watcher->deleteLater();

            if (!guardedSession || dying) {
                if (r.ok) QFile::remove(r.tempPath);
                return;
            }
            if (!data || data->state() != QAbstractSocket::ConnectedState) {
                if (r.ok) QFile::remove(r.tempPath);
                transferActive = false;
                startNextDownload();
                return;
            }
            if (!r.ok) {
                control->write(("ERROR:" + r.error).toUtf8());
                transferActive = false;
                startNextDownload();
                return;
            }

            auto *transfer = new FileTransfer(data, r.tempPath, true, this);
            connect(transfer, &FileTransfer::finished, this, &Session::onTransferFinished);
            if (!transfer->startWithKnownHash(r.size, r.hash)) {
                control->write("ERROR:Cannot stream temp file");
                delete transfer;
                QFile::remove(r.tempPath);
                transferActive = false;
                startNextDownload();
                return;
            }
            server->logActivity("Streaming folder: " + path);
        });
        watcher->setFuture(future);
        return;
    }

    auto *transfer = new FileTransfer(data, path, false, this);
    connect(transfer, &FileTransfer::finished, this, &Session::onTransferFinished);
    if (!transfer->start()) {
        control->write("ERROR:Cannot open file");
        delete transfer;
        transferActive = false;
        startNextDownload();
        return;
    }
    server->logActivity("Streaming file: " + path);
}

void Session::onTransferFinished() {
    if (dying) return;
    transferActive = false;
    startNextDownload();
}


FileTransfer::FileTransfer(QSslSocket *socket, const QString &sourcePath, bool removeAfter, QObject *parent)
    : QObject(parent), socket(socket), file(sourcePath), sourcePath(sourcePath), removeAfter(removeAfter) {}

bool FileTransfer::start() {
    if (!file.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        QByteArray chunk = file.read(HashChunkSize);
        if (chunk.isEmpty()) break;
        hasher.addData(chunk);
    }
    QString hash = hasher.result().toHex();
    totalSize = file.size();
    file.seek(0);

    QString header = "FILESIZE:" + QString::number(totalSize) + "|SHA256:" + hash + "\n";
    socket->write(header.toUtf8());

    connect(socket, &QSslSocket::bytesWritten, this, &FileTransfer::writeMore);
    connect(socket, &QSslSocket::disconnected, this, &FileTransfer::cleanup);
    writeMore();
    return true;
}

bool FileTransfer::startWithKnownHash(qint64 size, const QString &hash) {
    if (!file.open(QIODevice::ReadOnly)) return false;
    totalSize = size;

    QString header = "FILESIZE:" + QString::number(totalSize) + "|SHA256:" + hash + "\n";
    socket->write(header.toUtf8());

    connect(socket, &QSslSocket::bytesWritten, this, &FileTransfer::writeMore);
    connect(socket, &QSslSocket::disconnected, this, &FileTransfer::cleanup);
    writeMore();
    return true;
}

void FileTransfer::writeMore() {
    if (done) return;
    while (bytesSent < totalSize && socket->bytesToWrite() < MaxQueuedBytes) {
        QByteArray chunk = file.read(SendChunkSize);
        if (chunk.isEmpty()) break;
        qint64 written = socket->write(chunk);
        if (written < 0) { cleanup(); return; }
        bytesSent += written;
    }
    if (bytesSent >= totalSize) cleanup();
}

void FileTransfer::cleanup() {
    if (done) return;
    done = true;
    disconnect(socket, nullptr, this, nullptr);
    file.close();
    if (removeAfter) QFile::remove(sourcePath);
    emit finished();
    deleteLater();
}
