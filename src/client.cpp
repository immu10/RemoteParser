#include <client.h>


#include <QDebug>
#include <QCryptographicHash>
#include <QFile>

Client::Client(QObject *parent) : QObject(parent) {
    controlSocket = new QSslSocket(this);
    dataSocket = new QSslSocket(this);

    connect(controlSocket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        qDebug() << "SSL errors (control):" << errors;
        controlSocket->ignoreSslErrors();
    });
    connect(dataSocket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        qDebug() << "SSL errors (data):" << errors;
        dataSocket->ignoreSslErrors();
    });

    connect(controlSocket, &QSslSocket::encrypted, this, &Client::onControlEncrypted);
    connect(controlSocket, &QSslSocket::readyRead, this, &Client::onControlReadyRead);
    connect(dataSocket, &QSslSocket::encrypted, this, &Client::onDataEncrypted);
    connect(dataSocket, &QSslSocket::readyRead, this, &Client::onDataReadyRead);
}

void Client::connectToServer(const QString &h, int p) {
    host = h;
    port = p;
    state = ConnectingControl;
    controlSocket->connectToHostEncrypted(host, port);
}

void Client::onControlEncrypted() {
    qDebug() << "Control socket encrypted";
    state = AwaitingSession;
    controlSocket->write("HELLO:CONTROL\n");
}

void Client::onDataEncrypted() {
    qDebug() << "Data socket encrypted";
    state = AwaitingDataReady;
    dataSocket->write(("HELLO:DATA:" + sessionId + "\n").toUtf8());
}

void Client::sendRequest(const QString &request) {
    if (state != Ready) {
        qDebug() << "Not ready, dropping request:" << request;
        return;
    }
    controlSocket->write(request.toUtf8());
    emit requestSent();
}

void Client::queueDownload(const QString &remotePath, const QString &savePath) {
    pendingTransfers.enqueue({remotePath, savePath});
    emit transferQueued(remotePath, savePath);
    sendRequest("DOWNLOAD:" + remotePath);
}

void Client::onControlReadyRead() {
    QByteArray data = controlSocket->readAll();

    if (state == AwaitingSession) {
        int newlineIdx = data.indexOf('\n');
        if (newlineIdx == -1) return;
        QString line = QString::fromUtf8(data.left(newlineIdx)).trimmed();
        if (line.startsWith("SESSION:")) {
            sessionId = line.mid(8);
            state = ConnectingData;
            dataSocket->connectToHostEncrypted(host, port);
        } else {
            qDebug() << "Unexpected handshake response:" << line;
        }
        return;
    }

    if (state != Ready) return;

    if (data.startsWith("INFO:")) {
        int newlineIdx = data.indexOf('\n');
        if (newlineIdx == -1) {
            emit info(QString::fromUtf8(data.mid(5)));
            return;
        }
        emit info(QString::fromUtf8(data.mid(5, newlineIdx - 5)));
        data = data.mid(newlineIdx + 1);
        if (data.isEmpty()) return;
    }

    QString message = QString::fromUtf8(data);
    qDebug() << "Control response:" << message;

    if (message.startsWith("OK:")) {
        emit operationSuccess(message.mid(3));
    } else if (message.startsWith("ERROR:")) {
        QString reason = message.mid(6);
        emit operationError(reason);
        if (!receivingFile && !pendingTransfers.isEmpty()) {
            TransferItem failed = pendingTransfers.dequeue();
            emit transferFailed(failed.savePath, reason);
        }
    } else if (message.trimmed() == "EMPTY") {
        emit directoryListed({});
    } else {
        QStringList entries = message.split("\n", Qt::SkipEmptyParts);
        emit directoryListed(entries);
    }
}

void Client::onDataReadyRead() {
    QByteArray data = dataSocket->readAll();

    if (state == AwaitingDataReady) {
        int newlineIdx = data.indexOf('\n');
        if (newlineIdx == -1) return;
        QString line = QString::fromUtf8(data.left(newlineIdx)).trimmed();
        if (line == "DATAREADY") {
            state = Ready;
            emit ready();
            data = data.mid(newlineIdx + 1);
            if (data.isEmpty()) return;
        } else {
            qDebug() << "Unexpected data handshake response:" << line;
            return;
        }
    }

    if (state != Ready) return;

    if (!receivingFile && data.startsWith("FILESIZE:")) {
        int newlineIdx = data.indexOf('\n');
        if (newlineIdx == -1) {
            qDebug() << "Incomplete FILESIZE header";
            return;
        }
        if (pendingTransfers.isEmpty()) {
            qDebug() << "FILESIZE arrived with no pending transfer";
            return;
        }
        QString headerStr = QString::fromUtf8(data.left(newlineIdx));
        QStringList parts = headerStr.split("|");
        expectedFileSize = parts[0].mid(9).toLongLong();
        expectedHash = parts[1].mid(7);
        bytesReceived = 0;
        hasher.reset();

        currentTransfer = pendingTransfers.dequeue();
        if (outFile.isOpen()) outFile.close();
        outFile.setFileName(currentTransfer.savePath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            emit downloadError("Cannot open save file: " + outFile.errorString());
            emit transferFailed(currentTransfer.savePath, outFile.errorString());
            return;
        }
        receivingFile = true;
        emit transferStarted(currentTransfer.savePath);
        data = data.mid(newlineIdx + 1);
    }

    if (receivingFile) {
        qint64 remaining = expectedFileSize - bytesReceived;
        if (data.size() > remaining) data.truncate(remaining);

        if (!data.isEmpty()) {
            outFile.write(data);
            hasher.addData(data);
            bytesReceived += data.size();
            emit downloadProgress(bytesReceived, expectedFileSize);
        }

        if (bytesReceived >= expectedFileSize) {
            outFile.close();
            QString actualHash = hasher.result().toHex();
            QString savedPath = currentTransfer.savePath;
            QString wantedHash = expectedHash;
            receivingFile = false;
            expectedFileSize = 0;
            bytesReceived = 0;
            expectedHash.clear();
            currentTransfer = {};
            if (actualHash != wantedHash) {
                QFile::remove(savedPath);
                emit downloadError("Checksum mismatch — file corrupted");
                emit transferFailed(savedPath, "Checksum mismatch");
            } else {
                emit downloadComplete(savedPath);
            }
        }
    }
}
