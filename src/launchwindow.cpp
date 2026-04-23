#include <launchwindow.h>
#include <QVBoxLayout>
#include <QDebug>

#include "clientwindow.h"
#include "server.h"
#include <serverwindow.h>

const QString serverIP = "127.0.0.1";
const int serverPort = 8080;


LaunchWindow::LaunchWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle("File Parser");
    setMinimumSize(300, 200);

    serverButton = new QPushButton("Server", this);
    clientButton = new QPushButton("Client", this);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(serverButton);
    layout->addWidget(clientButton);
    setLayout(layout);

    connect(serverButton, &QPushButton::clicked, this, &LaunchWindow::onServerClicked);
    connect(clientButton, &QPushButton::clicked, this, &LaunchWindow::onClientClicked);
}

void LaunchWindow::onServerClicked() {
    Server *server = new Server();
    server->startListening(serverPort);
    ServerWindow *window = new ServerWindow(server);
    window->show();

    this->close();
}
void LaunchWindow::onClientClicked() {
    qDebug() << "Connecting to" << serverIP << ":" << serverPort;
    ClientWindow *w = new ClientWindow(serverIP, serverPort);
    w->show();
    this->close();
}