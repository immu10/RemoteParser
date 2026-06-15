#include <client.h>


#include <QDebug>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

static constexpr qint64 UploadChunkSize = 256 * 1024;
static constexpr qint64 UploadMaxQueued = 4 * 1024 * 1024;
static constexpr qint64 HashChunkSize = 1 * 1024 * 1024;

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
    connect(dataSocket, &QSslSocket::bytesWritten, this, &Client::onDataBytesWritten);

    connect(controlSocket, &QSslSocket::disconnected, this, [this]() {
        handleDisconnect("The connection was closed.");
    });
    connect(controlSocket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        handleDisconnect(controlSocket->errorString());
    });
    connect(dataSocket, &QSslSocket::disconnected, this, [this]() {
        handleDisconnect("The connection was closed.");
    });
}

void Client::connectToServer(const QString &h, int p) {
    host = h;
    port = p;
    state = ConnectingControl;
    controlSocket->connectToHostEncrypted(host, port);
}

void Client::handleDisconnect(const QString &reason) {
    if (state == Disconnected) return;   // report the loss only once
    state = Disconnected;
    emit connectionLost(reason);
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
    if (request.startsWith("LIST:") || request == "DRIVES")
        emit listingRequested();
}

void Client::queueDownload(const QString &remotePath, const QString &savePath) {
    pendingDownloads.enqueue({remotePath, savePath});
    emit transferQueued(remotePath, savePath);
    sendRequest("DOWNLOAD:" + remotePath);
}

bool Client::queueUpload(const QString &localPath, const QString &destPath) {
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash h(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        QByteArray chunk = f.read(HashChunkSize);
        if (chunk.isEmpty()) break;
        h.addData(chunk);
    }
    UploadItem item;
    item.localPath = localPath;
    item.destPath = destPath;
    item.size = f.size();
    item.hash = h.result().toHex();
    f.close();

    pendingUploads.enqueue(item);
    emit uploadQueued(localPath, destPath);

    QString cmd = "UPLOAD:" + destPath + "|FILESIZE:" + QString::number(item.size) + "|SHA256:" + item.hash;
    sendRequest(cmd);
    return true;
}

void Client::cancelDownload(const QString &remotePath) {
    sendRequest("CANCEL:DOWNLOAD:" + remotePath);
}

void Client::cancelUpload(const QString &destPath) {
    qint64 sent = (sendingFile && currentUpload.destPath == destPath) ? uploadBytesSent : 0;
    QString cmd = "CANCEL:UPLOAD:" + destPath + "|" + QString::number(sent);
    sendRequest(cmd);
    if (sendingFile && currentUpload.destPath == destPath) {
        sendingFile = false;
        if (inFile.isOpen()) inFile.close();
        emit uploadFailed(destPath, "Cancelled");
    } else {
        for (int i = 0; i < pendingUploads.size(); ++i) {
            if (pendingUploads.at(i).destPath == destPath) {
                pendingUploads.removeAt(i);
                emit uploadFailed(destPath, "Cancelled");
                return;
            }
        }
    }
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

    // Accumulate, then consume only complete (newline-terminated) lines so a
    // message split across TCP segments isn't truncated.
    controlBuffer += data;
    int newlineIdx;
    while ((newlineIdx = controlBuffer.indexOf('\n')) != -1) {
        QByteArray msgBytes = controlBuffer.left(newlineIdx);
        controlBuffer = controlBuffer.mid(newlineIdx + 1);
        if (msgBytes.isEmpty()) continue;
        handleControlMessage(msgBytes);
    }
}

void Client::handleControlMessage(const QByteArray &msgBytes) {
    QString message = QString::fromUtf8(msgBytes);
    qDebug() << "Control:" << message;

    if (message.startsWith("INFO:")) {
        emit info(message.mid(5));
        return;
    }
    if (message.startsWith("UPLOADREADY:")) {
        QString destPath = message.mid(12);
        if (pendingUploads.isEmpty()) {
            qDebug() << "UPLOADREADY with no pending upload";
            return;
        }
        currentUpload = pendingUploads.dequeue();
        if (inFile.isOpen()) inFile.close();
        inFile.setFileName(currentUpload.localPath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            emit uploadFailed(currentUpload.destPath, "Cannot open local: " + inFile.errorString());
            return;
        }
        sendingFile = true;
        uploadBytesSent = 0;
        emit uploadStarted(currentUpload.destPath);
        writeMoreUploadBytes();
        return;
    }
    if (message.startsWith("CANCELLED:DOWNLOAD:")) {
        QString rest = message.mid(19);
        int colonIdx = rest.lastIndexOf(':');
        if (colonIdx == -1) return;
        QString remotePath = rest.left(colonIdx);
        qint64 serverSent = rest.mid(colonIdx + 1).toLongLong();

        if (receivingFile && currentDownload.remotePath == remotePath) {
            downloadDraining = true;
            downloadDrainTarget = serverSent;
            if (bytesReceived >= downloadDrainTarget) {
                finalizeDownload(false, "Cancelled");
            }
            return;
        }
        for (int i = 0; i < pendingDownloads.size(); ++i) {
            if (pendingDownloads.at(i).remotePath == remotePath) {
                QString savePath = pendingDownloads.at(i).savePath;
                pendingDownloads.removeAt(i);
                emit transferFailed(savePath, "Cancelled");
                return;
            }
        }
        return;
    }
    if (message.startsWith("CANCELLED:UPLOAD:")) {
        QString destPath = message.mid(17);
        emit uploadFailed(destPath, "Cancelled");
        return;
    }
    if (message.startsWith("OK:Uploaded:")) {
        QString destPath = message.mid(12);
        emit uploadComplete(destPath);
        return;
    }
    if (message.startsWith("OK:")) {
        emit operationSuccess(message.mid(3));
        return;
    }
    if (message.startsWith("ERROR:")) {
        QString reason = message.mid(6);
        emit operationError(reason);
        if (!receivingFile && !pendingDownloads.isEmpty()) {
            DownloadItem failed = pendingDownloads.dequeue();
            emit transferFailed(failed.savePath, reason);
        }
        return;
    }
    if (message.startsWith("LIST:BEGIN:")) {
        collectingListing = true;
        currentListingPath = message.mid(11);   // path this listing answers ("" = drives)
        listingEntries.clear();
        return;
    }
    if (message == "LIST:END") {
        collectingListing = false;
        emit directoryListed(currentListingPath, listingEntries);
        listingEntries.clear();
        return;
    }
    if (collectingListing) {
        listingEntries << message;   // "DIR:<name>" or "FILE:<name>"
        return;
    }
    qDebug() << "Unhandled control message:" << message;
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
        } else {
            qDebug() << "Unexpected data handshake:" << line;
            return;
        }
    }

    if (state != Ready) return;
    processDataChunk(data);
}

