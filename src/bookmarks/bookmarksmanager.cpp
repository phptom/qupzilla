/* ============================================================
* QupZilla - WebKit based browser
* Copyright (C) 2010-2011  nowrep
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* ============================================================ */
#include "mainapplication.h"
#include "bookmarksmanager.h"
#include "ui_bookmarksmanager.h"
#include "qupzilla.h"
#include "locationbar.h"
#include "webview.h"
#include "bookmarkstoolbar.h"
#include "tabwidget.h"
#include "bookmarksmodel.h"
#include "qtwin.h"

//Won't be bad idea to rewrite bookmarks access via bookmarksmodel

BookmarksManager::BookmarksManager(QupZilla* mainClass, QWidget* parent) :
    QWidget(parent)
    ,m_isRefreshing(false)
    ,ui(new Ui::BookmarksManager)
    ,p_QupZilla(mainClass)
    ,m_bookmarksModel(mApp->bookmarksModel())
{
    ui->setupUi(this);
    //CENTER on scren
    const QRect screen = QApplication::desktop()->screenGeometry();
    const QRect &size = QWidget::geometry();
    QWidget::move( (screen.width()-size.width())/2, (screen.height()-size.height())/2 );

#ifdef Q_WS_WIN
    if (QtWin::isCompositionEnabled()) {
        QtWin::extendFrameIntoClientArea(this);
        ui->gridLayout->setContentsMargins(0, 0, 0, 0);
    }
#endif

    connect(ui->deleteB, SIGNAL(clicked()), this, SLOT(deleteItem()));
    connect(ui->close, SIGNAL(clicked(QAbstractButton*)), this, SLOT(hide()));
    connect(ui->bookmarksTree, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(itemChanged(QTreeWidgetItem*)));
    connect(ui->addFolder, SIGNAL(clicked()), this, SLOT(addFolder()));
    connect(ui->bookmarksTree, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(contextMenuRequested(const QPoint &)));
    connect(ui->bookmarksTree, SIGNAL(itemControlClicked(QTreeWidgetItem*)), this, SLOT(itemControlClicked(QTreeWidgetItem*)));

    connect(m_bookmarksModel, SIGNAL(bookmarkAdded(BookmarksModel::Bookmark)), this, SLOT(addBookmark(BookmarksModel::Bookmark)));
    connect(m_bookmarksModel, SIGNAL(bookmarkDeleted(BookmarksModel::Bookmark)), this, SLOT(removeBookmark(BookmarksModel::Bookmark)));
    connect(m_bookmarksModel, SIGNAL(folderAdded(QString)), this, SLOT(addFolder(QString)));
    connect(m_bookmarksModel, SIGNAL(folderDeleted(QString)), this, SLOT(removeFolder(QString)));
    connect(m_bookmarksModel, SIGNAL(bookmarkEdited(BookmarksModel::Bookmark,BookmarksModel::Bookmark)), this, SLOT(bookmarkEdited(BookmarksModel::Bookmark,BookmarksModel::Bookmark)));

    //QTimer::singleShot(0, this, SLOT(refreshTable()));
}

QupZilla* BookmarksManager::getQupZilla()
{
    if (!p_QupZilla)
        p_QupZilla = mApp->getWindow();
    return p_QupZilla;
}

void BookmarksManager::setMainWindow(QupZilla* window)
{
    if (window)
        p_QupZilla = window;
}

void BookmarksManager::addFolder()
{
    QString text = QInputDialog::getText(this, tr("Add new folder"), tr("Choose name for new bookmark folder: "));
    if (text.isEmpty())
        return;

    m_bookmarksModel->createFolder(text);
}

void BookmarksManager::itemChanged(QTreeWidgetItem* item)
{
    if (!item || m_isRefreshing)
        return;

    QString name = item->text(0);
    QUrl url = QUrl(item->text(1));
    int id = item->whatsThis(1).toInt();

    delete item;
    m_bookmarksModel->editBookmark(id, name, url, "");
}

