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
#include <QToolButton>
#include <QPainter>
#include <QTimer>
#include <QEvent>
#include "transferrow.h"


// Lightweight busy spinner that floats centered over a parent widget (the file
// list). Appears only after a short delay so quick listings don't flicker.
class SpinnerOverlay : public QWidget {
public:
    explicit SpinnerOverlay(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();

        anim.setInterval(40);
        connect(&anim, &QTimer::timeout, this, [this]() {
            angle = (angle + 30) % 360;
            update();
        });

        showDelay.setSingleShot(true);
        showDelay.setInterval(250);
        connect(&showDelay, &QTimer::timeout, this, [this]() {
            if (parentWidget()) setGeometry(parentWidget()->rect());
            raise();
            show();
            anim.start();
        });

        if (parent) parent->installEventFilter(this);
    }

    void start() {
        if (isVisible() || showDelay.isActive()) return;
        showDelay.start();
    }

    void stop() {
        showDelay.stop();
        anim.stop();
        hide();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (obj == parentWidget() && ev->type() == QEvent::Resize && isVisible())
            setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(obj, ev);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(255, 255, 255, 130));

        const qreal dim = 44;
        const int blades = 12;
        p.translate(width() / 2.0, height() / 2.0);
        p.rotate(angle);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < blades; ++i) {
            int alpha = 40 + 215 * i / (blades - 1);
            p.setBrush(QColor(110, 110, 110, alpha));
            p.drawRoundedRect(QRectF(dim / 2.0 - 7, -2.5, 7, 5), 2.5, 2.5);
            p.rotate(360.0 / blades);
        }
    }

