#include "phrasebookmaker.h"
#include "phrase.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

PhrasebookMaker::PhrasebookMaker(QObject *parent) : QObject(parent)
{

}

void PhrasebookMaker::exportFilesToSingleNewPhrasebook(const QList<QUrl> &sources, const QUrl &destination, const QString &sourceLanguage)
{
    init(sources, sourceLanguage);

    //Assumption, sources are *.ts files, destionation is a already existing *.qph file
    if(!preprocessSources(sources))
        return;

    if(!destination.isValid()){
        emit error(tr("No target file specified!"));
        return;
    }

    QString defaultName = destination.fileName();
    defaultName.replace(".ts", ".qph");

    QSaveFile newPhrasebook(destination.toLocalFile().replace(destination.fileName(), defaultName));
    defaultName = defaultName.split('.').first();

    if(newPhrasebook.open(QIODevice::WriteOnly)) {
        QTextStream writeStream(&newPhrasebook);

        //Header
        writeStream << "<!DOCTYPE QPH>" << endl;
        writeStream << QString("<QPH sourcelanguage=\"%1\" language=\"%2\">").arg(m_sourceLanguage).arg(m_targetLanguage) << endl;

        //Actual read
        QVector<Phrase> uniquePhrases;
        for( const QUrl &url : sources){
             const QVector<Phrase> phrases = parseSingleTsFile(url,defaultName);

             for(const Phrase &p : phrases){
                 if(!uniquePhrases.contains(p))
                     uniquePhrases.append(p);

                 for(const Phrase & subset : p.oldSources()){
                     if(!uniquePhrases.contains(subset))
                         uniquePhrases.append(subset);
                 }
             }
        }

        for(const Phrase &p : qAsConst(uniquePhrases)){
            writeStream << p;
        }

        writeStream << "</QPH>" << endl;
        newPhrasebook.commit();
    }

    emit progressValue(m_max);

    emit success();
    emit newlyCreatedFiles(QList<QUrl>{destination});
}

void PhrasebookMaker::exportFilesToNewPhrasebooks(const QList<QUrl> &sources, const QString &sourceLanguage)
{
    init(sources, sourceLanguage);

    QList<QUrl> nUrls;
    for( const QUrl &url : sources){

        if(!preprocessSources(QList<QUrl>{url}))
            return;

        /*  Steps:
            - Read source file
            - Extract it into QVector of Phrase
            - Merge Phrases & SubPhrases, exclude dublicates
            - write into new file
        */

        //Prepare write & read
        QString defaultName = url.fileName();
        defaultName.replace(".ts", ".qph");
        nUrls.append(QUrl::fromLocalFile(defaultName));

        QSaveFile newPhrasebook(url.toLocalFile().replace(url.fileName(), defaultName));
        defaultName = defaultName.split('.').first();

        if(!newPhrasebook.open(QIODevice::WriteOnly)) {
            emit error(tr("Could not create file"));
            return;
        }

        //Read
        const QVector<Phrase> phrases = parseSingleTsFile(url, defaultName);

        //Entangle & Filter
        QVector<Phrase> uniquePhrases;
        for(const Phrase &p : phrases){
            if(!uniquePhrases.contains(p))
                uniquePhrases.append(p);
            for(const Phrase & subset : p.oldSources())
                if(!uniquePhrases.contains(subset))
                    uniquePhrases.append(subset);
        }

        //actual writing

        //Header
        QTextStream writeStream(&newPhrasebook);
        writeStream << "<!DOCTYPE QPH>" << endl;
        writeStream << QString("<QPH sourcelanguage=\"%1\" language=\"%2\">").arg(m_sourceLanguage).arg(m_targetLanguage) << endl;

        for(const Phrase &p : qAsConst(uniquePhrases)){
            writeStream << p;
        }

        writeStream << "</QPH>" << endl;
        newPhrasebook.commit();
    }
    emit progressValue(m_max);
    emit success();
    emit newlyCreatedFiles(nUrls);
}

const int FileModeUndefined(-1);
const int FileModeTS(0);
const int FileModeQPH(1);

