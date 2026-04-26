#pragma once
#include <QObject>
#include <QsslSocket>
#include <QFile>
#include <QCryptographicHash>
#include <QQueue>

class Client : public QObject {
    Q_OBJECT

public:
    struct TransferItem {
        QString remotePath;
        QString savePath;
    };

    explicit Client(QObject *parent = nullptr);
    void connectToServer(const QString &host, int port);
    void sendRequest(const QString &request);
    void queueDownload(const QString &remotePath, const QString &savePath);

private slots:
    void onControlEncrypted();
    void onDataEncrypted();
    void onControlReadyRead();
    void onDataReadyRead();

private:
    enum State {
        Disconnected,
        ConnectingControl,
        AwaitingSession,
        ConnectingData,
        AwaitingDataReady,
        Ready
    };

    QSslSocket *controlSocket;
    QSslSocket *dataSocket;
    QString host;
    int port = 0;
    QString sessionId;
    State state = Disconnected;

    QQueue<TransferItem> pendingTransfers;
    TransferItem currentTransfer;
    bool receivingFile = false;
    qint64 expectedFileSize = 0;
    qint64 bytesReceived = 0;
    QString expectedHash;
    QFile outFile;
    QCryptographicHash hasher{QCryptographicHash::Sha256};

signals:
    void ready();
    void directoryListed(const QStringList &entries);
    void requestSent();
    void operationSuccess(const QString &message);
    void operationError(const QString &message);
    void downloadComplete(const QString &savePath);
    void downloadError(const QString &message);
    void downloadProgress(qint64 received, qint64 total);
    void info(const QString &message);
    void transferQueued(const QString &remotePath, const QString &savePath);
    void transferStarted(const QString &savePath);
    void transferFailed(const QString &savePath, const QString &reason);
};