void BookmarksManager::itemControlClicked(QTreeWidgetItem* item)
{
    if (!item || item->text(1).isEmpty())
        return;
    getQupZilla()->tabWidget()->addView(QUrl(item->text(1)));
}

void BookmarksManager::loadInNewTab()
{
    if (QAction* action = qobject_cast<QAction*>(sender()))
        getQupZilla()->tabWidget()->addView(action->data().toUrl(), tr("New Tab"), TabWidget::NewNotSelectedTab);
}

void BookmarksManager::deleteItem()
{
    QTreeWidgetItem* item = ui->bookmarksTree->currentItem();
    if (!item)
        return;

    if (item->text(1).isEmpty()) { // Delete folder
        QString folder = item->text(0);
        m_bookmarksModel->removeFolder(folder);
        return;
    }

    int id = item->whatsThis(1).toInt();
    m_bookmarksModel->removeBookmark(id);
}

void BookmarksManager::addBookmark(WebView* view)
{
    insertBookmark(view->url(), view->title());
}

void BookmarksManager::moveBookmark()
{
    QTreeWidgetItem* item = ui->bookmarksTree->currentItem();
    if (!item)
        return;
    if (QAction* action = qobject_cast<QAction*>(sender())) {
        m_bookmarksModel->editBookmark(item->whatsThis(1).toInt(), item->text(0), QUrl(), action->data().toString());
    }
}

void BookmarksManager::contextMenuRequested(const QPoint &position)
{
    if (!ui->bookmarksTree->itemAt(position))
        return;
    QString link = ui->bookmarksTree->itemAt(position)->text(1);
    if (link.isEmpty())
        return;

    QMenu menu;
    menu.addAction(tr("Open link in actual &tab"), getQupZilla(), SLOT(loadActionUrl()))->setData(link);
    menu.addAction(tr("Open link in &new tab"), this, SLOT(loadInNewTab()))->setData(link);
    menu.addSeparator();

    QMenu moveMenu;
    moveMenu.setTitle(tr("Move bookmark to &folder"));
    moveMenu.addAction(QIcon(":icons/other/unsortedbookmarks.png"), tr("Unsorted Bookmarks"), this, SLOT(moveBookmark()))->setData("unsorted");
    moveMenu.addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Bookmarks In Menu"), this, SLOT(moveBookmark()))->setData("bookmarksMenu");
    moveMenu.addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Bookmarks In ToolBar"), this, SLOT(moveBookmark()))->setData("bookmarksToolbar");
    QSqlQuery query;
    query.exec("SELECT name FROM folders");
    while(query.next())
        moveMenu.addAction(style()->standardIcon(QStyle::SP_DirIcon), query.value(0).toString(), this, SLOT(moveBookmark()))->setData(query.value(0).toString());
    menu.addMenu(&moveMenu);

    menu.addSeparator();
    menu.addAction(tr("&Close"), this, SLOT(close()));

    //Prevent choosing first option with double rightclick
    QPoint pos = QCursor::pos();
    QPoint p(pos.x(), pos.y()+1);
    menu.exec(p);
}

