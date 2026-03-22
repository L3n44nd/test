#include "fileProcessor.h"
#include <QApplication>

fileProcessor::fileProcessor(QObject *parent)
    : QObject(parent){}

QString fileProcessor::generatePath(const QString& fileName) const {
    QFileInfo fileInfo(fileName);
    QString fileSuffix = fileInfo.completeSuffix();
    QString fileBaseName = fileInfo.completeBaseName();

    QString currFilePath = QDir(targetDirectory).filePath(fileName);

    if (SaveMode == saveMode::overwrite) {
        return currFilePath;
    }

    if (!QFile::exists(currFilePath)) return currFilePath;

    int fileNum = 1;
    while (fileNum < 50000) {
        QString newFileName = QString("%1_%2.%3").arg(fileBaseName).arg(fileNum++).arg(fileSuffix);
        QString newPath = QDir(targetDirectory).filePath(newFileName);
        if (!QFile::exists(newPath)) return newPath;
    }

    QString currDate = QDateTime::currentDateTime().toString("dd-MM-yyyy_hh-mm-ss");
    QString datePath = QDir(targetDirectory).filePath(QString("%1_%2.%3").arg(fileBaseName).arg(currDate).arg(fileSuffix));
    return datePath;//если за адекватное колво попыток не нашелся свободный номер, сохранить с текущей датой
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

    for (const auto& file : files) {
        if(stopReq.load()){
            emit stopped();
            return;
        }
        QFileInfo fileInfo(file);
        bool successful = processOneFile(file);
        if (!successful) {
            if (fileErr) {
                emit error(QString("При обработке файла %1 возникла ошибка. Обработано файлов: %2").arg(fileInfo.fileName()).arg(filesProcessed));
            }
            else {
                emit stopped();
            }
            return;
        }
        if (deleteMode) {
            if (!(sameDir && SaveMode == saveMode::overwrite)) QFile::remove(file);//если директории совпадают и стоят режимы перезапись+удаление, без этой проверки файлы удалятся после обработки
        }
        ++filesProcessed;
    }
    emit finished(StartMode);
}

bool fileProcessor::processOneFile(const QString& filePath){
    QFile fileFrom(filePath);
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    QString outputPath = generatePath(fileName);
    QString tmpPath = outputPath + ".tmp";
    QFile tmpFile(tmpPath);

    if (!fileFrom.open(QIODevice::ReadOnly)) {
        fileErr = true;
        return false;
    }
    if (!tmpFile.open(QIODevice::WriteOnly)) {
        fileErr = true;
        fileFrom.close();
        tmpFile.remove();
        return false;
    }

    const int buffSize = 10 * 1024 * 1024;
    QByteArray buffer(buffSize, Qt::Uninitialized);
    int bytesRead;
    char* buffPtr = buffer.data();

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

    QFile::remove(outputPath);//на случай если стоит режим перезаписи. Без удаления tmp может пыаться переименоваться в существующий файл. если режим не стоит, пути никогда не совпадут и ничего не удалится.
    if (!tmpFile.rename(outputPath)) {
        fileErr = true;
        tmpFile.remove();
        return false;
    }

    return true;
}

void fileProcessor::run(const QStringList& files){
    fileErr = false;
    workingNow.store(true);
    processFiles(files);
    workingNow.store(false);
}

fileProcessor::~fileProcessor(){}