void PhrasebookMaker::updatePhrasebookFromFiles(const QList<QUrl> &sources, const QUrl &targetPhrasebook, const QString &sourceLanguage)
{
    // - ID if sources are TS or QPH and if they are mixed
    // - Parse target phrasebook and extract phrases
    // - extract phrases from sources
    // - patch phrases from target phrasebook
    // - write new file at target location, with the patched phrases

    int fileMode(FileModeUndefined);
    QString languageSource, languageTarget;
    for(const QUrl &url : sources){
        QFile readFile(url.toLocalFile());
        if(readFile.open(QIODevice::ReadOnly)){
            QString content = readFile.readAll();
            readFile.close();

            int cFileMode(FileModeUndefined);

            if(content.contains("<!DOCTYPE TS>")){
                cFileMode = FileModeTS;
            } else if(content.contains("<!DOCTYPE QPH>")){
                cFileMode = FileModeQPH;
            }
            if(fileMode == FileModeUndefined && cFileMode != FileModeUndefined)
                fileMode = cFileMode;

            if(cFileMode == FileModeUndefined){
                emit error("The file type could not be determand!");
                return;
            } else {
                if(fileMode != cFileMode){
                    emit error("Error: Mixed *.ts and *.qph files!");
                    return;
                }
            }

            //Language always follows the DOCTYPE
            int index(0);
            if(cFileMode == FileModeQPH){
                //not ts file check for source language

                QRegExp regEx("<QPH sourcelanguage=\"(.*)\"");
                regEx.setMinimal(true);
                index = regEx.indexIn(content);
                if(index <0){
                    emit error("Parse error of target file!");
                    return ;
                }

                QString language = content.mid(index + 21, 5);
                if(languageSource.isEmpty())
                    languageSource = language;
                else if(languageSource != language){
                    emit error(tr("Source languages do not match!"));
                    return;
                }
            }

            QRegExp regEx("language=\"(.*)\"");
            regEx.setMinimal(true);
            index = regEx.indexIn(content, index + 21);
            if(index <0){
                emit error("Parse error of target file!");
                return ;
            }
            QString language = content.mid(index + 10, 5);
            if(languageTarget.isEmpty())
                languageTarget = language;
            else if(languageTarget != language){
                emit error(tr("Targeted languages do not match!"));
                return;
            }
        }
    }

    //error handling
    if(fileMode == FileModeUndefined){
        emit error("The file type could not be determand!");
        return;
    }

    if(languageTarget.isEmpty()){
        emit error("Targeted language could not be determind!");
        return;
    }

    if(languageSource.isEmpty())
        languageSource = sourceLanguage;
    //Determined source and target languages need to match the target file languages
    QFile readFile(targetPhrasebook.toLocalFile());
    if(readFile.open(QIODevice::ReadOnly)){
        QString content = readFile.readAll();
        readFile.close();

        if(!content.contains("<!DOCTYPE QPH>")){
            emit error(tr("Target file is not a phrasebook!"));
            return;
        }

        //Source language of target
        QRegExp regEx("<QPH sourcelanguage=\"(.*)\"");
        regEx.setMinimal(true);
        int index = regEx.indexIn(content);
        if(index <0){
            emit error("Parse error of target file!");
            return ;
        }
        QString language = content.mid(index + 21, 5);
        if(languageSource.isEmpty())
            languageSource = language;
        else if(languageSource != language){
            emit error(tr("Source languages do not match!"));
            return;
        }

        //target language of target
        regEx.setPattern("language=\"(.*)\"");
        index = regEx.indexIn(content, index + 21);
        if(index <0){
            emit error("Parse error of target file!");
            return ;
        }
        language = content.mid(index + 10, 5);
        if(language != languageTarget){
            emit error(tr("Targeted language does not match with the others!"));
            return;
        }
    }

    //Now the actual patching

    //Extract Phrases from target and add phrases when not existend
    QVector<Phrase> existingPhrases = phrasesFromPhrasebook(targetPhrasebook);
    for(const QUrl &url : sources){
        const QVector<Phrase> phrasesFromSourceFile = fileMode == FileModeQPH ?
                    phrasesFromPhrasebook(url) :
                    parseSingleTsFile(url, targetPhrasebook.fileName().split(".").first());

        for(const Phrase &p : phrasesFromSourceFile){
            if(!existingPhrases.contains(p)){
                existingPhrases.append(p);
            }
            //else : Source and translation already exists -> do nothing

            for(const Phrase & subset : p.oldSources())
                //Subsets will be empty for FileModeQPH
                if(!existingPhrases.contains(subset))
                    existingPhrases.append(subset);
        }
    }

    //Save to HD
    QSaveFile newPhrasebook(targetPhrasebook.toLocalFile());
    if(newPhrasebook.open(QIODevice::WriteOnly)) {
        QTextStream writeStream(&newPhrasebook);

        //Header
        writeStream << "<!DOCTYPE QPH>" << endl;
        writeStream << QString("<QPH sourcelanguage=\"%1\" language=\"%2\">").arg(languageSource).arg(languageTarget) << endl;

        for(const Phrase &p : qAsConst(existingPhrases)){
            writeStream << p;
        }

        writeStream << "</QPH>" << endl;
        newPhrasebook.commit();
    }

    emit progressValue(m_max);
    emit success();
}

