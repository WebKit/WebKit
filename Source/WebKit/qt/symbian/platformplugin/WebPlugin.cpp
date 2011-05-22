/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "WebPlugin.h"

#if defined(WTF_USE_QT_MULTIMEDIA) && WTF_USE_QT_MULTIMEDIA
#include "HTML5VideoPlugin.h"
#endif

#include <QtGui>
#include <QtPlugin>
#include <akndiscreetpopup.h>

class ItemListDelegate : public QStyledItemDelegate {
public:
    ItemListDelegate(QObject* parent = 0)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        Q_ASSERT(index.isValid());

        QStyleOptionViewItemV4 opt = option;
        opt.state = opt.state & (~QStyle::State_HasFocus);
        opt.text = index.data().toString();
        initStyleOption(&opt, index);

        const QWidget* widget = 0;
        Popup* selectPopup = qobject_cast<Popup*>(parent());
        if (selectPopup)
            widget = selectPopup->listWidget();

        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }
};

Popup::Popup(const QWebSelectData& data)
    : m_data(data)
{
    setModal(true);
    m_list = new QListWidget(this);
    int width = QApplication::desktop()->size().width();
    QVBoxLayout* vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(width / 20, 5, 5, 5);
    if (data.multiple())
        m_list->setSelectionMode(QAbstractItemView::MultiSelection);
    populateList();
    vLayout->addWidget(m_list);
   
    ItemListDelegate* itemDelegate = new ItemListDelegate(this);
    m_list->setItemDelegate(itemDelegate);
}

void Popup::resizeEvent(QResizeEvent* e)
{
    QDialog::resizeEvent(e);
    m_list->setGeometry(m_list->geometry().x(), m_list->geometry().y(), m_list->width(), m_list->height());
}

void Popup::populateList()
{
    QListWidgetItem* listItem;
    m_preSelectedIndices.clear();
    for (int i = 0; i < m_data.itemCount(); ++i) {
        if (m_data.itemType(i) == QWebSelectData::Option) {
            listItem = new QListWidgetItem(m_data.itemText(i));
            m_list->addItem(listItem);
            listItem->setSelected(m_data.itemIsSelected(i));
            if (m_data.itemIsSelected(i)) {
                m_preSelectedIndices.append(i);
                m_list->setCurrentItem(listItem);
            }
        } else if (m_data.itemType(i) == QWebSelectData::Group) {
            listItem = new QListWidgetItem(m_data.itemText(i));
            m_list->addItem(listItem);
            listItem->setSelected(false);
            listItem->setFlags(Qt::NoItemFlags);
        }
    }
    connect(m_list, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onItemSelected(QListWidgetItem*)));
}

void Popup::onItemSelected(QListWidgetItem* item)
{
    if (item->flags() != Qt::NoItemFlags) {
        if (!m_data.multiple())
            updateAndClose();
    }
}

void Popup::updateSelectionsBeforeDialogClosing()
{
    QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (m_data.multiple()) {
        QList<int> selectedIndices;
        Q_FOREACH(QListWidgetItem* item, selected)
            selectedIndices.append(m_list->row(item));

        for (int i = 0; i < m_data.itemCount(); ++i) {
            if ((m_preSelectedIndices.contains(i) && !selectedIndices.contains(i)) || (!m_preSelectedIndices.contains(i) && selectedIndices.contains(i)))
                emit itemClicked(i);
        }
    } else {
        Q_FOREACH(QListWidgetItem* item, selected)
            emit itemClicked(m_list->row(item));
    }
}

void Popup::updateAndClose()
{
    updateSelectionsBeforeDialogClosing();
    accept();
}

WebPopup::WebPopup()
    : m_popup(0)
{
}

WebPopup::~WebPopup()
{
    if (m_popup)
        m_popup->deleteLater();
}

Popup* WebPopup::createSingleSelectionPopup(const QWebSelectData& data)
{
    return new SingleSelectionPopup(data);
}

Popup* WebPopup::createMultipleSelectionPopup(const QWebSelectData& data)
{
    return new MultipleSelectionPopup(data);
}

Popup* WebPopup::createPopup(const QWebSelectData& data)
{
    Popup* result = data.multiple() ? createMultipleSelectionPopup(data) : createSingleSelectionPopup(data);
    connect(result, SIGNAL(finished(int)), this, SLOT(popupClosed()));
    connect(result, SIGNAL(itemClicked(int)), this, SLOT(itemClicked(int)));
    return result;
}

void WebPopup::show(const QWebSelectData& data)
{
    if (m_popup)
        return;

    m_popup = createPopup(data);
    m_popup->showMaximized();
}

void WebPopup::hide()
{
    if (!m_popup)
        return;

    m_popup->accept();
}

void WebPopup::popupClosed()
{
    if (!m_popup)
        return;

    m_popup->deleteLater();
    m_popup = 0;
    emit didHide();
}

void WebPopup::itemClicked(int idx)
{
    emit selectItem(idx, true, false);
}

SingleSelectionPopup::SingleSelectionPopup(const QWebSelectData& data)
    : Popup(data)
{
    setWindowTitle("Select item");

    QAction* cancelAction = new QAction("Cancel", this);
    cancelAction->setSoftKeyRole(QAction::NegativeSoftKey);
    addAction(cancelAction);
    connect(cancelAction, SIGNAL(triggered(bool)), this, SLOT(reject()));
}

MultipleSelectionPopup::MultipleSelectionPopup(const QWebSelectData& data)
    : Popup(data)
{
    setWindowTitle("Select items");

    QAction* cancelAction = new QAction("Cancel", this);
    cancelAction->setSoftKeyRole(QAction::NegativeSoftKey);
    addAction(cancelAction);
    connect(cancelAction, SIGNAL(triggered(bool)), this, SLOT(reject()));

    QAction* okAction = new QAction("Ok", this);
    okAction->setSoftKeyRole(QAction::PositiveSoftKey);
    addAction(okAction);
    connect(okAction, SIGNAL(triggered(bool)), this, SLOT(updateAndClose()));
}

void WebNotificationPresenter::showNotification(const QWebNotificationData* data)
{
    HBufC* title = HBufC::New(data->title().size());
    *title = TPtrC(data->title().utf16(), data->title().size());
    HBufC* message = HBufC::New(data->message().size());
    *message = TPtrC(data->message().utf16(), data->message().size());
    TRAP_IGNORE(CAknDiscreetPopup::ShowGlobalPopupL(*title, *message, KAknsIIDNone, KNullDesC));
    emit notificationClosed();
    deleteLater();
    delete title;
    delete message;
}

bool WebPlugin::supportsExtension(Extension extension) const
{
    if (extension == MultipleSelections)
        return true;
    if (extension == Notifications)
        return true;
    if (extension == FullScreenVideoPlayer)
        return true;        
    return false;
}

QObject* WebPlugin::createExtension(Extension extension) const
{
    switch (extension) {
    case MultipleSelections:
        return new WebPopup();
    case Notifications:
        return new WebNotificationPresenter();
#if defined(WTF_USE_QT_MULTIMEDIA) && WTF_USE_QT_MULTIMEDIA
    case FullScreenVideoPlayer:
        return new HTML5FullScreenVideoHandler();
#endif
    default:
        return 0;
    }
}

Q_EXPORT_PLUGIN2(qwebselectim, WebPlugin)
