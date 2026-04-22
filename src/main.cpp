#include <iostream>
#include <filesystem>
#include <QApplication>
#include "mainwindow.h"
#include "server.h"
#include "client.h"
#include "launchwindow.h"

namespace fs = std::filesystem;


int main(int argc, char *argv[]) {
    std::cout << "file parser starting... " << std::endl;
    
    QApplication app(argc, argv);

    Server server;
    server.startListening(8080);
    // Client client;
    // client.connectToServer("127.0.0.1", 8080);
    LaunchWindow launch;
    launch.show();

    // MainWindow w("127.0.0.1", 8080);
    // w.show();

    return app.exec();
}