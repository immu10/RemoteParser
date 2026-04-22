#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include "server.h"

class ServerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ServerWindow(Server *server, QWidget *parent = nullptr);

private slots:
    void onClientConnected(const QString &address);
    void onActivityLogged(const QString &message);

private:
    QTextEdit *logView;
    QLabel *statusLabel;
    Server *server;
};