void PhrasebookMaker::patchTsFileFromPhrasebooks(const QList<QUrl> &sourcesQph, const QUrl &targetTsFile)
{
    /*
    Steps:
    - Check sources are qph files
    - check target *ts lanugage matches all languages inside the phrasebook files
    - Parse targetTS extract all phrases, subset untranslated phrases
    - Parse source qph extract all phrases and check each against untranslated phrases
        When translation found, add it, add type tag "unfinished" move it to all phrases
        continue untill subset empty or all sources parsed
    */

    //Id TS target languagse
    bool isTsFile(false);
    QString targetLanguage;
    QFile f(targetTsFile.toLocalFile());
    if(f.open(QIODevice::ReadOnly)){
        QTextStream readStream(&f);

        while(!readStream.atEnd()){
            QString line = readStream.readLine();

            if(!isTsFile && line.contains("<!DOCTYPE TS>")){
                isTsFile = true;
            }

            if(line.contains("language=\"")){
                const QString searchStr("language=\"");
                int i= line.indexOf(searchStr);
                targetLanguage = line.mid(i + searchStr.length(), 5);
            }
            if(isTsFile && !targetLanguage.isEmpty())
                //All needed information acquiered: break from while -> next file in list
                break;
        }
        f.close();
    }
    if(targetLanguage.isEmpty()){
        emit error(tr("Could not id targeted language!"));
        return;
    }

    //Checks done update section
    QVector<Phrase> phrasesFromTs = parseSingleTsFile(targetTsFile);
    QVector<Phrase> notTranslatedPhrases;
    QVector<Phrase> nowTranslatedPhrases;

    for(const Phrase &p : qAsConst(phrasesFromTs))
            if(!p.hasTranslation() && (p.type() != Phrase::Vanished && p.type() != Phrase::Obsolete))
                notTranslatedPhrases << p;

    if(notTranslatedPhrases.isEmpty()){
        emit error(tr("No untranslated phrases in the ts file!"));
        return;
    }


    m_max =sourcesQph.size() * notTranslatedPhrases.size();
    m_value = 0;
    emit progressMaximum(m_max);
    emit progressValue(0);
    for(const QUrl &url : sourcesQph){
        if(!url.fileName().endsWith(QStringLiteral(".qph"))){
            emit error(tr("Please select only phrasebook files"));
            return;
        }

        QString targetLangPb;
        QFile readFile(url.toLocalFile());
        if(readFile.open(QIODevice::ReadOnly)){
            QString data = readFile.readAll();
            QRegExp regEx("<QPH sourcelanguage=\"(.*)\"");
            regEx.setMinimal(true);
            int index = regEx.indexIn(data);
            if(index <0){
                emit error("Parse error of target file!");
                return ;
            }

            regEx.setPattern("language=\"(.*)\"");
            index = regEx.indexIn(data, index + 21);
            if(index < 0){
                emit error("Parse error of target file!");
                return;
            }

            targetLangPb = data.mid(index + 10, 5);
            if(targetLangPb != targetLanguage){
                emit error(tr("Phrasebook targets a different language compared to the *.ts file!"));
                return;
            }
        } else {
            emit error(tr("Could not open phrasebook!"));
            return;
        }
    }

    for(const QUrl &url : sourcesQph){
        const QVector<Phrase> qphPhrases = phrasesFromPhrasebook(url,false);

        for( Phrase &tsPhrase : notTranslatedPhrases){
            for(const Phrase &qphPhrase : qphPhrases){
                if(qphPhrase.source() == tsPhrase.source()){
                    tsPhrase.setTranslation(qphPhrase.target());
                    nowTranslatedPhrases << tsPhrase;
                    notTranslatedPhrases.removeOne(tsPhrase);
                }
            }
        }

        if(notTranslatedPhrases.isEmpty()){
            //all translations done -> break early
            break;
        }
    }
    emit progressValue(m_max);

    if(nowTranslatedPhrases.isEmpty()){
        emit error(tr("No new translations were found!"));
        return;
    }

    // a Ts file can be much more complex and contain more information than the Phrase class can currently map to
    //Therefore we will read the whole ts file again into memory and parse it line by line
    //Search for the <message> </message> block, check if no translation -> search recently found translation and alter the translation string
    //and then write the whole context block in one go.

    QFile readTsFile(targetTsFile.toLocalFile());
    QSaveFile writeTsFile(targetTsFile.toLocalFile());
    if(readTsFile.open(QIODevice::ReadOnly) && writeTsFile.open(QIODevice::WriteOnly)){
        QTextStream readStream(&readTsFile);
        QTextStream writeStream(&writeTsFile);

        QString msgBlock;

        //Read and write header
        bool firstContextFound = false;
        while(!firstContextFound){
            QString line = readStream.readLine();
            if(line.contains("<context>"))
                firstContextFound = true;
            else
                writeStream << line << endl;
        }

        //reset
        readStream.seek(0);
        QString readData =  readStream.readAll();

        //Close read file otherwise QSaveFile will fail on commit
        readTsFile.close();


        //Parse and write remainder

        QRegExp contextReg("<context>(.*)</context>");
        contextReg.setMinimal(true);

        int index (0);
        while (true) {
            index = contextReg.indexIn(readData, index);
            if(index >= 0){
                QString orgContext = readData.mid(index, contextReg.matchedLength());
                QString toWriteContext = orgContext;

                QRegExp messageReg("<message>(.*)</message>");
                messageReg.setMinimal(true);

                int indexMessage(0);
                while(true){
                    indexMessage = messageReg.indexIn(orgContext,indexMessage);
                    if(indexMessage > 0){
                        QString messageString = orgContext.mid(indexMessage, messageReg.matchedLength());
                        Phrase p(messageString);
                        if(!p.hasTranslation()){
                            //No translation, check source text against new translations
                            for(const Phrase &ntp : qAsConst(nowTranslatedPhrases)){
                                if(p.source() == ntp.source()){
                                    QString orgMsg = messageString;
                                    messageString.replace(QRegExp ("<translation(.*)>(.*)</translation>"),
                                                          QStringLiteral("<translation type=\"unfinished\">%1</translation>").arg(ntp.target()));
                                    toWriteContext.replace(orgMsg, messageString);
                                }
                            }
                        }
                    }else{
                        writeStream << toWriteContext << endl;
                        //breaks from while loop
                        break;
                    }
                    indexMessage += messageReg.matchedLength();
                }
            } else {
                break;
            }
            index += contextReg.matchedLength();
        }

        //Write closer
        writeStream << QStringLiteral("</TS>") << endl;

        if(!writeTsFile.commit()){
            emit error(tr("Could not save changes"));
            return;
        }
    } else {
        error(tr("Ts file could not be read or written to"));
        return;
    }

    emit success();
}

