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

#ifndef qwkhistory_h
#define qwkhistory_h

#include "WebKit2/WKBackForwardListItem.h"
#include "qwebkitglobal.h"
#include <QObject>
#include <QSharedData>

class QWKHistoryPrivate;
class QWKHistoryItemPrivate;
class QUrl;
class QString;

namespace WebKit {
class WebBackForwardList;
}

class QWEBKIT_EXPORT QWKHistoryItem {
public:
    QWKHistoryItem(const QWKHistoryItem& other);
    QWKHistoryItem &operator=(const QWKHistoryItem& other);

    ~QWKHistoryItem();
    QString title() const;
    QUrl url() const;

private:
    QWKHistoryItem(WKBackForwardListItemRef);

    QExplicitlySharedDataPointer<QWKHistoryItemPrivate> d;

    friend class QWKHistory;
    friend class QWKHistoryItemPrivate;
};

class QWEBKIT_EXPORT QWKHistory : public QObject {
    Q_OBJECT
public:
    int backListCount() const;
    int forwardListCount() const;
    int count() const;
    QWKHistoryItem currentItem() const;
    QWKHistoryItem backItem() const;
    QWKHistoryItem forwardItem() const;
    QWKHistoryItem itemAt(int index) const;
    QList<QWKHistoryItem> backItems(int maxItems) const;
    QList<QWKHistoryItem> forwardItems(int maxItems) const;

private:
    QWKHistory();
    ~QWKHistory();

    QWKHistoryPrivate* d;
    friend class QWKHistoryPrivate;
    friend class QWKPagePrivate;
};
#endif /* qwkhistory_h */
