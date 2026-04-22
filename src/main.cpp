#include <iostream>
#include <filesystem>
#include <QApplication>
#include "mainwindow.h"

namespace fs = std::filesystem;


int main(int argc, char *argv[]){


    std::cout<<"file parser starting... " << std::endl;
    // fs::path dir = ".";
    // for (const auto& entry : fs::directory_iterator(dir)){
    //     std::cout << entry.path() << std::endl;
    // }
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
    
    return 0;
}