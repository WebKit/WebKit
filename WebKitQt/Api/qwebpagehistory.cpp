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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "qwebpagehistory.h"
#include "qwebpagehistory_p.h"

#include "DeprecatedString.h"
#include "PlatformString.h"
#include "Image.h"

#include <QSharedData>

QWebHistoryItem::~QWebHistoryItem()
{
}

QUrl QWebHistoryItem::originalUrl() const
{
    return QUrl(d->item->originalURL().url());
}


QUrl QWebHistoryItem::currentUrl() const
{
    return QUrl(d->item->url().url());
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

QWebPageHistory::QWebPageHistory(QWebPageHistoryPrivate *priv)
{
    d = priv;
}

QWebHistoryItem QWebPageHistory::itemAtIndex(int i) const
{
    WebCore::HistoryItem *item = d->lst->itemAtIndex(i);

    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(item);
    return QWebHistoryItem(priv);
}

QWebPageHistory::QWebPageHistory(const QWebPageHistory &other)
{
    d = other.d;
}

QWebPageHistory::~QWebPageHistory()
{
}

QList<QWebHistoryItem> QWebPageHistory::items() const
{
    const WebCore::HistoryItemVector &items = d->lst->entries();

    QList<QWebHistoryItem> ret;
    for (int i = 0; i < items.size(); ++i) {
        QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(items[i].get());
        ret.append(QWebHistoryItem(priv));
    }
    return ret;
}

QList<QWebHistoryItem> QWebPageHistory::backItems(int maxItems) const
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

QList<QWebHistoryItem> QWebPageHistory::forwardItems(int maxItems) const
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

bool QWebPageHistory::canGoBack() const
{
    return d->lst->backListCount() > 0;
}

bool QWebPageHistory::canGoForward() const
{
    return d->lst->forwardListCount() > 0;
}

void QWebPageHistory::goBack()
{
    d->lst->goBack();
}

void QWebPageHistory::goForward()
{
    d->lst->goBack();
}

void QWebPageHistory::goToItem(QWebHistoryItem *item)
{
    d->lst->goToItem(item->d->item);
}

QWebHistoryItem QWebPageHistory::backItem() const
{
    WebCore::HistoryItem *i = d->lst->backItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

QWebHistoryItem QWebPageHistory::currentItem() const
{
    WebCore::HistoryItem *i = d->lst->currentItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

QWebHistoryItem QWebPageHistory::forwardItem() const
{
    WebCore::HistoryItem *i = d->lst->forwardItem();
    QWebHistoryItemPrivate *priv = new QWebHistoryItemPrivate(i);
    return QWebHistoryItem(priv);
}

