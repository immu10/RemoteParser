#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QWidget>
#include <QStatusBar>
#include <QDebug>
#include <QFileIconProvider>
#include <QStandardItemModel>

MainWindow::MainWindow(const QString &serverIP, int serverPort, QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle("File Parser");
    setMinimumSize(900, 600);
    

    // --- Models ---
    treeModel = new QFileSystemModel(this);
    treeModel->setRootPath("");
    treeModel->setFilter(QDir::NoDotAndDotDot | QDir::Dirs);

    listModel = new QStandardItemModel(this); // server data

    // --- Tree View (left) ---
    treeView = new QTreeView(this);
    treeView->setModel(treeModel);
    treeView->setRootIndex(treeModel->index(""));
    treeView->hideColumn(1); // size
    treeView->hideColumn(2); // type
    treeView->hideColumn(3); // date
    treeView->setMinimumWidth(250);

    // --- List View (right) ---
    listView = new QListView(this);
    listView->setModel(listModel);
    listView->setEditTriggers(QAbstractItemView::NoEditTriggers); 

    // --- Path bar + browse button ---
    pathBar = new QLineEdit(this);
    pathBar->setReadOnly(true);
    pathBar->setPlaceholderText("Select a folder...");
    
    browseButton = new QPushButton("Browse", this);
    backButton = new QPushButton("←", this);

    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->addWidget(backButton);
    topBar->addWidget(pathBar);
    topBar->addWidget(browseButton);

    // --- Splitter ---
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(treeView);
    splitter->addWidget(listView);
    splitter->setSizes({250, 650});

    // --- Main layout ---
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(topBar);
    mainLayout->addWidget(splitter);

    QWidget *central = new QWidget(this);
    central->setLayout(mainLayout);
    setCentralWidget(central);

    // --- Connections ---
    connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,this, &MainWindow::onDirectorySelected);
    connect(browseButton, &QPushButton::clicked,this, &MainWindow::onBrowseClicked);
    connect(listView, &QListView::doubleClicked, this, &MainWindow::onListItemDoubleClicked);
    connect(backButton, &QPushButton::clicked, this, &MainWindow::onBackClicked);




    // --- Client ---
    client = new Client(this);
    connect(client, &Client::directoryListed, this, &MainWindow::onDirectoryListed);
    client->connectToServer(serverIP, serverPort);
}

void MainWindow::onDirectorySelected(const QModelIndex &index) {
    QString path = treeModel->filePath(index);
    qDebug() << "Directory selected:" << path;
    if (path.isEmpty()) return;
    pathBar->setText(path);
    statusBar()->showMessage("Loading...");
    client->sendRequest(path);  // ask server for contents
}

void MainWindow::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory");
    if (!dir.isEmpty()) {
        pathBar->setText(dir);
        treeView->setRootIndex(treeModel->index(dir));
        statusBar()->showMessage("Loading...");
        client->sendRequest(dir);  // ← ask server
    }
}

void MainWindow::onDirectoryListed(const QStringList &entries) {
    listModel->clear();
    QFileIconProvider iconProvider;
    
    for (const QString &entry : entries) {
        QStandardItem *item = new QStandardItem();
        if (entry.startsWith("DIR:")) {
            item->setText(entry.mid(4));
            item->setIcon(iconProvider.icon(QFileIconProvider::Folder));
        } else if (entry.startsWith("FILE:")) {
            item->setText(entry.mid(5));
            item->setIcon(iconProvider.icon(QFileIconProvider::File));
        }
        listModel->appendRow(item);
    }
    statusBar()->showMessage("Ready");
}

void MainWindow::onRequestSent() {
    statusBar()->showMessage("Loading...");
}

void MainWindow::onListItemDoubleClicked(const QModelIndex &index) {
    QString name = listModel->item(index.row())->text();
    QString currentPath = pathBar->text();
    QString newPath = currentPath + "/" + name;
    statusBar()->showMessage("Loading...");
    client->sendRequest(newPath);
    pathBar->setText(newPath);
}

void MainWindow::onBackClicked() {
    QString currentPath = pathBar->text();
    QDir dir(currentPath);
    dir.cdUp();
    QString parentPath = dir.absolutePath();
    pathBar->setText(parentPath);
    statusBar()->showMessage("Loading...");
    client->sendRequest(parentPath);
}









MainWindow::~MainWindow() {}

