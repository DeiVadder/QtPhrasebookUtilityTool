#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "model.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class PhrasebookMaker;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void addSource();
    void addTarget();
    void removeSelected();

    void addCreatedFiles(const QList<QUrl> &files);

    void mergeFiles();
    void exportToCsv();
    void exportToSinglePhrasebook();
    void exportToPhrasebooks();
    void updatePhrasebook();
    void patchTsFile();

    void displayError(const QString &error);
    void displaySuccess();

    QString requestSourceLanguage();

    void onDoubleClickedSources(const QModelIndex &index){onDoubleClicked(m_sourceModel,index); }
    void onDoubleClickedTarget(const QModelIndex &index){onDoubleClicked(m_targetModel, index);}
    void onDoubleClicked(const Model &model, const QModelIndex &index);
signals:
    void exportFileToNewCsv(const QList<QUrl> &sources, const QUrl &target, const QString &sourceLanguage);
    void exportFilesToNewPhrasebooks(const QList<QUrl> &sources, const QString &srcLang);
    void exportFilesToSingleNewPhrasebook(const QList<QUrl> &sources, const QUrl &destination, const QString &sourceLanguage);
    void updatePhrasebookWithSources(const QList<QUrl> &sourcesTs, const QUrl &targetPhrasebook, const QString &sourceLanguage);
    void patchTsFileFromPhrasebooks(const QList<QUrl> &sourcesQph, const QUrl &targetTsFile);

private:
    QList<QUrl> fetchSources();

private:
    Ui::MainWindow *ui;


    Model m_sourceModel;
    Model m_targetModel;
    PhrasebookMaker *pMaker;

};
#endif // MAINWINDOW_H
