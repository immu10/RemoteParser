#pragma once

#include <QWidget>
#include <QLabel>
#include <QToolButton>

class TransferRow : public QWidget {
    Q_OBJECT

public:
    enum Direction { Download, Upload };

    TransferRow(Direction dir, const QString &path, const QString &otherPath, QWidget *parent = nullptr);

    void setQueued();
    void setActive();
    void setProgress(qint64 bytes, qint64 total);
    void setDone();
    void setFailed(const QString &reason);
    void setCancelled();

    QString getPath() const { return m_path; }
    QString getOtherPath() const { return m_otherPath; }
    Direction getDirection() const { return m_dir; }

signals:
    void cancelRequested(Direction dir, const QString &path);

private:
    Direction m_dir;
    QString m_path;
    QString m_otherPath;
    QLabel *nameLabel;
    QLabel *statusLabel;
    QToolButton *cancelBtn;
};
