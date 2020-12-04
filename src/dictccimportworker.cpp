/*
    Copyright (C) 2016-19 Sebastian J. Wolf

    This file is part of Dictionary.

    Dictionary is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Dictionary is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Dictionary. If not, see <http://www.gnu.org/licenses/>.
*/

#include "dictccimportworker.h"
#include <JlCompress.h>
#include <QDebug>
#include <QDir>
#include <QRegExp>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QStringList>
#include <QStringListIterator>

DictCCImportWorker::DictCCImportWorker()
{
    currentMetadataVersion = 1;
}

void DictCCImportWorker::importDictionaries()
{
    emit statusChanged("Checking for new dictionaries...");
    QString downloadDirectoryString = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString tempDirectoryString = getTempDirectory();
    qDebug() << "Reading from directory: " << downloadDirectoryString;
    qDebug() << "Extracting temporarily to directory: " << tempDirectoryString;
    QStringList nameFilter("*.zip");
    QDir downloadDirectory(downloadDirectoryString);
    QStringList zipFiles = downloadDirectory.entryList(nameFilter);
    QStringListIterator zipFilesIterator(zipFiles);
    QRegExp dictCCMatcher("\\w+\\-\\d+\\-\\w+\\.zip");
    while (zipFilesIterator.hasNext()) {
        QString zipArchiveFileName = zipFilesIterator.next();
        QString zipArchiveFullPath = downloadDirectoryString + "/" + zipArchiveFileName;
        if (dictCCMatcher.indexIn(zipArchiveFileName, 0) != -1) {
            qDebug() << downloadDirectoryString + "/" + zipArchiveFileName + " successfully validated!";
            QString zipArchiveExtractionDir = getDirectory(tempDirectoryString + "/" + zipArchiveFileName);
            qDebug() << "Extracting archive " + zipArchiveFullPath + " to " + zipArchiveExtractionDir;
            QStringList extractedFiles = JlCompress::extractDir(zipArchiveFullPath, zipArchiveExtractionDir);
            QStringListIterator extractedFilesIterator(extractedFiles);
            while (extractedFilesIterator.hasNext()) {
                QString extractedFileName = extractedFilesIterator.next();
                qDebug() << "Extracted file: " + extractedFileName;
                readFile(extractedFileName);
            }
            QDir temporaryDirectory(zipArchiveExtractionDir);
            if (temporaryDirectory.rmdir(zipArchiveExtractionDir)) {
                qDebug() << "Temporary directory " + zipArchiveExtractionDir + " successfully deleted";
            } else {
                qDebug() << "Unable to delete temporary directory " + zipArchiveExtractionDir;
            }
        }
    }
    emit importFinished();
}

void DictCCImportWorker::readFile(QString &completeFileName)
{
    QFile inputFile(completeFileName);
    if (inputFile.open(QIODevice::ReadOnly))
    {
       QTextStream inputStream(&inputFile);
       inputStream.setCodec("UTF-8");
       QMap<QString,QString> dictionaryMetadata = getMetadata(inputStream);
       if (dictionaryMetadata.contains("languages") && dictionaryMetadata.contains("timestamp")) {
           emit statusChanged("Dict.cc dictionary found: " + dictionaryMetadata.value("languages") + " - " + dictionaryMetadata.value("timestamp"));
           writeDictionary(inputStream, dictionaryMetadata);
       }
       inputFile.close();
       if (inputFile.remove()) {
           qDebug() << "Temporary file " + completeFileName + " successfully deleted";
       } else {
           qDebug() << "Unable to delete temporary file " + completeFileName;
       }
    } else
    {
        qDebug() << "Unable to open extracted file " + completeFileName;
    }
}



QMap<QString,QString> DictCCImportWorker::getMetadata(QTextStream &inputStream) {
    QMap<QString,QString> metadata;
    if (!inputStream.atEnd()) {
        QString firstLine = inputStream.readLine();
        QRegExp languagesMatcher("([A-Z]{2}\\-[A-Z]{2})");
        if (firstLine.contains("dict.cc") && languagesMatcher.indexIn(firstLine) != -1) {
            qDebug() << "Dictionary languages identified: " + languagesMatcher.cap(1);
            metadata.insert("languages", languagesMatcher.cap(1));
        }
        if (!inputStream.atEnd()) {
            QString secondLine = inputStream.readLine();
            QRegExp dateTimeMatcher("(\\d{4}\\-\\d{2}\\-\\d{2}\\s\\d{2}\\:\\d{2})");
            if (dateTimeMatcher.indexIn(secondLine) != -1) {
                qDebug() << "Dictionary timestamp identified: " + dateTimeMatcher.cap(1);
                metadata.insert("timestamp", dateTimeMatcher.cap(1));
            }
        }
    }
    return metadata;
}

