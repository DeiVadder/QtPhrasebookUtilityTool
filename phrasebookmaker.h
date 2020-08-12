#ifndef PHRASEBOOKMAKER_H
#define PHRASEBOOKMAKER_H

#include <QObject>
#include <QUrl>
#include <QFile>
#include <QTextStream>

class Phrase;
class PhrasebookMaker : public QObject
{
    Q_OBJECT
    enum class FileMode{
        FileModeUndefined =-1,
        FileModeTS = 0,
        FileModeQPH,
        FileModeCsv
    };

public:
    explicit PhrasebookMaker(QObject *parent = nullptr);

    void exportFilesToSingleNewPhrasebook(const QList<QUrl> &sources, const QUrl &destination, const QString &sourceLanguage);
    void exportFilesToNewPhrasebooks(const QList<QUrl> &sources, const QString &sourceLanguage);
    void exportTsFileAsCsv(const QList<QUrl> &sources, const QUrl &file, const QString &sourceLanguage);

    void updatePhrasebookFromFiles(const QList<QUrl> &sources, const QUrl &targetPhrasebook, const QString &sourceLanguage);
    void patchTsFromFiles(const QList<QUrl> &sourcesQph, const QUrl &targetTsFile);

signals:
    void error(const QString &error);

    void progressMaximum(int maximum);
    void progressValue(int value);

    void success();

    void newlyCreatedFiles(const QList<QUrl> &url);

private:
    void init(const QList<QUrl> &sources, const QString sourceLanguage);
    bool preprocessTsSources(const QList<QUrl> &sources, bool targetLanguagesAreTheSame = false);

    QVector<Phrase> phrasesFromPhrasebook(const QUrl &url, bool emitSignal = true);
    QVector<Phrase> parseSingleTsFile(const QUrl &url, const QString &defaultName = QString());

    [[nodiscard]] bool isQphFile(const QUrl &url);
    [[nodiscard]] bool isTsFile(const QUrl &url);
    [[nodiscard]] bool isCsvFile(const QUrl &url);
    [[nodiscard]] bool hasPatternDefined(const QUrl &url, const QString &pattern);

    //might be more usefull to limit this QString
    template<typename T >
    [[nodiscard]] T getFromFirstLines(const QUrl &url, const QString &pattern) {
        T t;
        QFile file(url.toLocalFile());
        if(!file.exists()|| !file.open(QIODevice::ReadOnly))
            return t;
        QString context;
        QTextStream s(&file);
        for(int i(0); i < 3; i++)
            context.append(s.readLine());
        return T{findExactMatch(context, pattern)};
    }


    [[nodiscard]]QString findExactMatch(const QString &source, const QString &pattern);

protected:
    QString m_sourceLanguage, m_targetLanguage;

    int m_max = 0;
    int m_value = 0;

};

#endif // PHRASEBOOKMAKER_H
