#pragma once


#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QProgressBar>
#include <QDockWidget>
#include <QListWidget>


#include "client.h"
// #include "server.h"


class ClientWindow : public QMainWindow {
    Q_OBJECT

public:
    ClientWindow(const QString &serverIP, int serverPort, QWidget *parent = nullptr);
    ~ClientWindow();

private slots:
    void onDirectorySelected(const QModelIndex &index);
    void onBrowseClicked();
    void onRequestSent();
    void onDirectoryListed(const QStringList &entries);
    void onListItemDoubleClicked(const QModelIndex &index);
    void onBackClicked();
    void onContextMenu(const QPoint &pos);
    void onOperationSuccess(const QString &message);
    void onOperationError(const QString &message);
    void onDownloadComplete(const QString &savePath);
    void onDownloadError(const QString &message);


private:
    Client *client;


    QLineEdit *pathBar;


    QPushButton *homeButton;
    QPushButton *refreshButton;
    QPushButton *backButton;
    // QPushButton *browseButton;


    QTreeView *treeView;
    QListView *listView;
    QFileSystemModel *treeModel;
    QStandardItemModel   *listModel;
    QProgressBar *progressBar;
    QDockWidget *transferDock;
    QListWidget *transferList;
    QListWidgetItem *activeTransferItem = nullptr;


    QString pendingPath;
    QString currentPath;
    QString lastDownloadPath;


    void downloadFile(const QString &remotePath, bool isDirectory = false);
};  