void DictCCImportWorker::writeDictionary(QTextStream &inputStream, QMap<QString, QString> &metadata)
{
    QString databaseDirectory = getDirectory(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/harbour-dictionary");
    QString databaseFilePath = databaseDirectory + "/" + metadata.value("languages") + ".db";
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "connection" + metadata.value("languages"));
    database.setDatabaseName(databaseFilePath);
    if (database.open()) {
        qDebug() << "SQLite database " + databaseFilePath + " successfully opened";
        if (!isAlreadyImported(metadata, database)) {
            writeMetadata(metadata, database);
            writeDictionaryEntries(inputStream, metadata, database);
            emit dictionaryFound(metadata.value("languages"), metadata.value("timestamp"));
        }
        database.close();
    } else {
        qDebug() << "Error opening SQLite database " + databaseFilePath;
    }
}

bool DictCCImportWorker::isAlreadyImported(QMap<QString, QString> &metadata, QSqlDatabase &database)
{
    QSqlQuery databaseQuery(database);
    QStringList existingTables = database.tables();
    if (existingTables.contains("metadata")) {
        databaseQuery.prepare("select value from metadata where key = 'metadataVersion'");
        databaseQuery.exec();
        if (databaseQuery.next()) {
            int importVersion = databaseQuery.value(0).toInt();
            if (importVersion < currentMetadataVersion) {
                qDebug() << "Metadata version outdated, so re-import needed";
                return false;
            }
        } else {
            qDebug() << "Metadata version not found, so outdated and re-import needed";
            return false;
        }
        databaseQuery.prepare("select value from metadata where key = 'timestamp'");
        databaseQuery.exec();
        if (databaseQuery.next()) {
            QString databaseTimestamp = databaseQuery.value(0).toString();
            QString fileTimestamp = metadata.value("timestamp");
            qDebug() << "Database timestamp: " + databaseTimestamp + " File timestamp: " + fileTimestamp;
            if (databaseTimestamp == fileTimestamp) {
                return true;
            }
        }
    }
    return false;
}

void DictCCImportWorker::writeMetadata(QMap<QString, QString> &metadata, QSqlDatabase &database)
{
    QSqlQuery databaseQuery(database);
    QStringList existingTables = database.tables();
    if (!existingTables.contains("metadata")) {
        databaseQuery.prepare("create table metadata (key text primary key, value text)");
        if (databaseQuery.exec()) {
            qDebug() << "Metadata table successfully created!";
        } else {
            qDebug() << "Error creating metadata table!";
        }
    } else {
        qDebug() << "Metadata table already existing";
    }
    databaseQuery.prepare("insert or replace into metadata values((:key),(:value))");
    databaseQuery.bindValue(":key", "languages");
    databaseQuery.bindValue(":value", metadata.value("languages"));
    if (databaseQuery.exec()) {
        qDebug() << "Languages successfully stored in metadata table";
    }
    databaseQuery.bindValue(":key", "timestamp");
    databaseQuery.bindValue(":value", metadata.value("timestamp"));
    if (databaseQuery.exec()) {
        qDebug() << "Timestamp successfully stored in metadata table";
    }
    databaseQuery.bindValue(":key", "metadataVersion");
    databaseQuery.bindValue(":value", QString::number(currentMetadataVersion));
    if (databaseQuery.exec()) {
        qDebug() << "Metadata version successfully stored in metadata table";
    }
}

