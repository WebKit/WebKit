#ifndef QWEBHISTORY_P_H
#define QWEBHISTORY_P_H

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

class QWebHistoryPrivate : public QSharedData
{
public:
    QWebHistoryPrivate(WebCore::BackForwardList *l)
    {
        l->ref();
        lst = l;
    }
    ~QWebHistoryPrivate()
    {
        lst->deref();
    }
    WebCore::BackForwardList *lst;
};


#endif
