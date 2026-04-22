#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>

class Server : public QObject {
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);
    void startListening(int port);

private slots:
    void onNewConnection();

signals:
    void clientConnected(const QString &address);
    void activityLogged(const QString &message);

private:
    QString listDirectory(const QString &path);
    QTcpServer *tcpServer;
};