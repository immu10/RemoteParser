#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>



class ClientWindow;
class Server;

class LaunchWindow : public QWidget {
    Q_OBJECT

public:
    explicit LaunchWindow(QWidget *parent = nullptr);

private slots:
    void onServerClicked();
    void onClientClicked();

private:
    QString localAddresses() const;

    QPushButton *serverButton;
    QPushButton *clientButton;

    QLineEdit *serverPortEdit;
    QLineEdit *hostEdit;
    QLineEdit *clientPortEdit;
    QLabel *serverInfoLabel;
};
