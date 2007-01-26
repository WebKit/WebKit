#ifndef QWEBPAGEHISTORY_P_H
#define QWEBPAGEHISTORY_P_H

#include "BackForwardList.h"
#include "HistoryItem.h"

class QWebHistoryItemPrivate : public QSharedData
{
public:
    QWebHistoryItemPrivate(WebCore::HistoryItem *i)
    {
        i->ref();
        item = i;
    }
    ~QWebHistoryItemPrivate()
    {
        item->deref();
    }
    
    WebCore::HistoryItem *item;
};

class QWebPageHistoryPrivate : public QSharedData
{
public:
    QWebPageHistoryPrivate(WebCore::BackForwardList *l)
    {
        l->ref();
        lst = l;
    }
    ~QWebPageHistoryPrivate()
    {
        lst->deref();
    }
    WebCore::BackForwardList *lst;
};


#endif
