#include "transferrow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QLocale>

TransferRow::TransferRow(Direction dir, const QString &path, const QString &otherPath, QWidget *parent)
    : QWidget(parent), m_dir(dir), m_path(path), m_otherPath(otherPath) {

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(8);

    auto *texts = new QVBoxLayout();
    texts->setSpacing(2);

    QString arrow = (dir == Download) ? "↓ " : "↑ ";
    nameLabel = new QLabel(arrow + QFileInfo(path).fileName());
    QFont f = nameLabel->font();
    f.setBold(true);
    nameLabel->setFont(f);

    statusLabel = new QLabel("Queued");
    statusLabel->setStyleSheet("color: gray;");

    texts->addWidget(nameLabel);
    texts->addWidget(statusLabel);
    layout->addLayout(texts, 1);

    cancelBtn = new QToolButton();
    cancelBtn->setText("✕");
    cancelBtn->setToolTip("Cancel");
    cancelBtn->setAutoRaise(true);
    connect(cancelBtn, &QToolButton::clicked, this, [this]() {
        emit cancelRequested(m_dir, m_path);
    });
    layout->addWidget(cancelBtn, 0, Qt::AlignVCenter);

    setToolTip(otherPath);
}

void TransferRow::setQueued() {
    statusLabel->setText("Queued");
    statusLabel->setStyleSheet("color: gray;");
    cancelBtn->setEnabled(true);
    cancelBtn->setVisible(true);
}

void TransferRow::setActive() {
    statusLabel->setText("Starting...");
    statusLabel->setStyleSheet("");
    cancelBtn->setEnabled(true);
    cancelBtn->setVisible(true);
}

void TransferRow::setProgress(qint64 bytes, qint64 total) {
    QLocale loc;
    QString b = loc.formattedDataSize(bytes);
    QString t = loc.formattedDataSize(total);
    int pct = total > 0 ? int((bytes * 100) / total) : 0;
    statusLabel->setText(QString("%1 / %2  (%3%)").arg(b, t).arg(pct));
    statusLabel->setStyleSheet("");
}

void TransferRow::setDone() {
    statusLabel->setText("Done");
    statusLabel->setStyleSheet("color: green;");
    cancelBtn->setVisible(false);
}

void TransferRow::setFailed(const QString &reason) {
    statusLabel->setText("Failed: " + reason);
    statusLabel->setStyleSheet("color: red;");
    cancelBtn->setVisible(false);
}

void TransferRow::setCancelled() {
    statusLabel->setText("Cancelled");
    statusLabel->setStyleSheet("color: gray;");
    cancelBtn->setVisible(false);
}
