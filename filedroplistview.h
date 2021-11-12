#ifndef FILEDROPLISTVIEW_H
#define FILEDROPLISTVIEW_H

#include <QListView>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>

class FileDropListView : public QListView
{
    Q_OBJECT
public:
    explicit FileDropListView(QWidget *parent) : QListView(parent)
    {
        setAcceptDrops(true);
    }

signals:
    void urlsDropedIntoView(QList<QUrl> urls);

protected:
    virtual void dropEvent(QDropEvent *e) override
    {
        QList<QUrl> urls = e->mimeData()->urls();
        for(const QUrl &url : urls){
            if(!url.fileName().endsWith(QStringLiteral(".csv")) &&
                    !url.fileName().endsWith(QStringLiteral(".ts")) &&
                    !url.fileName().endsWith(QStringLiteral(".qph"))){
                urls.removeOne(url);
            }
        }
        if(!urls.isEmpty())
            emit urlsDropedIntoView(urls);
        e->acceptProposedAction();
    }

    virtual void dragMoveEvent(QDragMoveEvent *e) override
    {
        if(e->mimeData()->hasUrls()){
            e->acceptProposedAction();
        }
    }

    virtual void dragEnterEvent(QDragEnterEvent *ev) override
    {
        if(ev->mimeData()->hasUrls()){
            ev->acceptProposedAction();
        }
    }
};

#endif // FILEDROPLISTVIEW_H
