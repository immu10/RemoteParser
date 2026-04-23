#include "clientwindow.h"


#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QWidget>
#include <QStatusBar>
#include <QDebug>
#include <QFileIconProvider>
#include <QStandardItemModel>
#include <QMenu>
#include <QInputDialog>
#include <QShortcut>
#include <QStandardPaths>



ClientWindow::ClientWindow(const QString &serverIP, int serverPort, QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle("File Parser");
    setMinimumSize(900, 600);
    
    lastDownloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

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
    listView->setContextMenuPolicy(Qt::CustomContextMenu);

    // --- Path bar +  buttons ---
    pathBar = new QLineEdit(this);
    pathBar->setReadOnly(true);
    pathBar->setPlaceholderText("Select a folder...");
    
    // browseButton = new QPushButton("Browse", this);
    refreshButton = new QPushButton("↻", this);
    homeButton = new QPushButton("🏠", this);
    backButton = new QPushButton("←", this);

    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->addWidget(backButton);
    topBar->addWidget(pathBar);
    topBar->addWidget(homeButton);
    // topBar->addWidget(browseButton);
    topBar->addWidget(refreshButton);


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
    
    client = new Client(this);


    // connect(browseButton, &QPushButton::clicked,this, &ClientWindow/::onBrowseClicked);
    connect(refreshButton, &QPushButton::clicked, this, [this]() {pendingPath = currentPath; client->sendRequest("LIST:" + currentPath);});
    connect(homeButton, &QPushButton::clicked, this, [this]() {pendingPath = ""; client->sendRequest("DRIVES");});
    connect(backButton, &QPushButton::clicked, this, &ClientWindow::onBackClicked);

    connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,this, &ClientWindow::onDirectorySelected);
    connect(listView, &QListView::doubleClicked, this, &ClientWindow::onListItemDoubleClicked);
    connect(listView, &QListView::customContextMenuRequested, this, &ClientWindow::onContextMenu);

    connect(client, &Client::operationSuccess, this, &ClientWindow::onOperationSuccess);
    connect(client, &Client::operationError, this, &ClientWindow::onOperationError);
    connect(client, &Client::directoryListed, this, &ClientWindow::onDirectoryListed);

    
    client->connectToServer(serverIP, serverPort);
    pendingPath = "C:/Users";
    currentPath = "C:/Users";
    pathBar->setText(currentPath);

    //keybinds

    new QShortcut(QKeySequence("F5"), this, [this]() {
        client->sendRequest("LIST:" + currentPath);
    });
    new QShortcut(QKeySequence("Alt+Left"), this, [this]() {
        onBackClicked();
    });
    new QShortcut(QKeySequence("Backspace"), this, [this]() {
        onBackClicked();
    });
    new QShortcut(QKeySequence("Alt+Home"), this, [this]() {
        currentPath = "";
        pendingPath = "";
        client->sendRequest("DRIVES");
    });
    new QShortcut(QKeySequence("Delete"), this, [this]() {
        QModelIndex index = listView->currentIndex();
        if (!index.isValid()) return;
        QStandardItem *item = listModel->item(index.row());
        if (item->data(Qt::UserRole).toString() == "FILE" || item->data(Qt::UserRole).toString() == "DIR") {
            QString fullPath = currentPath + "/" + item->text();
            client->sendRequest("DELETE:" + fullPath);
        }
    });
    new QShortcut(QKeySequence("F2"), this, [this]() {
        QModelIndex index = listView->currentIndex();
        if (!index.isValid()) return;
        onContextMenu(listView->visualRect(index).center());
    });

}

void ClientWindow::onDirectorySelected(const QModelIndex &index) {
    QString path = treeModel->filePath(index);
    qDebug() << "Directory selected:" << path;
    if (path.isEmpty()) return;
    pendingPath = path;
    statusBar()->showMessage("Loading...");
    client->sendRequest("LIST:" + path);  // ask server for contents
}

