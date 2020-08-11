#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "merger.h"
#include "phrasebookmaker.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QThread>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(tr("Phrasebook Utility Tool"));
    qRegisterMetaType<QList<QUrl>>("QList<QUrl>");

    pMaker = new PhrasebookMaker();
    QThread *t = new QThread();
    pMaker->moveToThread(t);
    connect(qApp, &QCoreApplication::aboutToQuit, t, &QThread::quit);
    connect(t, &QThread::finished, t, &QThread::deleteLater);
    connect(t, &QThread::finished, pMaker, &PhrasebookMaker::deleteLater);

    connect(pMaker, &PhrasebookMaker::error,            this, &MainWindow::displayError);
    connect(pMaker, &PhrasebookMaker::progressMaximum,  ui->progressBar, &QProgressBar::setMaximum);
    connect(pMaker, &PhrasebookMaker::progressValue,    ui->progressBar, &QProgressBar::setValue);
    connect(pMaker, &PhrasebookMaker::success,          this, &MainWindow::displaySuccess);
    connect(pMaker, &PhrasebookMaker::newlyCreatedFiles,this, &MainWindow::addCreatedFiles);

    connect(this, &MainWindow::exportFilesToNewPhrasebooks, pMaker, &PhrasebookMaker::exportFilesToNewPhrasebooks);
    connect(this, &MainWindow::exportFilesToSingleNewPhrasebook, pMaker, &PhrasebookMaker::exportFilesToSingleNewPhrasebook);
    connect(this, &MainWindow::updatePhrasebookWithSources, pMaker, &PhrasebookMaker::updatePhrasebookFromFiles);
    connect(this, &MainWindow::patchTsFileFromPhrasebooks, pMaker, &PhrasebookMaker::patchTsFileFromPhrasebooks);
    connect(this, &MainWindow::exportFileToNewCsv, pMaker, &PhrasebookMaker::exportTsFileAsCsv);

    t->start();

    connect(ui->actionAdd_Source_File, &QAction::triggered, this, &MainWindow::addSource);
    connect(ui->actionAdd_Targer_File, &QAction::triggered, this, &MainWindow::addTarget);
    connect(ui->actionRemove_Selected_File, &QAction::triggered, this, &MainWindow::removeSelected);

    connect(ui->actionMerge_Into_Target, &QAction::triggered, this, &MainWindow::mergeFiles);
    connect(ui->actionPatch_Ts_File, &QAction::triggered, this, &MainWindow::patchTsFile);

    connect(ui->actionExport_To_Target, &QAction::triggered, this, &MainWindow::exportToSinglePhrasebook);
    connect(ui->actionExport_To_Phrasebook, &QAction::triggered, this, &MainWindow::exportToPhrasebooks);
    connect(ui->actionUpdate_Phrasebook, &QAction::triggered, this, &MainWindow::updatePhrasebook);

    connect(ui->actionExport_as_CSV, &QAction::triggered, this, &MainWindow::exportToCsv);

    ui->listViewSourceFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);

    ui->listViewSourceFiles->setModel(&m_sourceModel);
    ui->listViewDestinationFile->setModel(&m_targetModel);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addSource()
{
    const QList<QUrl> selectedFiles = QFileDialog::getOpenFileUrls(this, tr("Select your sources"),QUrl(),tr("Translation or Phrasebook (*.ts *.qph)") );

    for(const QUrl &url : selectedFiles){
        m_sourceModel.addEntry(url);
    }
}

void MainWindow::addTarget()
{
    const QList<QUrl> urls = QFileDialog::getOpenFileUrls();

    for(const QUrl &url : urls)
        m_targetModel.addEntry(url);
}

void MainWindow::removeSelected()
{
    QListView *ListView = nullptr;

    if(ui->listViewSourceFiles->hasFocus())
        ListView = ui->listViewSourceFiles;
    else if(ui->listViewDestinationFile->hasFocus())
        ListView= ui->listViewDestinationFile;

    if(!ListView || ListView->selectionModel()->selectedIndexes().size() == 0){
        QMessageBox::warning(this, tr("Removing selection"), tr("No files selected!"));
        return;
    }

    const QModelIndexList list = ListView->selectionModel()->selectedIndexes();
    Model *m = qobject_cast<Model*>(ListView->model());
    m->removeRowsByIndex(list);
}

void MainWindow::addCreatedFiles(const QList<QUrl> &files)
{
    for(const QUrl & url : files)
        m_targetModel.addEntry(url,true);
}

void MainWindow::mergeFiles()
{
    //Sources
    const QList<QUrl> sources = fetchSources();
    if(sources.isEmpty())
        return;

    QUrl target;
    QModelIndexList selectionTo = ui->listViewDestinationFile->selectionModel()->selectedIndexes();
    if(selectionTo.isEmpty()){
        target = QFileDialog::getOpenFileUrl();
        if(!target.isValid())
            return;
    } else {
        target = m_targetModel.data(selectionTo.first(),Model::UrlRole).toUrl();
    }

    Merger m;
    bool ok = m.Merge(sources, target);
    if(!ok){
        QMessageBox::critical(this, tr("Failure"), tr("Merging failed with the reason: \n%1").arg(m.error()));
    } else {
        QMessageBox::information(this, tr("Success"), tr("Successfully merged all *.ts files into one"));
    }
}

void MainWindow::exportToCsv()
{
    //Allowed is one or no target to be selected
    //When one target move all ts entries into that one csv file
    //When no  target simply create 1 csv file parallel to the source

    const QList<QUrl> sources = fetchSources();
    if(sources.isEmpty())
        return;


    QUrl target;
    QModelIndexList selectionTo = ui->listViewDestinationFile->selectionModel()->selectedIndexes();
    if(!selectionTo.isEmpty() && selectionTo.size() == 1){
        target = m_targetModel.data(selectionTo.first(),Model::UrlRole).toUrl();
    } else {
        QMessageBox::warning(this, tr("Selected Target Files"), tr("Please select at most one target file"));
        return;
    }

    emit exportFileToNewCsv(sources, target);
}