private:
    QTimer anim;
    QTimer showDelay;
    int angle = 0;
};


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

    listSpinner = new SpinnerOverlay(listView);

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

    connect(client, &Client::listingRequested, this, [this]() { listSpinner->start(); });
    connect(client, &Client::directoryListed, this, [this](const QStringList &) { listSpinner->stop(); });
    connect(client, &Client::operationError, this, [this](const QString &) { listSpinner->stop(); });

    connect(client, &Client::downloadComplete, this, &ClientWindow::onDownloadComplete);
    connect(client, &Client::downloadError, this, &ClientWindow::onDownloadError);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setMaximumWidth(200);
    progressBar->setTextVisible(true);
    progressBar->setVisible(false);
    statusBar()->addPermanentWidget(progressBar);

    connect(client, &Client::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (progressBar->maximum() == 0) progressBar->setRange(0, 100);
        if (!progressBar->isVisible()) progressBar->setVisible(true);
        int pct = total > 0 ? int((received * 100) / total) : 0;
        progressBar->setValue(pct);
    });

    connect(client, &Client::info, this, [this](const QString &msg) {
        statusBar()->showMessage(msg);
        progressBar->setRange(0, 0);
        progressBar->setVisible(true);
    });

    connect(client, &Client::ready, this, [this]() {
        statusBar()->showMessage("Connected");
        client->sendRequest("LIST:" + currentPath);
    });

    transferDock = new QDockWidget("Transfers", this);
    transferList = new QListWidget(transferDock);
    transferDock->setWidget(transferList);
    transferDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, transferDock);

    auto *transferToggleBtn = new QToolButton(this);
    transferToggleBtn->setDefaultAction(transferDock->toggleViewAction());
    transferToggleBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    statusBar()->addPermanentWidget(transferToggleBtn);

    auto addTransferRow = [this](TransferRow::Direction dir, const QString &keyPath, const QString &otherPath) {
        auto *row = new TransferRow(dir, keyPath, otherPath);
        auto *li = new QListWidgetItem();
        li->setSizeHint(row->sizeHint());
        transferList->addItem(li);
        transferList->setItemWidget(li, row);
        connect(row, &TransferRow::cancelRequested, this, [this](TransferRow::Direction d, const QString &p) {
            if (d == TransferRow::Download) client->cancelDownload(p);
            else client->cancelUpload(p);
        });
    };

    auto findRow = [this](TransferRow::Direction dir, const QString &keyPath) -> TransferRow* {
        for (int i = 0; i < transferList->count(); ++i) {
            QListWidgetItem *li = transferList->item(i);
            auto *row = qobject_cast<TransferRow*>(transferList->itemWidget(li));
            if (row && row->getDirection() == dir && row->getPath() == keyPath) return row;
        }
        return nullptr;
    };

    auto findRowBySavePath = [this](const QString &savePath) -> TransferRow* {
        for (int i = 0; i < transferList->count(); ++i) {
            QListWidgetItem *li = transferList->item(i);
            auto *row = qobject_cast<TransferRow*>(transferList->itemWidget(li));
            if (row && row->getDirection() == TransferRow::Download
                && row->getOtherPath() == savePath) return row;
        }
        return nullptr;
    };

    connect(client, &Client::transferQueued, this, [addTransferRow](const QString &remotePath, const QString &savePath) {
        addTransferRow(TransferRow::Download, remotePath, savePath);
    });

    connect(client, &Client::transferStarted, this, [findRowBySavePath](const QString &savePath) {
        if (auto *row = findRowBySavePath(savePath)) row->setActive();
    });

    connect(client, &Client::downloadProgress, this, [findRowBySavePath, this](qint64 received, qint64 total) {
        // current download row matches currentDownload's savePath; we don't have it directly here,
        // walk and find the one that's Active among Download rows
        for (int i = 0; i < transferList->count(); ++i) {
            QListWidgetItem *li = transferList->item(i);
            auto *row = qobject_cast<TransferRow*>(transferList->itemWidget(li));
            if (row && row->getDirection() == TransferRow::Download) {
                row->setProgress(received, total);
                break;
            }
        }
    });

    connect(client, &Client::downloadComplete, this, [findRowBySavePath](const QString &savePath) {
        if (auto *row = findRowBySavePath(savePath)) row->setDone();
    });

    connect(client, &Client::transferFailed, this, [findRowBySavePath](const QString &savePath, const QString &reason) {
        if (auto *row = findRowBySavePath(savePath)) {
            if (reason == "Cancelled") row->setCancelled();
            else row->setFailed(reason);
        }
    });

    connect(client, &Client::uploadQueued, this, [addTransferRow](const QString &localPath, const QString &destPath) {
        addTransferRow(TransferRow::Upload, destPath, localPath);
    });

    connect(client, &Client::uploadStarted, this, [findRow](const QString &destPath) {
        if (auto *row = findRow(TransferRow::Upload, destPath)) row->setActive();
    });

    connect(client, &Client::uploadProgress, this, [findRow](const QString &destPath, qint64 sent, qint64 total) {
        if (auto *row = findRow(TransferRow::Upload, destPath)) row->setProgress(sent, total);
    });

    connect(client, &Client::uploadComplete, this, [findRow](const QString &destPath) {
        if (auto *row = findRow(TransferRow::Upload, destPath)) row->setDone();
    });

    connect(client, &Client::uploadFailed, this, [findRow](const QString &destPath, const QString &reason) {
        if (auto *row = findRow(TransferRow::Upload, destPath)) {
            if (reason == "Cancelled") row->setCancelled();
            else row->setFailed(reason);
        }
    });


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
    QMenu contextMenu(this);
    QAction *deleteAction = contextMenu.addAction("Delete");
    QAction *renameAction = contextMenu.addAction("Rename");
    QAction *newFolderAction = contextMenu.addAction("New Folder");
    QAction *downloadAction = contextMenu.addAction("Download");
    QAction *uploadAction = contextMenu.addAction("Upload here...");

    if (hasSelection) {
        QStandardItem *item = listModel->item(index.row());
        name = item->text();
        fullPath = currentPath + "/" + name;
    } 
    
    
    deleteAction->setEnabled(hasSelection);
    renameAction->setEnabled(hasSelection);
    downloadAction->setEnabled(true); 


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

    }else if (selected == downloadAction) {
        if (hasSelection) {
            QStandardItem *item = listModel->item(index.row());
            bool isDir = item->data(Qt::UserRole).toString() == "DIR";
            downloadFile(fullPath, isDir);
        } else {
            downloadFile(currentPath, true);
        }


    }else if (selected == uploadAction) {
        QString localPath = QFileDialog::getOpenFileName(this, "Pick file to upload");
        if (localPath.isEmpty()) return;
        QString destDir = currentPath;
        QString destPath = destDir + "/" + QFileInfo(localPath).fileName();
        if (!client->queueUpload(localPath, destPath)) {
            statusBar()->showMessage("Cannot open local file");
        }
    }else if (selected == newFolderAction) {
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

void ClientWindow::downloadFile(const QString &remotePath, bool isDirectory) {
    QString filename = QFileInfo(remotePath).fileName();
    if (isDirectory) {
        if (filename.isEmpty()) filename = "download";
        filename += ".zip";
    }
    QString savePath = QFileDialog::getSaveFileName(this, "Save File", lastDownloadPath + "/" + filename);
    if (savePath.isEmpty()) return;
    lastDownloadPath = QFileInfo(savePath).absolutePath();
    client->queueDownload(remotePath, savePath);
}

void ClientWindow::onDownloadComplete(const QString &savePath) {
    progressBar->setVisible(false);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    statusBar()->showMessage("Downloaded to: " + savePath);
}

void ClientWindow::onDownloadError(const QString &message) {
    progressBar->setVisible(false);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    statusBar()->showMessage("Download failed: " + message);
}

ClientWindow::~ClientWindow() {}