void DictCCImportWorker::writeDictionaryEntries(QTextStream &inputStream, QMap<QString,QString> &metadata, QSqlDatabase &database)
{
    QSqlQuery databaseQuery(database);
    QStringList existingTables = database.tables();
    if (existingTables.contains("entries")) {
        databaseQuery.prepare("drop table entries");
        if (!databaseQuery.exec()) {
            qDebug() << "Error removing entries table.";
            return;
        }
    }

    databaseQuery.prepare("create virtual table entries using fts4(id integer primary key, left_word text, left_gender text, left_other text, right_word text, right_gender text, right_other text, category text, tokenize=unicode61 \"remove_diacritics=0\")");
    if (databaseQuery.exec()) {
        qDebug() << "Entries table successfully created!";
    } else {
        qDebug() << "Error creating entries table!";
        return;
    }

    QStringList rawEntries;
    int lineCount = 0;
    while(!inputStream.atEnd())
    {
        QString newLine = inputStream.readLine();
        if (!newLine.startsWith("#")) {
            rawEntries.append(newLine);
            lineCount++;
        }
    }
    QStringListIterator rawEntriesIterator(rawEntries);
    int currentLineNumber = 0;
    int successfullyWrittenEntries = 0;
    emit statusChanged(metadata.value("languages") + " dictionary: Importing " + QString::number(lineCount) + " entries.");

    databaseQuery.prepare("begin transaction");
    databaseQuery.exec();

    databaseQuery.prepare("insert into entries values((:id),(:left_word),(:left_gender),(:left_other),(:right_word),(:right_gender),(:right_other),(:category))");
    while (rawEntriesIterator.hasNext()) {
        currentLineNumber++;
        div_t divisionResult = div(currentLineNumber * 100, lineCount);
        div_t everyXResult = div(currentLineNumber, 1000);
        QStringList currentResult = rawEntriesIterator.next().split("\t");
        if (currentResult.count() >= 3) {
            databaseQuery.bindValue(":id", currentLineNumber);
            DictCCWord leftWord = getDictCCWord(currentResult.value(0));
            databaseQuery.bindValue(":left_word", leftWord.getWord());
            databaseQuery.bindValue(":left_gender", leftWord.getGender());
            databaseQuery.bindValue(":left_other", leftWord.getOptional());
            DictCCWord rightWord = getDictCCWord(currentResult.value(1));
            databaseQuery.bindValue(":right_word", rightWord.getWord());
            databaseQuery.bindValue(":right_gender", rightWord.getGender());
            databaseQuery.bindValue(":right_other", rightWord.getOptional());
            databaseQuery.bindValue(":category", currentResult.value(2));
            if (databaseQuery.exec()) {
                successfullyWrittenEntries++;
            } else {
                qDebug() << databaseQuery.lastError().text();
            }
        }
        if (everyXResult.rem == 0) {
            emit statusChanged(QString::number(currentLineNumber) + " of " + QString::number(lineCount) + " entries imported.\n" + QString::number(divisionResult.quot) + "% completed");
        }
    }

    databaseQuery.prepare("end transaction");
    databaseQuery.exec();

    qDebug() << metadata.value("languages") + ": " + QString::number(successfullyWrittenEntries) + " entries imported.";
    emit statusChanged(metadata.value("languages") + " dictionary with " + QString::number(successfullyWrittenEntries) + " entries successfully imported.");

}

DictCCWord DictCCImportWorker::getDictCCWord(QString rawWord)
{
    DictCCWord dictCCWord;
    QString realWord = rawWord;
    QRegExp genderMatcher("(\\{.+\\})");
    if (genderMatcher.indexIn(realWord) != -1) {
        QString genderString = genderMatcher.cap(1);
        genderString = genderString.replace("{", "(");
        genderString = genderString.replace("}", ")");
        dictCCWord.setGender(genderString);
        realWord = realWord.remove(genderMatcher);
    }
    QRegExp optionalMatcher("(\\[.+\\])");
    if (optionalMatcher.indexIn(realWord) != -1) {
        dictCCWord.setOptional(optionalMatcher.cap(1));
        realWord = realWord.remove(optionalMatcher);
    }
    dictCCWord.setWord(realWord.trimmed());
    return dictCCWord;
}

QString DictCCImportWorker::getTempDirectory()
{
    QString tempDirectoryString = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/harbour-dictionary";
    return getDirectory(tempDirectoryString);
}

QString DictCCImportWorker::getDirectory(const QString &directoryString)
{
    QString myDirectoryString = directoryString;
    QDir myDirectory(directoryString);
    if (!myDirectory.exists()) {
        qDebug() << "Creating directory " + directoryString;
        if (myDirectory.mkdir(directoryString)) {
            qDebug() << "Directory " + directoryString + " successfully created!";
        } else {
            qDebug() << "Error creating directory " + directoryString + "!";
            myDirectoryString = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }
    }
    return myDirectoryString;
}
