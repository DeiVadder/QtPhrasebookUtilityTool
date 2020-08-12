#include "phrasebookmaker.h"
#include "phrase.h"

#include "csvio.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QRegularExpression>

PhrasebookMaker::PhrasebookMaker(QObject *parent) : QObject(parent)
{

}

constexpr bool TargetLanguagesHaveToBeTheSame = true;

const QString PatternSourceLanguage(QStringLiteral("(?<=sourcelanguage=\")(.*?)(?=\")"));
const QString PatternLanguage(QStringLiteral("(?<=\\ language=\")(.*?)(?=\">)"));

void PhrasebookMaker::exportFilesToSingleNewPhrasebook(const QList<QUrl> &sources, const QUrl &destination, const QString &sourceLanguage)
{
    init(sources, sourceLanguage);

    //Assumption, sources are *.ts files, destionation is a already existing *.qph file
    if(!preprocessTsSources(sources, TargetLanguagesHaveToBeTheSame))
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

        if(!preprocessTsSources(QList<QUrl>{url}))
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

void PhrasebookMaker::exportTsFileAsCsv(const QList<QUrl> &sources, const QUrl &file, const QString &sourceLanguage)
{
    if(sources.isEmpty()){
        emit error("No source files selected!");
        return;
    }
    preprocessTsSources(sources, false);

    m_value = 0;
    m_max = sources.size() + 1;
    emit progressMaximum(m_max);
    emit progressValue(0);

    QString fileName;
    if(file.isValid() && !file.isEmpty())
        fileName = file.toLocalFile();
    else{
        //If file is empty or invalid, construct one from the name of the source file
        fileName = sources.first().toLocalFile().replace(QStringLiteral(".ts"),QStringLiteral(".csv"));
        emit newlyCreatedFiles(QList<QUrl>{QUrl::fromLocalFile(fileName)});
    }

    //If sources is only 1 file, use exportAsCsv, else extract all Qlocals, check that there are no dublicats and than call the overload
    bool ok = false;
    if(sources.size() == 1){
        const QUrl &source = sources.first();

        QVector<Phrase> phrases = parseSingleTsFile(source);
        emit progressValue(++m_value);
        ok = CsvIo::exportAsCsv(phrases,
                                QLocale(sourceLanguage),
                                getFromFirstLines<QLocale>(source, PatternLanguage),
                                fileName);
    } else {
        QVector<QLocale> locales;
        QVector<QVector<Phrase> > phrases;
        for(const QUrl & url : sources){
            const QLocale l = getFromFirstLines<QLocale>(url, PatternLanguage);
            if(locales.contains(l)){
                emit error("Multiple sources have the same language");
                return;
            }
            locales.append(l);
            phrases.append(parseSingleTsFile(url));
            emit progressValue(++m_value);
        }

        ok = CsvIo::exportAsCsv(phrases,QLocale(sourceLanguage),locales,fileName);
    }

    emit progressValue(m_max);

    if(ok)
        emit success();
    else{
        emit error("Failed to export into csv file!");
        emit progressValue(0);
    }
}

void PhrasebookMaker::updatePhrasebookFromFiles(const QList<QUrl> &sources, const QUrl &targetPhrasebook, const QString &sourceLanguage)
{
    // - ID if sources are TS or QPH or CSV and if they are mixed
    // - Parse target phrasebook and extract phrases
    // - extract phrases from sources
    // - patch phrases from target phrasebook
    // - write new file at target location, with the patched phrases

    FileMode fileMode(FileMode::FileModeUndefined);


    //Identify source and target language of the targetPhrasebook
    const QString languageSource = getFromFirstLines<QString>(targetPhrasebook, PatternSourceLanguage);
    const QString languageTarget = getFromFirstLines<QString>(targetPhrasebook, PatternLanguage);

    if(languageTarget.isEmpty()){
        emit error("Targeted language could not be determined!");
        return;
    }

    auto checkTargetLanguage = [this,&languageTarget](const QUrl &url)->bool{
        //If this Returns false, emit error message and exit from PhrasebookMaker::updatePhrasebookFromFiles
        QString language = getFromFirstLines<QLocale>(url, PatternLanguage).name();
        if(languageTarget != language){
            emit error(tr("Targeted languages do not match!"));
            return false;
        }
        return true;
    };

    for(const QUrl &url : sources){

        FileMode cFileMode(FileMode::FileModeUndefined);
        if(isTsFile(url)){
//----------Is *.ts file
            cFileMode = FileMode::FileModeTS;
            if(!checkTargetLanguage(url))
                return;
        } else if(isQphFile(url)){
//----------Is *.qph file
            cFileMode = FileMode::FileModeQPH;
            QString language = getFromFirstLines<QLocale>(url,PatternSourceLanguage).name();

            if(languageSource != language){
                emit error(tr("Source languages do not match!"));
                return;
            }
            if(!checkTargetLanguage(url))
                return;

        } else if(isCsvFile(url)){
//----------is CSV file
            cFileMode = FileMode::FileModeCsv;

            //Check, if the cvs file has the same source language as the target phrasebook
            QString context = getFromFirstLines<QString>(url, QStringLiteral("(?<=\\()%1(?=\\):)").arg(sourceLanguage));
            if(context != sourceLanguage){
                //Source Language missmatch
                emit error(tr("Source languages do not match!"));
                return;
            }

            //Check if csv File has the targetLanguage as  a column
            if(!hasPatternDefined(url, QStringLiteral("(?<=\\()%1(?=\\):)").arg(languageTarget)/*context.isEmpty()*/)){
                //TargetLanguage not found
                emit error(tr("CSV file does not contain an entry for the targeted language!"));
                return;
            }
        }

        if(fileMode == FileMode::FileModeUndefined && cFileMode != FileMode::FileModeUndefined)
            fileMode = cFileMode;

        if(cFileMode == FileMode::FileModeUndefined){
            emit error("The file type could not be determined!");
            return;
        } else {
            if(fileMode != cFileMode){
                emit error("Error: Mixed different source!");
                return;
            }
        }
    }

    //error handling
    if(fileMode == FileMode::FileModeUndefined){
        emit error("The file type could not be determined!");
        return;
    }

    //Now the actual patching

    //Extract Phrases from target and add phrases when not existend
    QVector<Phrase> existingPhrases = phrasesFromPhrasebook(targetPhrasebook);
    auto getPhrasesFrom = [this, &targetPhrasebook, &languageTarget](const QUrl &source, FileMode mode)->QVector<Phrase>{
        switch (mode) {
        case FileMode::FileModeTS: return parseSingleTsFile(source, targetPhrasebook.fileName().split(".").first());
        case FileMode::FileModeQPH: return phrasesFromPhrasebook(source);
        case FileMode::FileModeCsv: return CsvIo::importFromCsv(source, QLocale(languageTarget));
        case FileMode::FileModeUndefined:{
            Q_ASSERT_X(false,"Getting Phrases from source", "Unidentified source");
            return  QVector<Phrase>();
        }
        }
        return  QVector<Phrase>();
    };
    for(const QUrl &source : sources){
        const QVector<Phrase> phrasesFromSourceFile = getPhrasesFrom(source, fileMode);

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

void PhrasebookMaker::patchTsFromFiles(const QList<QUrl> &sourcesQph, const QUrl &targetTsFile)
{
    /*
    Steps:
    - Check sources are qph or CSV files
    - check target *ts lanugage matches all languages inside the source files
    - Parse targetTS extract all phrases, subset untranslated phrases
    - Parse source and extract all phrases and check each against untranslated phrases
        When translation found, add it, add type tag "unfinished" move it to all phrases
        continue untill subset empty or all sources parsed
    */

    //Check target is ts file and extract targetlanguage

    if(!isTsFile(targetTsFile)){
        emit error(tr("Wrong format for the target file!"));
        return;
    }

    const QString targetLanguage = getFromFirstLines<QString>(targetTsFile, PatternLanguage);
    if(targetLanguage.isEmpty()){
        emit error(tr("Could not id targeted language!"));
        return;
    }

    //Check if the sources are of the same type and match to the targeted ts file
    FileMode mode{FileMode::FileModeUndefined};

    for(const QUrl &url : sourcesQph){
        //Detect if current File is a Phrasebook file or a CSV file or none of those
        if(isQphFile(url)){
            if(mode != FileMode::FileModeQPH && mode != FileMode::FileModeUndefined){
                emit error("Please do not mix different types of sources!");
                return;
            }
            mode = FileMode::FileModeQPH;
            //Check if targeted languages match
            const QString targetLanguageQph = getFromFirstLines<QString>(url, PatternLanguage);
            if(targetLanguageQph != targetLanguage){
                emit error(tr("Phrasebook targets a different language compared to the *.ts file!"));
                return;
            }
        } else if(isCsvFile(url)){
            if(mode != FileMode::FileModeCsv && mode != FileMode::FileModeUndefined){
                emit error("Please do not mix different types of sources!");
                return;
            }
            mode = FileMode::FileModeCsv;
            //Check if csv File has the targetLanguage as  a column
            if(!hasPatternDefined(url, QStringLiteral("(?<=\\()%1(?=\\):)").arg(targetLanguage)/*context.isEmpty()*/)){
                emit error(tr("CSV file does not contain the targeted language inside the *.ts file!"));
                return;
            }
        } else {
            emit error(tr("Unsupported source file found!"));
            return;
        }
    }

    //Checks for target done create the Phrase list update section
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
        const QVector<Phrase> qphPhrases = mode == FileMode::FileModeQPH ? phrasesFromPhrasebook(url,false): CsvIo::importFromCsv(url, targetLanguage);

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

//The progress bar and current progress needs some love to better represent how the different operations are progressing
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

bool PhrasebookMaker::preprocessTsSources(const QList<QUrl> &sources, bool targetLanguagesAreTheSame)
{
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

        if(!isTsFile(url)){
            emit error(tr("Invalid file format"));
            return  false;
        }

        if(!hasPatternDefined(url,PatternLanguage)){
            emit error(tr("*ts file has no language defined"));
            return false;
        }

        if(targetLanguagesAreTheSame){
            if(m_targetLanguage.isEmpty()){
                m_targetLanguage = getFromFirstLines<QString>(url, PatternLanguage);
            }else {
                if(m_targetLanguage != getFromFirstLines<QString>(url, PatternLanguage)){
                    emit error("Selected files target different languages!");
                    return false;
                }
            }
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

bool PhrasebookMaker::isQphFile(const QUrl &url)
{
    return hasPatternDefined(url, QStringLiteral("<!DOCTYPE QPH"));
}

bool PhrasebookMaker::isTsFile(const QUrl &url)
{
    return hasPatternDefined(url, QStringLiteral("<!DOCTYPE TS>"));
}

bool PhrasebookMaker::isCsvFile(const QUrl &url)
{
    //Patterns looks for the existance of the translation tabe between to brackets followed by a `:`e.g: (en_US):
    return hasPatternDefined(url, QStringLiteral("(?<=\\()(.*?)(?=\\):)"));
}

bool PhrasebookMaker::hasPatternDefined(const QUrl &url, const QString &pattern)
{
    return !getFromFirstLines<QString>(url, pattern).isEmpty();
}

#include <QDebug>
QString PhrasebookMaker::findExactMatch(const QString &source, const QString &pattern)
{
    QRegularExpression regEx(pattern);
    QRegularExpressionMatch match = regEx.match(source,0,QRegularExpression::PartialPreferCompleteMatch);

    qDebug() << match.capturedTexts();

    if(match.hasMatch()){
        return  match.captured(match.lastCapturedIndex());
    }
    return QString();
}
