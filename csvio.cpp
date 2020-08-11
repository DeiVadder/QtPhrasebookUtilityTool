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
#include <QSaveFile>
#include <QTextStream>

#include <QDebug>

const QString replaceSeperator(QStringLiteral("_Semicolon_"));

bool CsvIo::exportAsCsv(QVector<Phrase> &phrasesToExport,
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
              << QStringLiteral("Original text:") << seperator
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

bool CsvIo::exportAsCsv(QVector<QVector<Phrase> > &phrasesListsToExport, const QVector<QLocale> &targetLanguages, const QString &fileName, bool simplified)
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
              << QStringLiteral("Original text:") << seperator;
    for(const QLocale &targetLanguage : targetLanguages)
              outStream << QStringLiteral("Translation (%1):").arg(targetLanguage.name());
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
                if(&otherPhraseVector == &phrasesToExport)
                    continue;
                outStream << seperator;
                for(Phrase & otherPhrase : otherPhraseVector){
                    if(otherPhrase.source() == phrase.source() && otherPhrase.definition() == phrase.definition()){
                        outStream << (phrase.targetLiteral().replace(QStringLiteral(";"), replaceSeperator));
                        otherPhraseVector.removeOne(otherPhrase);
                        break;
                    }
                }
            }
        }
        outStream << Qt::endl;
        //Need to remove exported phrases from current list
        phrasesToExport.clear();
    }

    return fileOut.commit();
}

QVector<Phrase> CsvIo::importFromCsv(const QString &fileName)
{
    QVector<Phrase> phrases;

    QFile file(fileName);
    if(!file.exists() || !file.open(QIODevice::ReadOnly))
        return  phrases;

    QTextStream inStream(&file);
    QString line = inStream.readLine();
    constexpr QChar seperator(';');

    QString definition;
    auto splitRef = line.splitRef(seperator);
    if(splitRef.size() > 2)
        definition = line.splitRef(seperator).first().toString();

    const bool missingColumn = definition.isEmpty();
    auto stringRefToReconstructedString = [](const QStringRef &ref)->QString{
        QString str = ref.toString();
        str = str.replace(replaceSeperator, QStringLiteral(";"));
        str = str.replace(QStringLiteral("\\n"), QChar('\n'));
        return  str;
    };
    while(!inStream.atEnd()) {
        line = inStream.readLine();
        const QVector<QStringRef> split = line.splitRef(seperator);
        if(split.size() > (missingColumn ? 1 : 2)){
            const QString source = stringRefToReconstructedString(missingColumn ? split.first() : split.at(1));
            const QString target = stringRefToReconstructedString(split.last());
            const Phrase phrase(source,target,definition,Phrase::Finished);
            phrases.append(phrase);
        }
    }

    return phrases;
}
