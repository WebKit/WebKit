/*
    Copyright (C) 2007 Trolltech ASA

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "qwebhistory.h"
#include "qwebhistory_p.h"

#include "PlatformString.h"
#include "Image.h"

#include <QSharedData>

/*!
  \class QWebHistoryItem
  \since 4.4
  \brief The QWebHistoryItem class represents one item in the history of a QWebPage

  QWebHistoryItem represents on entry in the history stack of a web page.

  \sa QWebPage::history() QWebHistory

  QWebHistoryItem objects are value based and explicitly shared. 
*/

/*!
  Constructs a history item from \a other.
*/
QWebHistoryItem::QWebHistoryItem(const QWebHistoryItem &other)
    : d(other.d)
{
}

/*!
  Assigns the \a other history item to this.
*/
QWebHistoryItem &QWebHistoryItem::operator=(const QWebHistoryItem &other)
{
    d = other.d;
    return *this;
}

/*!
  Destructs a history item.
*/
QWebHistoryItem::~QWebHistoryItem()
{
}

/*!
 The original url associated with the history item.
*/
QUrl QWebHistoryItem::originalUrl() const
{
    return QUrl(d->item->originalURL().string());
}


/*!
 The current url associated with the history item.
*/
QUrl QWebHistoryItem::currentUrl() const
{
    return QUrl(d->item->url().string());
}


/*!
 The title of the page associated with the history item.
*/
QString QWebHistoryItem::title() const
{
    return d->item->title();
}


/*!
 The time when the apge associated with the item was last visited.
*/
QDateTime QWebHistoryItem::lastVisited() const
{
    //FIXME : this will be wrong unless we correctly set lastVisitedTime ourselves
    return QDateTime::fromTime_t((uint)d->item->lastVisitedTime());
}


/*!
 The icon associated with the history item.
*/
QPixmap QWebHistoryItem::icon() const
{
    return *d->item->icon()->getPixmap();
}

/*!
  \internal
*/
QWebHistoryItem::QWebHistoryItem(QWebHistoryItemPrivate *priv)
{
    d = priv;
}

/*!
  \class QWebHistory
  \since 4.4
  \brief The QWebHistory class represents the history of a QWebPage

  Each QWebPage contains a history of visited pages that can be accessed by QWebPage::history().
  QWebHistory represents this history and makes it possible to navigate it.
*/


QWebHistory::QWebHistory()
    : d(0)
{
}

QWebHistory::~QWebHistory()
{
    delete d;
}

/*!
  Clears the history.
*/
void QWebHistory::clear()
{
    RefPtr<WebCore::HistoryItem> current = d->lst->currentItem();
    int capacity = d->lst->capacity();
    d->lst->setCapacity(0);    
    d->lst->setCapacity(capacity);
    d->lst->addItem(current.get());
    d->lst->goToItem(current.get());
}

/*!
  returns a list of all items currently in the history.
*/
QList<QWebHistoryItem> QWebHistory::items() const
{
    const WebCore::HistoryItemVector &items = d->lst->entries();

    QList<QWebHistoryItem> ret;
    for (int i = 0; i < items.size(); ++i) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(items[i].get());
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

/*!
  Returns the list of items that are in the backwards history.
  At most \a maxItems entries are returned.
*/
QList<QWebHistoryItem> QWebHistory::backItems(int maxItems) const
{
    WebCore::HistoryItemVector items(maxItems);
    d->lst->backListWithLimit(maxItems, items);

    QList<QWebHistoryItem> ret;
    for (int i = 0; i < items.size(); ++i) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(items[i].get());
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

/*!
  Returns the list of items that are in the forward history.
  At most \a maxItems entries are returned.
*/
QList<QWebHistoryItem> QWebHistory::forwardItems(int maxItems) const
{
    WebCore::HistoryItemVector items(maxItems);
    d->lst->forwardListWithLimit(maxItems, items);

    QList<QWebHistoryItem> ret;
    for (int i = 0; i < items.size(); ++i) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(items[i].get());
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

/*!
  returns true if we have an item to go back to.
*/
bool QWebHistory::canGoBack() const
{
    return d->lst->backListCount() > 0;
}

/*!
  returns true if we have an item to go forward to.
*/
bool QWebHistory::canGoForward() const
{
    return d->lst->forwardListCount() > 0;
}

/*!
  goes back one history item.
*/
void QWebHistory::goBack()
{
    d->lst->goBack();
}

/*!
  goes forward one history item.
*/
void QWebHistory::goForward()
{
    d->lst->goBack();
}

/*!
  goes to item \a item in the history.
*/
void QWebHistory::goToItem(const QWebHistoryItem &item)
{
    d->lst->goToItem(item.d->item);
}

/*!
  returns the item before the current item.
*/
QWebHistoryItem QWebHistory::backItem() const
{
    WebCore::HistoryItem *i = d->lst->backItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

/*!
  returns the current item.
*/
QWebHistoryItem QWebHistory::currentItem() const
{
    WebCore::HistoryItem *i = d->lst->currentItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

/*!
  returns the item after the current item.
*/
QWebHistoryItem QWebHistory::forwardItem() const
{
    WebCore::HistoryItem *i = d->lst->forwardItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

/*!
  returns the item at index \a i.
*/
QWebHistoryItem QWebHistory::itemAtIndex(int i) const
{
    WebCore::HistoryItem *item = d->lst->itemAtIndex(i);

    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(item);
    return QWebHistoryItem(priv);
}