bool PhrasebookMaker::checkLanguages(const QUrl &url)
{
    QString sourceLangPb;
    QString targetLangPb;
    QFile readFile(url.toLocalFile());
    if(!readFile.fileName().endsWith(".qph")){
        emit error(tr("Not supported file format!"));
        return false;
    }
    if(!readFile.exists() || !readFile.open(QIODevice::ReadOnly)){
        emit error(tr("Target file could not be opened!"));
        return false;
    }
    QString data = readFile.readAll();
    QRegExp regEx("<QPH sourcelanguage=\"(.*)\"");
    regEx.setMinimal(true);
    int index = regEx.indexIn(data);
    if(index <0){
        emit error("Parse error of target file!");
        return false;
    }
    
    sourceLangPb = data.mid(index + 21,5);
    regEx.setPattern("language=\"(.*)\"");
    index = regEx.indexIn(data, index + 21);
    if(index < 0){
        emit error("Parseerror of target file!");
        return false;
    }
    
    targetLangPb = data.mid(index + 10, 5);

    if(targetLangPb != m_targetLanguage || m_sourceLanguage != sourceLangPb){
        emit error(tr("Language missmatch"));
        return false;
    }
    return  true;
}

void PhrasebookMaker::init(const QList<QUrl> &sources, const QString sourceLanguage)
{
    m_sourceLanguage = sourceLanguage;

    m_value = 0;
    emit progressValue(0);

    for(const QUrl &url :sources ){
        QFile f(url.toLocalFile());
        m_max +=f.size() / 1028;
    }
    emit progressMaximum(m_max);
}

