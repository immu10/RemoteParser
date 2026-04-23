#pragma once


#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>


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


private:
    QTreeView *treeView;
    QListView *listView;
    QFileSystemModel *treeModel;
    QStandardItemModel   *listModel;
    QLineEdit *pathBar;
    QPushButton *browseButton;
    Client *client;
    QPushButton *backButton;
};  