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

#ifndef qwkhistory_p_h
#define qwkhistory_p_h

#include <QSharedData>
#include "qwebkitglobal.h"
#include <WebKit2/WKBase.h>
#include <WebKit2/WKRetainPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {
class WebBackForwardList;
}

class QWKHistory;

class QWEBKIT_EXPORT QWKHistoryItemPrivate : public QSharedData {
public:
       ~QWKHistoryItemPrivate();
private:
    QWKHistoryItemPrivate(WKBackForwardListItemRef listItem);
    WKRetainPtr<WKBackForwardListItemRef> m_backForwardListItem;

    friend class QWKHistory;
    friend class QWKHistoryItem;
};

class QWEBKIT_EXPORT QWKHistoryPrivate {
public:
    static QWKHistory* createHistory(WebKit::WebBackForwardList* list);

private:
    QWKHistoryPrivate(WebKit::WebBackForwardList* list);
    ~QWKHistoryPrivate();

    WebKit::WebBackForwardList* m_backForwardList;

    friend class QWKHistory;
};

#endif /* qwkhistory_p_h */
