#ifndef MODEL_H
#define MODEL_H

#include <QAbstractListModel>
#include <QUrl>

class TranslationFile
{
public:
    TranslationFile(const QUrl &url);

    const QUrl &url() const;
    const QString &name() const;

    friend bool operator==(const TranslationFile &lhs, const TranslationFile &rhs){
        return  lhs.url() == rhs.url();
    }
private:
    QUrl m_url;
    QString m_fileName;
};

class Model : public QAbstractListModel
{
    Q_OBJECT

public:
    enum UrlRoles {
        UrlRole = Qt::UserRole +1
    };

    explicit Model(QObject *parent = nullptr);

    void addEntry(const QUrl &url, bool noDublicates = false);
    void removeRowsByIndex(const QModelIndexList &indexes);

//    // Header:
//    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;


private:
    QList<TranslationFile> m_files;

};

#endif // MODEL_H
