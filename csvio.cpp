#include "csvio.h"
//#include "algorithm"
//The csv file is supposed to have/create the following structure:
//Simple export, discard any information on source file or translation type, no dublicates and alphabetically ordered
/*
 * Text in source file; Language 1;
*/
//Advanced export, order by  Source file (name) than alphabetically, only include, when not vanished
/*
 *Name source file; text (source); Language 1;
*/

#include <QFile>
#include <QUrl>
#include <QSaveFile>
#include <QTextStream>

#include <QDebug>

const QString replaceSeperator(QStringLiteral("_Semicolon_"));

bool CsvIo::exportAsCsv(QVector<Phrase> &phrasesToExport,
                        const QLocale &sourceLanguage,
                        const QLocale &targetLanguage,
                        const QString &fileName,
                        const bool simplified)
{
    QSaveFile fileOut(fileName);
    if(!fileOut.open(QIODevice::WriteOnly))
        //Target file could not be created/open
        return false;

    if(simplified){
        // Filter out all entries, that are markt as vanished or obsolete
        auto iterator = phrasesToExport.begin();
        while(iterator < phrasesToExport.end()){
            if(iterator->type() == Phrase::Vanished || iterator->type() == Phrase::Obsolete)
                iterator = phrasesToExport.erase(iterator);
            else
                iterator++;
        }
    }
    std::sort(phrasesToExport.begin(), phrasesToExport.end());

    QTextStream outStream(&fileOut);

    constexpr QChar seperator(';');

    //Header
    outStream << QStringLiteral("Source file:") << seperator
              << QStringLiteral("Original text (%1):").arg(sourceLanguage.name()) << seperator
              << QStringLiteral("Translation (%1):").arg(targetLanguage.name())
              << Qt::endl;
    //Body
    for(Phrase &phrase : phrasesToExport){
        outStream << (phrase.definitionLiteral().replace(QStringLiteral(";"), replaceSeperator) )<< seperator
                  << (phrase.sourceLiteral().replace(QStringLiteral(";"), replaceSeperator)) << seperator
                  << (phrase.targetLiteral().replace(QStringLiteral(";"), replaceSeperator))
                  << Qt::endl;
    }

    return fileOut.commit();
}

bool CsvIo::exportAsCsv(QVector<QVector<Phrase> > &phrasesListsToExport, const QLocale &sourceLanguage, const QVector<QLocale> &targetLanguages, const QString &fileName, bool simplified)
{
    QSaveFile fileOut(fileName);
    if(!fileOut.open(QIODevice::WriteOnly))
        //Target file could not be created/open
        return false;

    if(simplified){
        // Filter out all entries, that are markt as vanished or obsolete
        for(QVector <Phrase> & phrasesToExport : phrasesListsToExport){
            auto iterator = phrasesToExport.begin();
            while(iterator < phrasesToExport.end()){
                if(iterator->type() == Phrase::Vanished || iterator->type() == Phrase::Obsolete)
                    iterator = phrasesToExport.erase(iterator);
                else
                    iterator++;
            }
            std::sort(phrasesToExport.begin(), phrasesToExport.end());
        }
    }

    QTextStream outStream(&fileOut);

    constexpr QChar seperator(';');

    //Header
    outStream << QStringLiteral("Source file:") << seperator
              << QStringLiteral("Original text (%1):").arg(sourceLanguage.name());
    for(const QLocale &targetLanguage : targetLanguages)
              outStream << seperator << QStringLiteral("Translation (%1):").arg(targetLanguage.name());
    outStream << Qt::endl;


    //We can't assume that each ts file has each entry -> we have to run through all vectors each time and check if available
    //But if we find a corresponding entry, we will remove it from the vector, reducing the search time for the next entry

    for(int i (0); i< phrasesListsToExport.size(); i++){
        QVector<Phrase> &phrasesToExport = phrasesListsToExport[i];

        //Body
        for(Phrase &phrase : phrasesToExport){
            outStream << (phrase.definitionLiteral().replace(QStringLiteral(";"), replaceSeperator) )<< seperator
                      << (phrase.sourceLiteral().replace(QStringLiteral(";"), replaceSeperator)) << seperator
                      << (phrase.targetLiteral().replace(QStringLiteral(";"), replaceSeperator));
            for(QVector<Phrase> &otherPhraseVector : phrasesListsToExport) {
                if(&otherPhraseVector != &phrasesToExport) {
                    outStream << seperator;
                    for(Phrase & otherPhrase : otherPhraseVector){
                        if(otherPhrase.source() == phrase.source() && otherPhrase.definition() == phrase.definition()){
                            outStream << (otherPhrase.targetLiteral().replace(QStringLiteral(";"), replaceSeperator));
                            otherPhraseVector.removeOne(otherPhrase);
                            break;
                        }
                    }
                }
            }
            outStream << Qt::endl;
        }
        //Need to remove exported phrases from current list
        phrasesToExport.clear();
    }

    return fileOut.commit();
}

QVector<Phrase> CsvIo::importFromCsv(const QUrl &fileName, const QLocale &targetLanguage)
{
    QVector<Phrase> phrases;

    QFile file(fileName.toLocalFile());
    if(!file.exists() || !file.open(QIODevice::ReadOnly))
        return  phrases;

    //First we read the header and identify, if we have the sourceFile defined (definition) and at what index the targeted Language is
    QTextStream inStream(&file);
    QString line = inStream.readLine();
    constexpr QChar seperator(';');

    bool hasDefinition{false};
    int languageIndex{-1};

    auto split = line.split(seperator);
    if(split.first().contains(QStringLiteral("Source file:")))
        hasDefinition = true;
    const QString tLanguage = QStringLiteral("(%1):").arg(targetLanguage.name());
    for(int i(0), j(split.size()); i < j; i++){
        if(split.at(i).contains(tLanguage)){
            languageIndex = i;
            break;
        }
    }

    if(Q_UNLIKELY(languageIndex == -1))
        //should not be possible, we check for this beforhand in phrasebookmaker
        return phrases;

    QString definition;

    auto stringRefToReconstructedString = [](const QStringRef &ref)->QString{
        QString str = ref.toString();
        str = str.replace(replaceSeperator, QStringLiteral(";"));
        str = str.replace(QStringLiteral("\\n"), QChar('\n'));
        return  str;
    };

    while(!inStream.atEnd()) {
        line = inStream.readLine();
        const QVector<QStringRef> split = line.splitRef(seperator);
        if(hasDefinition)
            definition = split.first().toString();

        if(split.size() > languageIndex){
            const QString target = stringRefToReconstructedString(split.at(languageIndex));
            if(!target.isEmpty()){
                const QString source = stringRefToReconstructedString(hasDefinition ? split.at(1) : split.first());
                const Phrase phrase(source,target,definition,Phrase::Finished);
                phrases.append(phrase);
            }
        }
    }

    return phrases;
}