bool PhrasebookMaker::preprocessSources(const QList<QUrl> &sources)
{
    m_targetLanguage.clear();

    if(sources.isEmpty()){
        emit error(tr("No files selected"));
        return false;
    }

    for(const QUrl &url :sources ){
        if(!url.isValid()){
            emit error(tr("Invalid file path!"));
            return false;
        }

        if(!url.fileName().endsWith(".ts")){
            emit error(tr("Invalid file format"));
            return  false;
        }

        QFile readFile(url.toLocalFile());

        if(readFile.size() > 209715200) {
            emit error(tr("File size exeeds reasonable limit of 200 mb"));
            return false;
        }

        if(!readFile.exists() || !readFile.open(QIODevice::ReadOnly)){
            emit error(tr("File does not exist or could not be opend!"));
            return false;
        }

        QTextStream stream(&readFile);
        bool isTsFile(false);
        QString language;

        while(!stream.atEnd()){
            QString line = stream.readLine();

            if(!isTsFile && line.contains("<!DOCTYPE TS>")){
                isTsFile = true;
            }

            if(line.contains("language=\"")){
                const QString searchStr("language=\"");
                int i= line.indexOf(searchStr);
                language = line.mid(i + searchStr.length(), 5);
                if(m_targetLanguage.isEmpty())
                    m_targetLanguage = language;
                else if(m_targetLanguage != language){
                    emit error("Selected files target different languages!");
                    return false;
                }

            }
            if(isTsFile && !language.isEmpty())
                //All needed information acquiered: break from while -> next file in list
                break;
        }

        if(!isTsFile){
            emit error(tr("Invalid file format"));
            return  false;
        }

        if(language.isEmpty()){
            emit error(tr("*ts file has no language defined"));
            return false;
        }
    }
    return true;
}

QVector<Phrase> PhrasebookMaker::phrasesFromPhrasebook(const QUrl &url, bool emitSignal)
{
    QVector<Phrase> phrases;
    QFile file(url.toLocalFile());

    if(file.exists() && file.open(QIODevice::ReadOnly)){
        QTextStream readStream(&file);
        QString data = readStream.readAll();
        if(readStream.status() == QTextStream::Ok){
            QRegExp phraseReg("<phrase>(.*)</phrase>");
            phraseReg.setMinimal(true);

            int index (0);
            int size(0);

            while (true) {
                index = phraseReg.indexIn(data, index);
                if(index >= 0){
                    QString section = data.mid(index, phraseReg.matchedLength());
                    size += section.size();
                    phrases.append(Phrase(section));
                } else {
                    break;
                }
                index += phraseReg.matchedLength();
                if(emitSignal)
                    emit progressValue(m_value + size / 1028);
            }
            if(emitSignal){
                m_value += file.size() /1028;
                emit progressValue(m_value);
            }
        }
    }
    if(phrases.isEmpty())
        emit error("Phrases could not be extracted!");

    return phrases;
}

QVector<Phrase> PhrasebookMaker::parseSingleTsFile(const QUrl &url, const QString &defaultName)
{
    QVector<Phrase> phrases;

    QFile readFile(url.toLocalFile());
    readFile.open(QIODevice::ReadOnly);

    QTextStream readStream(&readFile);

    QString data = readStream.readAll();

    QRegExp contextReg("<context>(.*)</context>");
    contextReg.setMinimal(true);

    int index (0);
    int size(0);
    while (true) {
        index = contextReg.indexIn(data, index);
        if(index >= 0){
            QString section = data.mid(index, contextReg.matchedLength());
            size += section.size();
            QString name = QString(" ") + Phrase::infoFromSection(section,"name");

            QRegExp messageReg("<message>(.*)</message>");
            messageReg.setMinimal(true);

            int indexMessage(0);
            while(true){
                indexMessage = messageReg.indexIn(section,indexMessage);
                if(indexMessage > 0)
                    phrases.append(Phrase(section.mid(indexMessage, messageReg.matchedLength()), defaultName + name));
                else
                    break;
                indexMessage += messageReg.matchedLength();
            }
        } else {
            break;
        }
        index += contextReg.matchedLength();
        emit progressValue(m_value + size / 1028);
    }
    m_value += readFile.size() /1028;
    emit progressValue(m_value);

    return phrases;
}
