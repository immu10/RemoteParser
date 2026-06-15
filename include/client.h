#pragma once
#include <QObject>
#include <QsslSocket>
#include <QFile>
#include <QCryptographicHash>
#include <QQueue>

class Client : public QObject {
    Q_OBJECT

public:
    struct DownloadItem {
        QString remotePath;
        QString savePath;
    };
    struct UploadItem {
        QString localPath;
        QString destPath;
        qint64 size = 0;
        QString hash;
    };

    explicit Client(QObject *parent = nullptr);
    void connectToServer(const QString &host, int port);
    void sendRequest(const QString &request);
    void queueDownload(const QString &remotePath, const QString &savePath);
    bool queueUpload(const QString &localPath, const QString &destPath);
    void cancelDownload(const QString &remotePath);
    void cancelUpload(const QString &destPath);

private slots:
    void onControlEncrypted();
    void onDataEncrypted();
    void onControlReadyRead();
    void onDataReadyRead();
    void onDataBytesWritten(qint64);

private:
    enum State {
        Disconnected,
        ConnectingControl,
        AwaitingSession,
        ConnectingData,
        AwaitingDataReady,
        Ready
    };

    void handleControlMessage(const QByteArray &msg);
    void processDataChunk(QByteArray &data);
    void writeMoreUploadBytes();
    void finalizeDownload(bool ok, const QString &reason);

    QSslSocket *controlSocket;
    QSslSocket *dataSocket;
    QString host;
    int port = 0;
    QString sessionId;
    State state = Disconnected;

    // Control-channel framing
    QByteArray controlBuffer;
    QStringList listingEntries;
    bool collectingListing = false;

    // Downloads
    QQueue<DownloadItem> pendingDownloads;
    DownloadItem currentDownload;
    bool receivingFile = false;
    qint64 expectedFileSize = 0;
    qint64 bytesReceived = 0;
    QString expectedHash;
    QFile outFile;
    QCryptographicHash hasher{QCryptographicHash::Sha256};
    bool downloadDraining = false;
    qint64 downloadDrainTarget = 0;

    // Uploads
    QQueue<UploadItem> pendingUploads;
    UploadItem currentUpload;
    bool sendingFile = false;
    QFile inFile;
    qint64 uploadBytesSent = 0;

signals:
    void ready();
    void directoryListed(const QStringList &entries);
    void requestSent();
    void listingRequested();
    void operationSuccess(const QString &message);
    void operationError(const QString &message);
    void downloadComplete(const QString &savePath);
    void downloadError(const QString &message);
    void downloadProgress(qint64 received, qint64 total);
    void info(const QString &message);
    void transferQueued(const QString &remotePath, const QString &savePath);
    void transferStarted(const QString &savePath);
    void transferFailed(const QString &savePath, const QString &reason);

    void uploadQueued(const QString &localPath, const QString &destPath);
    void uploadStarted(const QString &destPath);
    void uploadProgress(const QString &destPath, qint64 sent, qint64 total);
    void uploadComplete(const QString &destPath);
    void uploadFailed(const QString &destPath, const QString &reason);
};