void ClientWindow::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory");
    if (!dir.isEmpty()) {
        pendingPath = dir;
        treeView->setRootIndex(treeModel->index(dir));
        statusBar()->showMessage("Loading...");
        client->sendRequest("LIST:" + dir);  // ← ask server
    }
}

void ClientWindow::onDirectoryListed(const QStringList &entries) {
    currentPath = pendingPath;
    pathBar->setText(currentPath);
    backButton->setEnabled(!currentPath.isEmpty());
    listModel->clear();
    QFileIconProvider iconProvider;
    
    
    for (const QString &entry : entries) {
        QStandardItem *item = new QStandardItem();
        if (entry.startsWith("DIR:")) {
            item->setText(entry.mid(4));
            item->setIcon(iconProvider.icon(QFileIconProvider::Folder));
            item->setData("DIR", Qt::UserRole); 
        } else if (entry.startsWith("FILE:")) {
            item->setText(entry.mid(5));
            item->setIcon(iconProvider.icon(QFileIconProvider::File));
            item->setData("FILE", Qt::UserRole); 
        }
        listModel->appendRow(item);
    }
    statusBar()->showMessage("Ready");
}

void ClientWindow::onRequestSent() {
    statusBar()->showMessage("Loading...");
}

void ClientWindow::onListItemDoubleClicked(const QModelIndex &index) {
    QStandardItem *item = listModel->item(index.row());
    if (item->data(Qt::UserRole).toString() == "FILE") return; 

    QString name = item->text();  // ← use item directly
    QString newPath = currentPath.isEmpty() ? name : currentPath + "/" + name;
    newPath = QDir::cleanPath(newPath);
    pendingPath = newPath;
    statusBar()->showMessage("Loading...");
    client->sendRequest("LIST:" + newPath);

}

void ClientWindow::onBackClicked() {
    QDir dir(currentPath);
    dir.cdUp();
    if (dir.absolutePath() == currentPath) {
        currentPath = "";
        pendingPath = "";
        pathBar->setText("");
        client->sendRequest("DRIVES");
        return;
    }
    pendingPath = dir.absolutePath();
    statusBar()->showMessage("Loading...");
    client->sendRequest("LIST:" + pendingPath);
}

void ClientWindow::onContextMenu(const QPoint &pos) {
    QModelIndex index = listView->indexAt(pos);
    bool hasSelection = index.isValid();
    
    QString name, fullPath;
    if (hasSelection) {
        name = listModel->item(index.row())->text();
        fullPath = currentPath + "/" + name;
    }

    QMenu contextMenu(this);
    QAction *deleteAction = contextMenu.addAction("Delete");
    QAction *renameAction = contextMenu.addAction("Rename");
    QAction *newFolderAction = contextMenu.addAction("New Folder");

    deleteAction->setEnabled(hasSelection);
    renameAction->setEnabled(hasSelection);

    QAction *selected = contextMenu.exec(listView->mapToGlobal(pos));
    
    if (selected == deleteAction) {
        client->sendRequest("DELETE:" + fullPath);
    } else if (selected == renameAction) {
        bool ok;
    QString newName = QInputDialog::getText(this, "Rename", "New name:", QLineEdit::Normal, name, &ok);
    if (ok && !newName.isEmpty()) {
        QString newPath = currentPath + "/" + newName;
        client->sendRequest("RENAME:" + fullPath + "|" + newPath);
    }
} else if (selected == newFolderAction) {
    bool ok;
    QString folderName = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "New Folder", &ok);
    if (ok && !folderName.isEmpty()) {
        client->sendRequest("MKDIR:" + currentPath + "/" + folderName);
    }
}
}


void ClientWindow::onOperationSuccess(const QString &message) {
    statusBar()->showMessage(message);
    pendingPath = currentPath;
    client->sendRequest("LIST:" + currentPath);  // refresh the directory
}

void ClientWindow::onOperationError(const QString &message) {
    statusBar()->showMessage("Error: " + message);
}



ClientWindow::~ClientWindow() {}

