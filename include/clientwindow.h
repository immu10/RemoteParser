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

class SpinnerOverlay;

class ClientWindow : public QMainWindow {
    Q_OBJECT

public:
    ClientWindow(const QString &serverIP, int serverPort, QWidget *parent = nullptr);
    ~ClientWindow();

private slots:
    void onTreeExpanded(const QModelIndex &index);
    void onTreeClicked(const QModelIndex &index);
    void onRequestSent();
    void onDirectoryListed(const QString &path, const QStringList &entries);
    void onListItemDoubleClicked(const QModelIndex &index);
    void onBackClicked();
    void onContextMenu(const QPoint &pos);
    void onOperationSuccess(const QString &message);
    void onOperationError(const QString &message);
    void onDownloadComplete(const QString &savePath);
    void onDownloadError(const QString &message);
    void onConnectionLost(const QString &reason);


private:
    Client *client;


    QLineEdit *pathBar;


    QPushButton *homeButton;
    QPushButton *refreshButton;
    QPushButton *backButton;
    // QPushButton *browseButton;


    QTreeView *treeView;
    QListView *listView;
    SpinnerOverlay *listSpinner = nullptr;
    QStandardItemModel   *treeModel;   // remote server tree (lazy-loaded)
    QStandardItemModel   *listModel;
    QProgressBar *progressBar;
    QDockWidget *transferDock;
    QListWidget *transferList;
    QListWidgetItem *activeTransferItem = nullptr;


    QString pendingPath;
    QString currentPath;
    QString lastDownloadPath;


    void downloadFile(const QString &remotePath, bool isDirectory = false);

    // Remote tree helpers
    QStandardItem *findTreeNode(const QString &path);
    void updateTreeNode(const QString &path, const QStringList &entries);
};