void BookmarksManager::refreshTable()
{
    m_isRefreshing = true;
    ui->bookmarksTree->setUpdatesEnabled(false);
    ui->bookmarksTree->clear();

    QSqlQuery query;
    QTreeWidgetItem* newItem = new QTreeWidgetItem(ui->bookmarksTree);
    newItem->setText(0, tr("Bookmarks In Menu"));
    newItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    ui->bookmarksTree->addTopLevelItem(newItem);

    newItem = new QTreeWidgetItem(ui->bookmarksTree);
    newItem->setText(0, tr("Bookmarks In ToolBar"));
    newItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    ui->bookmarksTree->addTopLevelItem(newItem);

    query.exec("SELECT name FROM folders");
    while(query.next()) {
        newItem = new QTreeWidgetItem(ui->bookmarksTree);
        newItem->setText(0, query.value(0).toString());
        newItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        ui->bookmarksTree->addTopLevelItem(newItem);
    }

    query.exec("SELECT title, url, id, folder FROM bookmarks");
    while(query.next()) {
        QString title = query.value(0).toString();
        QUrl url = query.value(1).toUrl();
        int id = query.value(2).toInt();
        QString folder = query.value(3).toString();
        QTreeWidgetItem* item;
        if (folder == "bookmarksMenu")
            folder = tr("Bookmarks In Menu");
        if (folder == "bookmarksToolbar")
            folder = tr("Bookmarks In ToolBar");

        if (folder != "unsorted") {
            QList<QTreeWidgetItem*> findParent = ui->bookmarksTree->findItems(folder, 0);
            if (findParent.count() == 1) {
                item = new QTreeWidgetItem(findParent.at(0));
            }else{
                QTreeWidgetItem* newParent = new QTreeWidgetItem(ui->bookmarksTree);
                newParent->setText(0, folder);
                newParent->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                ui->bookmarksTree->addTopLevelItem(newParent);
                item = new QTreeWidgetItem(newParent);
            }
        } else
            item = new QTreeWidgetItem(ui->bookmarksTree);

        item->setText(0, title);
        item->setText(1, url.toEncoded());
        item->setToolTip(0, title);
        item->setToolTip(1, url.toEncoded());

        item->setWhatsThis(1, QString::number(id));
        item->setIcon(0, LocationBar::icon(url));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->bookmarksTree->addTopLevelItem(item);
    }
    ui->bookmarksTree->expandAll();

    ui->bookmarksTree->setUpdatesEnabled(true);
    m_isRefreshing = false;
}

void BookmarksManager::addBookmark(const BookmarksModel::Bookmark &bookmark)
{
    m_isRefreshing = true;
    QString translatedFolder = BookmarksModel::toTranslatedFolder(bookmark.folder);
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, bookmark.title);
    item->setText(1, bookmark.url.toEncoded());
    item->setWhatsThis(1, QString::number(bookmark.id));
    item->setIcon(0, LocationBar::icon(bookmark.url));
    item->setToolTip(0, bookmark.title);
    item->setToolTip(1, bookmark.url.toEncoded());
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    if (bookmark.folder != "unsorted")
        ui->bookmarksTree->addToParentItem(translatedFolder, item);
    else
        ui->bookmarksTree->addTopLevelItem(item);
    m_isRefreshing = false;
}

void BookmarksManager::removeBookmark(const BookmarksModel::Bookmark &bookmark)
{
    m_isRefreshing = true;
    if (bookmark.folder == "unsorted") {
        QList<QTreeWidgetItem*> list = ui->bookmarksTree->findItems(bookmark.title, Qt::MatchExactly);
        if (list.count() == 0)
            return;
        QTreeWidgetItem* item = list.at(0);
        if (item && item->whatsThis(1) == QString::number(bookmark.id))
            delete item;
    } else {
        QList<QTreeWidgetItem*> list = ui->bookmarksTree->findItems(BookmarksModel::toTranslatedFolder(bookmark.folder), Qt::MatchExactly);
        if (list.count() == 0)
            return;
        QTreeWidgetItem* parentItem = list.at(0);
        if (!parentItem)
            return;
        for (int i = 0; i < parentItem->childCount(); i++) {
            QTreeWidgetItem* item = parentItem->child(i);
            if (!item)
                continue;
            if (item->text(0) == bookmark.title  && item->whatsThis(1) == QString::number(bookmark.id)) {
                delete item;
                return;
            }
        }
    }
    m_isRefreshing = false;
}

void BookmarksManager::bookmarkEdited(const BookmarksModel::Bookmark &before, const BookmarksModel::Bookmark &after)
{
    removeBookmark(before);
    addBookmark(after);
}

void BookmarksManager::addFolder(const QString &name)
{
    m_isRefreshing = true;
    QTreeWidgetItem* item = new QTreeWidgetItem(ui->bookmarksTree);
    item->setText(0, name);
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    m_isRefreshing = false;
}

void BookmarksManager::removeFolder(const QString &name)
{
    QTreeWidgetItem* item = ui->bookmarksTree->findItems(name, Qt::MatchExactly).at(0);
    if (item)
        delete item;
}

