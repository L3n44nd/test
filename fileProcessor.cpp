#include "fileProcessor.h"
#include <QApplication>

fileProcessor::fileProcessor(QObject *parent)
    : QObject(parent){
    buffer.resize(buffSize);
    buffPtr = buffer.data();
}

QString fileProcessor::generatePath(const QString& fileName) const {
    QFileInfo fileInfo(fileName);
    QString fileSuffix = fileInfo.completeSuffix();
    QString fileBaseName = fileInfo.completeBaseName();
    QString currFilePath = QDir(targetDirectory).filePath(fileName);

    if (SaveMode == saveMode::overwrite) return currFilePath;
    if (!QFile::exists(currFilePath)) return currFilePath;

    uint64_t fileNum = 1;
    while (fileNum < UINT64_MAX) {
        QString newFileName = QString("%1_%2.%3").arg(fileBaseName).arg(fileNum++).arg(fileSuffix);
        QString newPath = QDir(targetDirectory).filePath(newFileName);
        if (!QFile::exists(newPath)) return newPath;
    }
    return "";
}

void fileProcessor::processFiles(const QStringList& files){
    if (files.isEmpty()) return;

    int filesProcessed = 0;
    totalSize = bytesProcessed = 0;

    for (const auto& file : files) {
        QFileInfo fileInfo(file);
        totalSize += fileInfo.size();
    }

    QFileInfo info(files.first());
    QString dirFrom = info.absolutePath();
    bool sameDir = dirFrom == targetDirectory;
    bool needToDelete = deleteMode && !(sameDir && SaveMode == saveMode::overwrite);

    for (const auto& file : files) {
        bool successful = processOneFile(file);
        if (!successful) {
            if (stopReq.load()) emit stopped();
            else {
                QFileInfo fileInfo(file);
                emit error(QString("При обработке файла %1 возникла ошибка. Обработано файлов: %2").arg(fileInfo.fileName()).arg(filesProcessed));
                //поток заблокируется здесь пока не будет выполнен слот обработки ошибки в главном потоке
            }
            return;
        }
        if (needToDelete) QFile::remove(file);
        ++filesProcessed;
    }
    emit finished(StartMode);
}

bool fileProcessor::processOneFile(const QString& filePath){
    if (stopReq.load()) return false;

    QFile fileFrom(filePath);
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    QString outputPath = generatePath(fileName);
    QString tmpPath = outputPath + ".tmp";
    QFile tmpFile(tmpPath);

    if (!fileFrom.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (!tmpFile.open(QIODevice::WriteOnly)) {
        fileFrom.close();
        tmpFile.remove();
        return false;
    }

    int bytesRead;
    while ((bytesRead = fileFrom.read(buffPtr, buffSize)) > 0) {
        if (stopReq.load()) {
            tmpFile.remove();
            return false;
        }
        for (int i = 0; i < bytesRead; ++i) {
            buffPtr[i] ^= key[i % 8]; //размер буфера кратен 8, смещения не будет
        }
        tmpFile.write(buffPtr, bytesRead);
        bytesProcessed += bytesRead;
        emit progress(bytesProcessed, totalSize);
    }

    fileFrom.close();
    tmpFile.close();

    QFile::remove(outputPath);//на случай если стоит режим перезаписи. Без удаления tmp может пытаться переименоваться в существующий файл. если режим не стоит, пути никогда не совпадут и ничего не удалится.
    if (!tmpFile.rename(outputPath)) {
        tmpFile.remove();
        return false;
    }

    return true;
}

void fileProcessor::run(const QStringList& files){
    workingNow.store(true);
    processFiles(files);
    workingNow.store(false);
}

fileProcessor::~fileProcessor(){}

