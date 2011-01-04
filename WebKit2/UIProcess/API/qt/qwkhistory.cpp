/*
 * Copyright (C) 2010 Juha Savolainen (juha.savolainen@weego.fi)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "qwkhistory.h"

#include <QSharedData>
#include <QString>
#include <QUrl>
#include "qwkhistory_p.h"
#include "WebBackForwardList.h"
#include <WebKit2/WKArray.h>
#include <WebKit2/WKRetainPtr.h>
#include "WKBackForwardList.h"
#include "WKStringQt.h"
#include "WKURL.h"
#include "WKURLQt.h"

using namespace WebKit;

QWKHistoryItemPrivate::QWKHistoryItemPrivate(WKBackForwardListItemRef listItem)
    : m_backForwardListItem(listItem)
{
}

QWKHistoryItemPrivate::~QWKHistoryItemPrivate()
{
}

QWKHistoryItem::QWKHistoryItem(const QWKHistoryItem& other)
    : d(other.d) 
{
}

QWKHistoryItem& QWKHistoryItem::QWKHistoryItem::operator=(const QWKHistoryItem& other) 
{ 
    d = other.d;
    return *this; 
}

QWKHistoryItem::QWKHistoryItem(WKBackForwardListItemRef item)
    : d(new QWKHistoryItemPrivate(item))
{
}

QWKHistoryItem::~QWKHistoryItem()
{
}

QString QWKHistoryItem::title() const
{
    if (!d->m_backForwardListItem)
        return QString();
    WKRetainPtr<WKStringRef> title = WKBackForwardListItemCopyTitle(d->m_backForwardListItem.get());
    return WKStringCopyQString(title.get());
}

QUrl QWKHistoryItem::url() const
{
    if (!d->m_backForwardListItem)
        return QUrl();
    WKRetainPtr<WKURLRef> url = WKBackForwardListItemCopyURL(d->m_backForwardListItem.get());
    return WKURLCopyQUrl(url.get());
}

QWKHistoryPrivate::QWKHistoryPrivate(WebKit::WebBackForwardList* list)
    : m_backForwardList(list)
{
}

QWKHistory* QWKHistoryPrivate::createHistory(WebKit::WebBackForwardList* list)
{
    QWKHistory* history = new QWKHistory();
    history->d = new QWKHistoryPrivate(list);
    return history;
}

QWKHistoryPrivate::~QWKHistoryPrivate()
{
}

QWKHistory::QWKHistory()
{
}

QWKHistory::~QWKHistory()
{
    delete d;
}

int QWKHistory::backListCount() const
{
    return WKBackForwardListGetBackListCount(toAPI(d->m_backForwardList));
}

int QWKHistory::forwardListCount() const
{
    return WKBackForwardListGetForwardListCount(toAPI(d->m_backForwardList));
}

int QWKHistory::count() const
{
    return backListCount() + forwardListCount();
}

QWKHistoryItem QWKHistory::currentItem() const
{
    WKRetainPtr<WKBackForwardListItemRef> itemRef = WKBackForwardListGetCurrentItem(toAPI(d->m_backForwardList));
    QWKHistoryItem item(itemRef.get());
    return item;
}

QWKHistoryItem QWKHistory::backItem() const
{
    WKRetainPtr<WKBackForwardListItemRef> itemRef = WKBackForwardListGetBackItem(toAPI(d->m_backForwardList));
    QWKHistoryItem item(itemRef.get());
    return item;
}

QWKHistoryItem QWKHistory::forwardItem() const
{
    WKRetainPtr<WKBackForwardListItemRef> itemRef = WKBackForwardListGetForwardItem(toAPI(d->m_backForwardList));
    QWKHistoryItem item(itemRef.get());
    return item;
}

QWKHistoryItem QWKHistory::itemAt(int index) const
{
    WKRetainPtr<WKBackForwardListItemRef> itemRef = WKBackForwardListGetItemAtIndex(toAPI(d->m_backForwardList), index);
    QWKHistoryItem item(itemRef.get());
    return item;
}

QList<QWKHistoryItem> QWKHistory::backItems(int maxItems) const
{
    WKArrayRef arrayRef = WKBackForwardListCopyBackListWithLimit(toAPI(d->m_backForwardList), maxItems);
    int size = WKArrayGetSize(arrayRef);
    QList<QWKHistoryItem> itemList;
    for (int i = 0; i < size; ++i) {
        WKTypeRef wkHistoryItem = WKArrayGetItemAtIndex(arrayRef, i);
        WKBackForwardListItemRef itemRef = static_cast<WKBackForwardListItemRef>(wkHistoryItem);
        QWKHistoryItem item(itemRef);
        itemList.append(item);
    }
    return itemList;
}

QList<QWKHistoryItem> QWKHistory::forwardItems(int maxItems) const
{
    WKArrayRef arrayRef = WKBackForwardListCopyForwardListWithLimit(toAPI(d->m_backForwardList), maxItems);
    int size = WKArrayGetSize(arrayRef);
    QList<QWKHistoryItem> itemList;
    for (int i = 0; i < size; ++i) {
        WKTypeRef wkHistoryItem = WKArrayGetItemAtIndex(arrayRef, i);
        WKBackForwardListItemRef itemRef = static_cast<WKBackForwardListItemRef>(wkHistoryItem);
        QWKHistoryItem item(itemRef);
        itemList.append(item);
    }
    return itemList;
}

