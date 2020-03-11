#include "phrase.h"

#include <QTextStream>

Phrase::Phrase()
{

}

Phrase::Phrase(const Phrase &phrase)
{
    m_source = phrase.m_source;
    m_target = phrase.m_target;
    m_definition = phrase.m_definition;
    m_oldSources = phrase.m_oldSources;
    m_translationType = phrase.m_translationType;
}

Phrase::Phrase(const QString &phraseText)
{
    QString subSec = subSection(phraseText,QString("source"));
    if(!subSec.isEmpty())
        m_source = extractInfo(subSec);

    subSec =subSection(phraseText, QString("target"));
    if(!subSec.isEmpty())
        m_target = extractInfo(subSec);

    m_translationType = extractType(phraseText);

    subSec =subSection(phraseText, QString("definition"));
    if(!subSec.isEmpty())
        m_definition = extractInfo(subSec);
}

Phrase::Phrase(const QString &context, const QString &definition)
    :  m_definition(definition)
{   
    QString subSec = subSection(context,QString("source"));
    if(!subSec.isEmpty())
        m_source = extractInfo(subSec);

    subSec =subSection(context, QString("translation"));
    if(!subSec.isEmpty())
        m_target = extractInfo(subSec);

    m_translationType = extractType(context);

    QRegExp oldSOurceReg("<oldsource>(.*)</oldsource>");
    oldSOurceReg.setMinimal(true);

    int indexOldSources(0);
    while(true){
        indexOldSources = oldSOurceReg.indexIn(context,indexOldSources);
        if(indexOldSources > 0)
            m_oldSources.append(Phrase(context.mid(indexOldSources, oldSOurceReg.matchedLength()),m_target, definition,type()));
        else
            break;
        indexOldSources += oldSOurceReg.matchedLength();
    }
}

Phrase::Phrase(const QString &source, const QString &target, const QString &definition, Type type)
    : m_source(source), m_target(target), m_definition(definition), m_translationType(type)
{

}

QString Phrase::subSection(const QString &input, const QString &tag)
{
    QString expStr = QString("<%1.*>(.*)</%1>").arg(tag);
    QRegExp r(expStr);
    r.setMinimal(true);
    int index = r.indexIn(input);
    if(index > 0){
        return  input.mid(index, r.matchedLength());
    }
    return  QString();
}

QString Phrase::extractInfo(const QString &line)
{
    QRegExp r  = QRegExp(">(.*)</");
    r.setMinimal(false);
    int index = r.indexIn(line);
    if (index > 0 /*&& r.matchedLength() >3*/){
        return line.mid(++index, r.matchedLength() -3);
    }

    return QString();
}

bool operator >(const Phrase &phraseA, const Phrase &phraseB)
{
    if(phraseA.m_definition == phraseB.m_definition)
        return  phraseA.m_source > phraseB.m_source;
    return phraseA.m_definition > phraseB.m_definition;
}

bool operator ==(const Phrase &phraseA, const Phrase &phraseB)
{
    //To filter dublicates, operator == does not requiere a check for the definition
    return  (phraseB.m_source == phraseA.m_source) && (phraseB.m_target == phraseA.m_target)/* && (phraseB.m_definition == phraseA.m_definition)*/;
}

Phrase &Phrase::operator=(const Phrase &phrase)
{
    if(this != &phrase){
        m_source = phrase.m_source;
        m_target = phrase.m_target;
        m_translationType = phrase.m_translationType;
        m_definition = phrase.m_definition;
        m_oldSources = phrase.m_oldSources;
    }
    return  *this;
}

Phrase::Type Phrase::extractType(const QString &context)
{
    QRegExp r("<translation type=\"(.*)\">");
    r.setMinimal(true);

    if(r.indexIn(context) >= 0 && r.capturedTexts().size() > 1){
        //Should have 2 matches, whole expression and the part between the two " that define the type
        //0 -> Fill capture with expression; 1 -> only the (.*) sub part
        QString type = r.capturedTexts().at(1);
        if(type == ("vanished"))
            return Vanished;
        if(type == ("unfinished"))
            return  Unfinished;
        if(type ==("obsolete"))
            return  Obsolete;
    }
    return  None;
}

QTextStream &operator<<(QTextStream &stream, const Phrase &phrase)
{
    if(phrase.isValid()){
        stream << "<phrase>\n\t<source>"
               << phrase.m_source
               << "</source>\n\t<target>"
               << phrase.m_target
               << "</target>\n\t<definition>"
               << phrase.m_definition
               << "</definition>\n</phrase>\n";
    }
    return stream;
}
