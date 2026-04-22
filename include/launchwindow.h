#pragma once

#include <QWidget>
#include <QPushButton>



class MainWindow;
class Server;

class LaunchWindow : public QWidget {
    Q_OBJECT

public:
    explicit LaunchWindow(QWidget *parent = nullptr);

private slots:
    void onServerClicked();
    void onClientClicked();

private:
    QPushButton *serverButton;
    QPushButton *clientButton;
};