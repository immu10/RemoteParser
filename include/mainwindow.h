#pragma once
#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onDirectorySelected(const QModelIndex &index);
    void onBrowseClicked();

private:
    QTreeView *treeView;
    QListView *listView;
    QFileSystemModel *treeModel;
    QFileSystemModel *listModel;
    QLineEdit *pathBar;
    QPushButton *browseButton;
};