void BookmarksManager::insertBookmark(const QUrl &url, const QString &title)
{
    if (url.isEmpty() || title.isEmpty())
        return;
    QDialog* dialog = new QDialog(getQupZilla());
    QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, dialog);
    QLabel* label = new QLabel(dialog);
    QLineEdit* edit = new QLineEdit(dialog);
    QComboBox* combo = new QComboBox(dialog);
    QDialogButtonBox* box = new QDialogButtonBox(dialog);
    box->addButton(QDialogButtonBox::Ok);
    box->addButton(QDialogButtonBox::Cancel);
    connect(box, SIGNAL(rejected()), dialog, SLOT(reject()));
    connect(box, SIGNAL(accepted()), dialog, SLOT(accept()));
    layout->addWidget(label);
    layout->addWidget(edit);
    layout->addWidget(combo);
    if (m_bookmarksModel->isBookmarked(url))
        layout->addWidget(new QLabel(tr("<b>Warning: </b>You already have this page bookmarked!")));
    layout->addWidget(box);

    combo->addItem(QIcon(":icons/other/unsortedbookmarks.png"), tr("Unsorted Bookmarks"));
    combo->addItem(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Bookmarks In Menu"));
    combo->addItem(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Bookmarks In ToolBar"));
    QSqlQuery query;
    query.exec("SELECT name FROM folders");
    while(query.next())
        combo->addItem(style()->standardIcon(QStyle::SP_DirIcon), query.value(0).toString());

    label->setText(tr("Choose name and location of bookmark."));
    edit->setText(title);
    edit->setCursorPosition(0);
    dialog->setWindowIcon(LocationBar::icon(url));
    dialog->setWindowTitle(tr("Add New Bookmark"));

    QSize size = dialog->size();
    size.setWidth(350);
    dialog->resize(size);
    dialog->exec();
    if (dialog->result() == QDialog::Rejected)
        return;
    if (edit->text().isEmpty())
        return;

    m_bookmarksModel->saveBookmark(url, edit->text(), BookmarksModel::fromTranslatedFolder(combo->currentText()));
    delete dialog;
}

void BookmarksManager::insertAllTabs()
{
    QDialog* dialog = new QDialog(getQupZilla());
    QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, dialog);
    QLabel* label = new QLabel(dialog);
    QComboBox* combo = new QComboBox(dialog);
    QDialogButtonBox* box = new QDialogButtonBox(dialog);
    box->addButton(QDialogButtonBox::Ok);
    box->addButton(QDialogButtonBox::Cancel);
    connect(box, SIGNAL(rejected()), dialog, SLOT(reject()));
    connect(box, SIGNAL(accepted()), dialog, SLOT(accept()));
    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addWidget(box);

    combo->addItem(QIcon(":icons/other/unsortedbookmarks.png"), tr("Unsorted Bookmarks"));
    combo->addItem(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Bookmarks In Menu"));
    combo->addItem(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Bookmarks In ToolBar"));
    QSqlQuery query;
    query.exec("SELECT name FROM folders");
    while(query.next())
        combo->addItem(style()->standardIcon(QStyle::SP_DirIcon), query.value(0).toString());

    label->setText(tr("Choose folder for bookmarks:"));
    dialog->setWindowIcon(QIcon(":/icons/qupzilla.png"));
    dialog->setWindowTitle(tr("Bookmark All Tabs"));

    QSize size = dialog->size();
    size.setWidth(350);
    dialog->resize(size);
    dialog->exec();
    if (dialog->result() == QDialog::Rejected)
        return;

    for (int i = 0; i<getQupZilla()->tabWidget()->count(); i++) {
        WebView* view = getQupZilla()->weView(i);
        if (!view || view->url().isEmpty())
            continue;

        m_bookmarksModel->saveBookmark(view->url(), view->title(), BookmarksModel::fromTranslatedFolder(combo->currentText()));
    }

    delete dialog;
}

BookmarksManager::~BookmarksManager()
{
    delete ui;
}
