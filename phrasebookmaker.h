#ifndef PHRASEBOOKMAKER_H
#define PHRASEBOOKMAKER_H

#include <QObject>
#include <QUrl>

class Phrase;
class PhrasebookMaker : public QObject
{
    Q_OBJECT
public:
    explicit PhrasebookMaker(QObject *parent = nullptr);

    void exportFilesToSingleNewPhrasebook(const QList<QUrl> &sources, const QUrl &destination, const QString &sourceLanguage);
    void exportFilesToNewPhrasebooks(const QList<QUrl> &sources, const QString &sourceLanguage);

    void updatePhrasebookFromFiles(const QList<QUrl> &sources, const QUrl &targetPhrasebook, const QString &sourceLanguage);
    void patchTsFileFromPhrasebooks(const QList<QUrl> &sourcesQph, const QUrl &targetTsFile);

signals:
    void error(const QString &error);

    void progressMaximum(int maximum);
    void progressValue(int value);

    void success();

    void newlyCreatedFiles(const QList<QUrl> &url);

private:
    bool checkLanguages(const QUrl &url);
    void init(const QList<QUrl> &sources, const QString sourceLanguage);
    bool preprocessSources(const QList<QUrl> &sources);

    QVector<Phrase> phrasesFromPhrasebook(const QUrl &url, bool emitSignal = true);
    QVector<Phrase> parseSingleTsFile(const QUrl &url, const QString &defaultName = QString());

protected:
    QString m_targetLanguage;
    QString m_sourceLanguage;

    int m_max = 0;
    int m_value = 0;

};

#endif // PHRASEBOOKMAKER_H