void Client::processDataChunk(QByteArray &data) {
    while (!data.isEmpty()) {
        if (!receivingFile) {
            if (!data.startsWith("FILESIZE:")) {
                return;
            }
            int newlineIdx = data.indexOf('\n');
            if (newlineIdx == -1) return;
            if (pendingDownloads.isEmpty()) {
                qDebug() << "FILESIZE arrived with no pending download";
                return;
            }
            QString headerStr = QString::fromUtf8(data.left(newlineIdx));
            QStringList parts = headerStr.split("|");
            expectedFileSize = parts[0].mid(9).toLongLong();
            expectedHash = parts[1].mid(7);
            bytesReceived = 0;
            hasher.reset();
            downloadDraining = false;
            downloadDrainTarget = 0;

            currentDownload = pendingDownloads.dequeue();
            if (outFile.isOpen()) outFile.close();
            outFile.setFileName(currentDownload.savePath);
            if (!outFile.open(QIODevice::WriteOnly)) {
                emit downloadError("Cannot open save file: " + outFile.errorString());
                emit transferFailed(currentDownload.savePath, outFile.errorString());
                return;
            }
            receivingFile = true;
            emit transferStarted(currentDownload.savePath);
            data = data.mid(newlineIdx + 1);
        }

        qint64 effectiveTotal = downloadDraining ? downloadDrainTarget : expectedFileSize;
        qint64 remaining = effectiveTotal - bytesReceived;
        if (remaining <= 0) {
            finalizeDownload(!downloadDraining, downloadDraining ? QString("Cancelled") : QString());
            continue;
        }
        QByteArray chunk = data.left(remaining);
        data = data.mid(chunk.size());
        if (!downloadDraining) {
            outFile.write(chunk);
            hasher.addData(chunk);
        }
        bytesReceived += chunk.size();
        if (!downloadDraining) {
            emit downloadProgress(bytesReceived, expectedFileSize);
        }
        if (bytesReceived >= effectiveTotal) {
            finalizeDownload(!downloadDraining, downloadDraining ? QString("Cancelled") : QString());
        }
    }
}

void Client::finalizeDownload(bool ok, const QString &reason) {
    outFile.close();
    QString savedPath = currentDownload.savePath;
    QString actualHash = hasher.result().toHex();
    QString wantedHash = expectedHash;

    receivingFile = false;
    expectedFileSize = 0;
    bytesReceived = 0;
    expectedHash.clear();
    currentDownload = {};
    downloadDraining = false;
    downloadDrainTarget = 0;

    if (!ok) {
        QFile::remove(savedPath);
        emit downloadError(reason);
        emit transferFailed(savedPath, reason);
        return;
    }
    if (actualHash != wantedHash) {
        QFile::remove(savedPath);
        emit downloadError("Checksum mismatch — file corrupted");
        emit transferFailed(savedPath, "Checksum mismatch");
        return;
    }
    emit downloadComplete(savedPath);
}

void Client::writeMoreUploadBytes() {
    if (!sendingFile) return;
    while (uploadBytesSent < currentUpload.size && dataSocket->bytesToWrite() < UploadMaxQueued) {
        QByteArray chunk = inFile.read(UploadChunkSize);
        if (chunk.isEmpty()) break;
        qint64 written = dataSocket->write(chunk);
        if (written < 0) {
            sendingFile = false;
            inFile.close();
            emit uploadFailed(currentUpload.destPath, "Write failed");
            return;
        }
        uploadBytesSent += written;
        emit uploadProgress(currentUpload.destPath, uploadBytesSent, currentUpload.size);
    }
    if (uploadBytesSent >= currentUpload.size) {
        sendingFile = false;
        inFile.close();
        // server will send OK:Uploaded:<path> on control when it finishes verifying
    }
}

void Client::onDataBytesWritten(qint64) {
    writeMoreUploadBytes();
}
