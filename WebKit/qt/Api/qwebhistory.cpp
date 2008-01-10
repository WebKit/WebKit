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

#include "DeprecatedString.h"
#include "PlatformString.h"
#include "Image.h"

#include <QSharedData>

QWebHistoryItem::QWebHistoryItem(const QWebHistoryItem &other)
    : d(other.d)
{
}

QWebHistoryItem &QWebHistoryItem::operator=(const QWebHistoryItem &other)
{
    d = other.d;
    return *this;
}

QWebHistoryItem::~QWebHistoryItem()
{
}

QUrl QWebHistoryItem::originalUrl() const
{
    return QUrl(d->item->originalURL().string());
}


QUrl QWebHistoryItem::currentUrl() const
{
    return QUrl(d->item->url().string());
}


QString QWebHistoryItem::title() const
{
    return d->item->title();
}


QDateTime QWebHistoryItem::lastVisited() const
{
    //FIXME : this will be wrong unless we correctly set lastVisitedTime ourselves
    return QDateTime::fromTime_t((uint)d->item->lastVisitedTime());
}


QPixmap QWebHistoryItem::icon() const
{
    return *d->item->icon()->getPixmap();
}


QWebHistoryItem::QWebHistoryItem(QWebHistoryItemPrivate *priv)
{
    d = priv;
}

QWebHistory::QWebHistory()
    : d(0)
{
}

QWebHistory::~QWebHistory()
{
    delete d;
}

void QWebHistory::clear()
{
    RefPtr<WebCore::HistoryItem> current = d->lst->currentItem();
    int capacity = d->lst->capacity();
    d->lst->setCapacity(0);    
    d->lst->setCapacity(capacity);
    d->lst->addItem(current.get());
    d->lst->goToItem(current.get());
}

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

bool QWebHistory::canGoBack() const
{
    return d->lst->backListCount() > 0;
}

bool QWebHistory::canGoForward() const
{
    return d->lst->forwardListCount() > 0;
}

void QWebHistory::goBack()
{
    d->lst->goBack();
}

void QWebHistory::goForward()
{
    d->lst->goBack();
}

void QWebHistory::goToItem(const QWebHistoryItem &item)
{
    d->lst->goToItem(item.d->item);
}

QWebHistoryItem QWebHistory::backItem() const
{
    WebCore::HistoryItem *i = d->lst->backItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

QWebHistoryItem QWebHistory::currentItem() const
{
    WebCore::HistoryItem *i = d->lst->currentItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

QWebHistoryItem QWebHistory::forwardItem() const
{
    WebCore::HistoryItem *i = d->lst->forwardItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

QWebHistoryItem QWebHistory::itemAtIndex(int i) const
{
    WebCore::HistoryItem *item = d->lst->itemAtIndex(i);

    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(item);
    return QWebHistoryItem(priv);
}

