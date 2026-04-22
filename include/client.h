#pragma once
#include <QObject>
#include <QTcpSocket>

class Client : public QObject {
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);
    void connectToServer(const QString &host, int port);
    void sendRequest(const QString &request);
private slots:
    void onConnected();
    void onReadyRead();
private:
    QTcpSocket *socket;
signals:
void directoryListed(const QStringList &entries);
void requestSent();
};