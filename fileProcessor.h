#pragma once

#include <QObject>
#include <qfile.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qtimer.h>
#include <atomic>
#include "modes.h"

class fileProcessor  : public QObject
{
    Q_OBJECT

public:
    fileProcessor(QObject *parent = nullptr);
    ~fileProcessor();
    void setSaveMode(saveMode SaveMode);
    void setStartMode(startMode StartMode);
    void setDeleteMode(bool deleteInputFiles);
    void setTargetDirectory(const QString& targetDirectory);
    void setXorKey(const QByteArray& key);
    bool isWorking() const { return workingNow.load(); };
    void stop() { stopReq.store(true); }
    void resetStopFlag() {stopReq.store(false); }

public slots:
    void run(const QStringList& files);

signals:
    void error(const QString& info);
    void stopped();
    void progress(qint64 processed, qint64 total);
    void finished(startMode StartMode);

private:
    bool deleteMode = false;
    bool fileErr = false;
    saveMode SaveMode;
    startMode StartMode;
    QString targetDirectory;
    QByteArray key;
    qint64 bytesProcessed;
    qint64 totalSize;
    std::atomic<bool> stopReq = false;
    std::atomic<bool> workingNow = false;

    QString generatePath(const QString& fileName) const;
    void processFiles(const QStringList& files);
    bool processOneFile(const QString& path);
};
