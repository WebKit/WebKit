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

