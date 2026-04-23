#pragma once

#include <QObject>
#include <QSslSocket>
#include <QSslServer>
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
    QSslServer *sslServer;
};