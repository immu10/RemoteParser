#pragma once

#include <QObject>
#include <QSslSocket>
#include <QSslServer>
#include <QDir>
#include <QFile>
#include <QQueue>
#include <QHash>
#include <QCryptographicHash>

class Session;
class FileTransfer;

class Server : public QObject {
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);
    void startListening(int port);

    void createSessionWithControl(QSslSocket *socket);
    void attachDataSocket(QSslSocket *socket, const QString &id);
    void removeSession(const QString &id);
    void logActivity(const QString &msg);

signals:
    void clientConnected(const QString &address);
    void activityLogged(const QString &message);

private slots:
    void onNewConnection();

private:
    QSslServer *sslServer;
    QHash<QString, Session*> sessions;
};

struct PendingUpload {
    QString destPath;
    qint64 expectedSize;
    QString expectedHash;
};

class Session : public QObject {
    Q_OBJECT

public:
    Session(QSslSocket *control, const QString &id, Server *server);
    ~Session();
    void attachData(QSslSocket *data);
    QString sessionId() const { return id; }

private slots:
    void onControlReadyRead();
    void onDataReadyRead();
    void onSocketDisconnected();
    void onTransferFinished();

private:
    void handleCommand(const QString &message);
    QString listDirectory(const QString &path);

    void enqueueDownload(const QString &path);
    void startNextDownload();
    void cancelDownload(const QString &path);

    void enqueueUpload(const PendingUpload &up);
    void startNextUpload();
    void cancelUpload(const QString &destPath, qint64 clientBytesSent);
    void finishCurrentUpload(bool ok, const QString &reason);

    void destroySelf();

    QString id;
    QSslSocket *control;
    QSslSocket *data = nullptr;
    Server *server;

    QQueue<QString> downloadQueue;
    bool transferActive = false;
    FileTransfer *activeTransfer = nullptr;
    QString currentRemotePath;

    QQueue<PendingUpload> uploadQueue;
    bool uploadActive = false;
    PendingUpload currentUpload;
    QFile uploadFile;
    QCryptographicHash uploadHasher{QCryptographicHash::Sha256};
    qint64 uploadBytesReceived = 0;
    bool uploadDraining = false;
    qint64 uploadDrainTarget = 0;

    bool dying = false;
};

class FileTransfer : public QObject {
    Q_OBJECT

public:
    FileTransfer(QSslSocket *socket, const QString &sourcePath, bool removeAfter, QObject *parent = nullptr);
    bool start();
    bool startWithKnownHash(qint64 size, const QString &hash);
    void abort();
    qint64 bytesSentSoFar() const { return bytesSent; }

signals:
    void finished();

private slots:
    void writeMore();
    void cleanup();

private:
    QSslSocket *socket;
    QFile file;
    QString sourcePath;
    bool removeAfter;
    bool done = false;
    qint64 totalSize = 0;
    qint64 bytesSent = 0;
};
