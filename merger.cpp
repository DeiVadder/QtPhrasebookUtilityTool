#include "merger.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QVector>
#include <QDebug>

Merger::Merger(QObject *parent) :QObject(parent)
{

}

bool Merger::Merge(const QList<QUrl> &sources, const QUrl &destination)
{

    QFile targetFile(destination.toLocalFile());

    if(!targetFile.exists() || !targetFile.open(QIODevice::ReadWrite)){
        m_error = tr("Target file could not be created");
        return false;
    }

    for(const QUrl &url : sources){
        bool ok = mergeTwoFiles(url.toLocalFile(), destination.toLocalFile());
        if(!ok)
            return false ;
    }

    return true;
}

bool Merger::mergeTwoFiles(const QString &fileA, const QString &fileB)
{
    //A into B

    QFile source(fileA);
    if(!source.open(QIODevice::ReadOnly)){
        m_error = tr("Could not open file \n%1").arg(fileA.splitRef(QChar('/')).last().toString());
        return false;
    }

    QSaveFile target(fileB);
    if(!target.open(QIODevice::ReadWrite)){
        m_error = tr("Could not open file \n%1").arg(fileB.splitRef(QChar('/')).last().toString());
        return  false;
    }

    QTextStream readStream(&source);
    QTextStream writeStream(&target);

    QString languageA, languageB;
    qint64/* max(0), */insert(0);
    bool stopInsert = false;
    bool aIsTsType(false), bIsTsType(false);

    //Preprocessing Target File
    while(!writeStream.atEnd()){
        QString readLine = writeStream.readLine();

        //Id Target Language
        if(languageB.isEmpty() && readLine.contains("language=")){
            QVector<QStringRef> refs = readLine.splitRef(QChar(' '));
            if(refs.size() > 1)
                languageB = refs.at(1).toString();
        }

        if(!bIsTsType && readLine.contains("<!DOCTYPE TS>")){
            bIsTsType = true;
        }

        if(readLine.contains("</TS>")){
            stopInsert = true;
        }
        if(!stopInsert)
            insert += (readLine.size() + 1);
    }

    if(!bIsTsType){
        m_error = tr("Target file is not a valid *.ts file!");
        target.cancelWriting();
        return false;
    }

    writeStream.reset();
    writeStream.seek(insert);

    //Reading and inserting
    bool firstContextFound(false);
    QString suffix = fileA.splitRef(QChar('/')).last().toString();

    while (!readStream.atEnd()){
        QString readLine = readStream.readLine();

        //Id Source Language
        if(languageA.isEmpty() && readLine.contains("language=")){
            QVector<QStringRef> refs = readLine.splitRef(QChar(' '));
            if(refs.size() > 1)
                languageA = refs.at(1).toString();
            if(languageA != languageB){
                m_error = tr("Source and Target languages do not match!");
                target.cancelWriting();
                return false;
            }
        }

        if(!aIsTsType && readLine.contains("<!DOCTYPE TS>")){
            aIsTsType = true;
        }

        //Detect first context keyword
        if(!firstContextFound && readLine.contains("<context>")){
            firstContextFound = true;
        }

        //Detect translation keyword and add type=vanished, if not there
        if(readLine.contains("</translation>") && !readLine.contains("type=\"vanished\""))
            readLine.replace("<translation>", "<translation type=\"vanished\">");

        //Attach suffix to the name property to prevent potential conflicts in the translation file
        if(readLine.contains("</name>"))
            readLine.replace("</name>", QString(" %1</name>").arg(suffix));

        if(firstContextFound){
            writeStream << readLine << endl;
            if(writeStream.status() != QTextStream::Ok){
                qDebug() << "Write error!" << readLine;
                m_error = tr("An error appeared during the writing process!");
                target.cancelWriting();
                return false;
            }
        }
    }

    if(!bIsTsType){
        m_error = tr("Source file is not a valid *.ts file!\n%1").arg(fileA.splitRef(QChar('/')).last().toString());
        target.cancelWriting();
        return false;
    }

    target.commit();
    return true;
}
