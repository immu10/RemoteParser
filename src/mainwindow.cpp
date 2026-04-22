#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle("File Parser");
    setMinimumSize(900, 600);

    // --- Models ---
    treeModel = new QFileSystemModel(this);
    treeModel->setRootPath("");
    treeModel->setFilter(QDir::NoDotAndDotDot | QDir::Dirs);

    listModel = new QFileSystemModel(this);
    listModel->setRootPath("");
    listModel->setFilter(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);

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

    // --- Path bar + browse button ---
    pathBar = new QLineEdit(this);
    pathBar->setReadOnly(true);
    pathBar->setPlaceholderText("Select a folder...");

    browseButton = new QPushButton("Browse", this);

    QHBoxLayout *topBar = new QHBoxLayout();
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
    connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onDirectorySelected);
    connect(browseButton, &QPushButton::clicked,
            this, &MainWindow::onBrowseClicked);
}

void MainWindow::onDirectorySelected(const QModelIndex &index) {
    QString path = treeModel->filePath(index);
    pathBar->setText(path);
    listView->setRootIndex(listModel->index(path));
}

void MainWindow::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory");
    if (!dir.isEmpty()) {
        pathBar->setText(dir);
        treeView->setRootIndex(treeModel->index(dir));
        listView->setRootIndex(listModel->index(dir));
    }
}

MainWindow::~MainWindow() {}