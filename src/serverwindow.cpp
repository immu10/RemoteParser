#include "serverwindow.h"
#include <QVBoxLayout>
#include <QWidget>

ServerWindow::ServerWindow(Server *server, QWidget *parent) 
    : QMainWindow(parent), server(server) {
    setWindowTitle("File Parser - Server");
    setMinimumSize(600, 400);

    statusLabel = new QLabel("Listening on port 8080", this);
    logView = new QTextEdit(this);
    logView->setReadOnly(true);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(statusLabel);
    layout->addWidget(logView);

    QWidget *central = new QWidget(this);
    central->setLayout(layout);
    setCentralWidget(central);


    connect(server, &Server::clientConnected, this, &ServerWindow::onClientConnected);
    connect(server, &Server::activityLogged, this, &ServerWindow::onActivityLogged);
}

void ServerWindow::onClientConnected(const QString &address) {
    statusLabel->setText("Client connected: " + address);
}
void ServerWindow::onActivityLogged(const QString &message) {
    logView->append(message);
}