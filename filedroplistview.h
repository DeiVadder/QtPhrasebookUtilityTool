#ifndef FILEDROPLISTVIEW_H
#define FILEDROPLISTVIEW_H

#include <QListView>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>

#include <QDebug>

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
        qDebug() << Q_FUNC_INFO;
        emit urlsDropedIntoView(e->mimeData()->urls());
        e->acceptProposedAction();
    }

    virtual void dragMoveEvent(QDragMoveEvent *e) override
    {
        if(e->mimeData()->hasUrls()){
            e->accept();
//            ev->acceptProposedAction();
        }
    }

    virtual void dragEnterEvent(QDragEnterEvent *ev) override
    {
        qDebug() << Q_FUNC_INFO << acceptDrops() << ev->mimeData()->hasUrls();
        if(ev->mimeData()->hasUrls()){
            ev->accept();
//            ev->acceptProposedAction();
        }
    }
};

#endif // FILEDROPLISTVIEW_H
