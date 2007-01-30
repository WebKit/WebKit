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
#ifndef QWEBPAGEHISTORY_H
#define QWEBPAGEHISTORY_H

#include <QUrl>
#include <QString>
#include <QIcon>
#include <QDateTime>
#include <QSharedData>

#include <qwebkitglobal.h>

#if QT_VERSION < 0x040300
template <class T> class QExplicitlySharedDataPointer
{
public:
    typedef T Type;

    inline T &operator*() { return *d; }
    inline const T &operator*() const { return *d; }
    inline T *operator->() { return d; }
    inline const T *operator->() const { return d; }
    inline operator T *() { return d; }
    inline operator const T *() const { return d; }
    inline T *data() { return d; }
    inline const T *data() const { return d; }
    inline const T *constData() const { return d; }

    //inline operator bool () const { return d != 0; }

    inline bool operator==(const QExplicitlySharedDataPointer<T> &other) const { return d == other.d; }
    inline bool operator!=(const QExplicitlySharedDataPointer<T> &other) const { return d != other.d; }
    inline bool operator==(const T *ptr) const { return d == ptr; }
    inline bool operator!=(const T *ptr) const { return d != ptr; }

    inline QExplicitlySharedDataPointer() { d = 0; }
    inline ~QExplicitlySharedDataPointer() { if (d && !d->ref.deref()) delete d; }

    explicit QExplicitlySharedDataPointer(T *data);
    inline QExplicitlySharedDataPointer(const QExplicitlySharedDataPointer<T> &o) : d(o.d) { if (d) d->ref.ref(); }
    inline QExplicitlySharedDataPointer<T> & operator=(const QExplicitlySharedDataPointer<T> &o) {
        if (o.d != d) {
            T *x = o.d;
            if (x) x->ref.ref();
            x = qAtomicSetPtr(&d, x);
            if (x && !x->ref.deref())
                delete x;
        }
        return *this;
    }
    inline QExplicitlySharedDataPointer &operator=(T *o) {
        if (o != d) {
            T *x = o;
            if (x) x->ref.ref();
            x = qAtomicSetPtr(&d, x);
            if (x && !x->ref.deref())
                delete x;
        }
        return *this;
    }

    inline bool operator!() const { return !d; }

private:

    T *d;
};
template <class T>
Q_INLINE_TEMPLATE QExplicitlySharedDataPointer<T>::QExplicitlySharedDataPointer(T *adata) : d(adata)
{ if (d) d->ref.ref(); }
#endif

class QWebPage;

class QWebHistoryItemPrivate;
class QWEBKIT_EXPORT QWebHistoryItem
{
public:
    ~QWebHistoryItem();

    QWebHistoryItem *parent() const;
    QList<QWebHistoryItem*> children() const;

    QUrl originalUrl() const;
    QUrl currentUrl() const;

    QString title() const;
    QDateTime lastVisited() const;

    QPixmap icon() const;

    QWebHistoryItem(QWebHistoryItemPrivate *priv);
private:
    friend class QWebPageHistory;
    friend class QWebPage;
    QExplicitlySharedDataPointer<QWebHistoryItemPrivate> d;
};

class QWebPageHistoryPrivate;
class QWEBKIT_EXPORT QWebPageHistory
{
public:
    QWebPageHistory(const QWebPageHistory &other);
    ~QWebPageHistory();
    
    QList<QWebHistoryItem> items() const;
    QList<QWebHistoryItem> backItems(int maxItems) const;
    QList<QWebHistoryItem> forwardItems(int maxItems) const;

    bool canGoBack() const;
    bool canGoForward() const;

    void goBack();
    void goForward();
    void goToItem(QWebHistoryItem *item);

    QWebHistoryItem backItem() const;
    QWebHistoryItem currentItem() const;
    QWebHistoryItem forwardItem() const;
    QWebHistoryItem itemAtIndex(int i) const;

    
    QWebPageHistory(QWebPageHistoryPrivate *priv);
private:
    QExplicitlySharedDataPointer<QWebPageHistoryPrivate> d;
};

#endif