void MainWindow::exportToSinglePhrasebook()
{
    //Sources
    const QList<QUrl> sources = fetchSources();
    if(sources.isEmpty())
        return;

    QModelIndexList selectionTo = ui->listViewDestinationFile->selectionModel()->selectedIndexes();
    QUrl target;
    if(!selectionTo.isEmpty()){
        target = m_targetModel.data(selectionTo.first(),Model::UrlRole).toUrl();
    } else {
        target = QFileDialog::getSaveFileUrl(nullptr,tr("Select target file"), QString(),tr("Phrasebook (*.qph)"));
    }
    if(!target.isValid())
        return;

    QString sourceLanguage = requestSourceLanguage();

    if(!sourceLanguage.isEmpty())
        emit exportFilesToSingleNewPhrasebook(sources,target, sourceLanguage);
}

void MainWindow::exportToPhrasebooks()
{
    //Sources
    const QList<QUrl> sources = fetchSources();
    if(sources.isEmpty())
        return;

//    PhrasebookMaker pMaker;
//    connect(&pMaker, &PhrasebookMaker::error, this, &MainWindow::displayError);
//    connect(&pMaker, &PhrasebookMaker::progressMaximum, ui->progressBar, &QProgressBar::setMaximum);
//    connect(&pMaker, &PhrasebookMaker::progressValue, ui->progressBar, &QProgressBar::setValue);

    QString srcLang = requestSourceLanguage();

    if(!srcLang.isEmpty())
        emit exportFilesToNewPhrasebooks(sources, srcLang);
}

void MainWindow::updatePhrasebook()
{
    //Sources
    const QList<QUrl> sources = fetchSources();
    if(sources.isEmpty())
        return;

    //Target
    QModelIndexList selectionTo = ui->listViewDestinationFile->selectionModel()->selectedIndexes();
    QUrl target;
    if(!selectionTo.isEmpty()){
        target = m_targetModel.data(selectionTo.first(),Model::UrlRole).toUrl();
    } else {
        target = QFileDialog::getSaveFileUrl(nullptr,tr("Select target file"), QString(),tr("Phrasebook (*.qph)"));

        //add it to listview, in case it's not already on the target list
        if(target.isValid())
            addCreatedFiles(QList<QUrl>{target});
    }
    if(!target.isValid()){
        QMessageBox::information(nullptr, tr("Target phrasebook"), tr("Please select a target phrasebook to update"));
        return;
    }

    QString sourceLanguage = requestSourceLanguage();

    if(!sourceLanguage.isEmpty()){
        emit updatePhrasebookWithSources(sources,target,sourceLanguage);
    } else {
        QMessageBox::information(nullptr, tr("Source language"), tr("Please specify the source language"));
    }
}

void MainWindow::patchTsFile()
{
    //Sources
    const QList<QUrl> sources = fetchSources();
    if(sources.isEmpty())
        return;

    //Target
    QModelIndexList selectionTo = ui->listViewDestinationFile->selectionModel()->selectedIndexes();
    QUrl target;
    if(!selectionTo.isEmpty()){
        target = m_targetModel.data(selectionTo.first(),Model::UrlRole).toUrl();
    } else {
        target = QFileDialog::getSaveFileUrl(nullptr,tr("Select target file"), QString(),tr("Translation file (*.ts)"));
    }
    if(!target.isValid()){
        QMessageBox::information(nullptr, tr("Translation file"), tr("Please select a translation file to update"));
        return;
    } else {
        //add in case it's not in target list
        addCreatedFiles(QList<QUrl>{target});
    }

    emit patchTsFileFromPhrasebooks(sources,target);
}

void MainWindow::displayError(const QString &error)
{
    QMessageBox::warning(this, "Error", error);
}

void MainWindow::displaySuccess()
{
    QMessageBox::information(this, tr("Success"), tr("Export was successful"));
}

QString MainWindow::requestSourceLanguage()
{
    QStringList languages;
    for(int i(2); i < static_cast<int>(QLocale::LastLanguage); i++) {
        languages.append( QLocale::languageToString(static_cast<QLocale::Language>(i)));
    }

    const QStringList refLanguages = languages;
    std::sort(languages.begin(), languages.end());

    int index = languages.indexOf(QLocale::languageToString(static_cast<QLocale::Language>(QLocale::system().language())));
    QString selection = QInputDialog::getItem(nullptr,tr("Missing information"),tr("Select source language"),languages,index,false);

    if(selection.isNull() || selection.isEmpty()){
        displayError(tr("No source language selected"));
        return QString();
    }

    index = refLanguages.indexOf(selection);
    if(index < 0){
        displayError(tr("Invalid language"));
        return QString();
    }

    return QLocale(static_cast<QLocale::Language>(index + 2)).name();
}

QList<QUrl> MainWindow::fetchSources()
{
    QList<QUrl> sources;

    QModelIndexList selection = ui->listViewSourceFiles->selectionModel()->selectedIndexes();
    if(selection.isEmpty()){
        //Assume all Files
        ui->listViewSourceFiles->selectAll();
        selection = ui->listViewSourceFiles->selectionModel()->selectedIndexes();

        if(selection.isEmpty()){
            QMessageBox::warning(this, tr("No files selected"), tr("Please select at least one source file"));
            return sources;
        }
    }

    for(const QModelIndex &index : selection){
        sources.append(m_sourceModel.data(index, Model::UrlRole).toUrl());
    }
    return sources;
}

