#ifndef MERGER_H
#define MERGER_H

#include <QUrl>
#include <QObject>

class Merger : public QObject
{
    Q_OBJECT
public:
    explicit Merger(QObject*parent = nullptr);

    bool Merge(const QList<QUrl> &sources, const QUrl&destination);
    inline const QString &error(){return  m_error;}

private:
    bool mergeTwoFiles(const QString &fileA, const QString &fileB);

private:
    QString m_error;
};

#endif // MERGER_H
