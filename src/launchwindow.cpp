#include <launchwindow.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDebug>
#include <QSettings>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QIntValidator>

#include "clientwindow.h"
#include "server.h"
#include <serverwindow.h>

static const int kDefaultPort = 8080;

LaunchWindow::LaunchWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle("File Parser");
    setMinimumSize(380, 320);

    QSettings settings("fileParser", "fileParser");
    const QString lastHost = settings.value("client/host", "").toString();
    const int lastClientPort = settings.value("client/port", kDefaultPort).toInt();
    const int lastServerPort = settings.value("server/port", kDefaultPort).toInt();

    auto *portValidator = new QIntValidator(1, 65535, this);

    // --- Server section ---
    auto *serverBox = new QGroupBox("Host (this PC shares its files)", this);
    auto *serverForm = new QFormLayout();

    serverPortEdit = new QLineEdit(QString::number(lastServerPort));
    serverPortEdit->setValidator(portValidator);
    serverForm->addRow("Port:", serverPortEdit);

    serverButton = new QPushButton("Start Server", this);
    serverInfoLabel = new QLabel(this);
    serverInfoLabel->setWordWrap(true);
    serverInfoLabel->setStyleSheet("color: gray;");

    auto *serverLayout = new QVBoxLayout();
    serverLayout->addLayout(serverForm);
    serverLayout->addWidget(serverButton);
    serverLayout->addWidget(serverInfoLabel);
    serverBox->setLayout(serverLayout);

    // --- Client section ---
    auto *clientBox = new QGroupBox("Connect (reach another PC)", this);
    auto *clientForm = new QFormLayout();

    hostEdit = new QLineEdit(lastHost);
    hostEdit->setPlaceholderText("e.g. 192.168.1.42");
    clientPortEdit = new QLineEdit(QString::number(lastClientPort));
    clientPortEdit->setValidator(portValidator);

    clientForm->addRow("Host IP:", hostEdit);
    clientForm->addRow("Port:", clientPortEdit);

    clientButton = new QPushButton("Connect", this);

    auto *clientLayout = new QVBoxLayout();
    clientLayout->addLayout(clientForm);
    clientLayout->addWidget(clientButton);
    clientBox->setLayout(clientLayout);

    // --- Root ---
    auto *layout = new QVBoxLayout();
    layout->addWidget(serverBox);
    layout->addWidget(clientBox);
    setLayout(layout);

    connect(serverButton, &QPushButton::clicked, this, &LaunchWindow::onServerClicked);
    connect(clientButton, &QPushButton::clicked, this, &LaunchWindow::onClientClicked);
}

QString LaunchWindow::localAddresses() const {
    QStringList addrs;
    for (const QHostAddress &addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
            addrs << addr.toString();
    }
    return addrs.isEmpty() ? QStringLiteral("this machine's IP") : addrs.join(", ");
}

void LaunchWindow::onServerClicked() {
    const int port = serverPortEdit->text().toInt();

    QSettings settings("fileParser", "fileParser");
    settings.setValue("server/port", port);

    Server *server = new Server();
    server->startListening(port);

    const QString listenInfo =
        QString("Listening on  %1  (port %2)\nEnter this address on the other PC to connect.")
            .arg(localAddresses())
            .arg(port);

    ServerWindow *window = new ServerWindow(server, listenInfo);
    window->show();

    this->close();
}

void LaunchWindow::onClientClicked() {
    QString host = hostEdit->text().trimmed();
    if (host.isEmpty()) {
        serverInfoLabel->setText("Enter a host IP to connect to.");
        hostEdit->setFocus();
        return;
    }
    const int port = clientPortEdit->text().toInt();

    QSettings settings("fileParser", "fileParser");
    settings.setValue("client/host", host);
    settings.setValue("client/port", port);

    qDebug() << "Connecting to" << host << ":" << port;
    ClientWindow *w = new ClientWindow(host, port);
    w->show();
    this->close();
}
