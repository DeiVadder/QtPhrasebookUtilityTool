#ifndef CSVIO_H
#define CSVIO_H

#include <QLocale>

#include "phrase.h"

class CsvIo
{
public:
    CsvIo() = default;

    static bool exportAsCsv(QVector<Phrase> &phrasesToExport, const QLocale &sourceLanguage,
                     const QLocale &targetLanguage,
                     const QString &fileName,
                     bool simplified = true);

    static bool exportAsCsv(QVector<QVector<Phrase> > &phrasesListsToExport,
                            const QLocale &sourceLanguage,
                            const QVector<QLocale> &targetLanguages,
                            const QString &fileName,
                            bool simplified = true);

    static QVector<Phrase> importFromCsv(const QUrl &fileName, const QLocale &targetLanguage);
};

#endif // CSVIO_H
