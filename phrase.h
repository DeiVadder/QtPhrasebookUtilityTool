#ifndef PHRASE_H
#define PHRASE_H

#include <QString>
#include <QVector>

//#include <QTextStream>

class QTextStream;
class Phrase
{
public:

    enum Type {None, Unfinished, Finished, Vanished, Obsolete };

    Phrase();
    Phrase(const Phrase &phrase);
    Phrase(const QString &phraseText);
    Phrase(const QString &context, const QString &definition);
    Phrase(const QString &source, const QString &target, const QString &definition, Type type);

    static QString subSection(const QString &input, const QString &tag);
    static QString extractInfo(const QString &line);

    static QString infoFromSection(const QString &input, const QString &tag){return extractInfo(subSection(input, tag));}

    inline bool isValid() const {return  !m_source.isEmpty() && !m_target.isEmpty();}
    inline bool hasTranslation() const {return  !m_target.isEmpty();}

    inline Type type() const {return  m_translationType;}

    inline const QString &source() const {return m_source;}
    inline const QString &target() const {return m_target;}
    inline const QString &definition() const {return  m_definition;}
    inline const QVector<Phrase> oldSources() const {return  m_oldSources;}

    inline void setTranslation(const QString &translation){m_target = translation;}

    friend bool operator ==(const Phrase &phraseA, const Phrase &phraseB);
    friend bool operator !=(const Phrase &phraseA, const Phrase &phraseB) { return !(phraseA == phraseB);}
    friend bool operator >(const Phrase &phraseA, const Phrase &phraseB);
    Phrase &operator=(const Phrase &phrase);

    friend QTextStream &operator<<(QTextStream &stream, const Phrase &phrase);

private:
    Type extractType(const QString &context);

protected:

    QString m_source;
    QString m_target;
    QString m_definition;

    QVector<Phrase> m_oldSources;

    Type m_translationType = None;
};

#endif // PHRASE_H
