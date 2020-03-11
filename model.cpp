#include "model.h"

TranslationFile::TranslationFile(const QUrl &url)
    : m_url(url), m_fileName(url.fileName())
{
}

const QUrl &TranslationFile::url() const
{
    return  m_url;
}

const QString &TranslationFile::name() const
{
    return  m_fileName;
}

Model::Model(QObject *parent)
    : QAbstractListModel(parent)
{
}

void Model::addEntry(const QUrl &url, bool noDublicates)
{

    beginInsertRows(QModelIndex() , rowCount(), rowCount());
    TranslationFile t(url);
    if(noDublicates){
        if(!m_files.contains(t))
            m_files << t;
    } else {
        m_files << t;
    }

    endInsertRows();
}

#include <QDebug>
void Model::removeRowsByIndex(const QModelIndexList &indexes)
{
    if(indexes.isEmpty())
        return;

    QVector<int> index;
    for(const QModelIndex &modelIndex : indexes)
        index.append(modelIndex.row());
    std::sort(index.begin(), index.end(),[](int a, int b)->bool{return  a > b;});

    beginRemoveRows(QModelIndex(),index.last(),index.first());
    for(int i : index){
        m_files.removeAt(i);
    }
    endRemoveRows();
}

//QVariant model::headerData(int section, Qt::Orientation orientation, int role) const
//{
//    // FIXME: Implement me!
//}

int Model::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_files.count();
}

QVariant Model::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const TranslationFile &file = m_files[index.row()];
    if(role == Qt::DisplayRole)
        return  file.name();

    if(role == UrlRole)
        return  file.url();

    return QVariant();
}
