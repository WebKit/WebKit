/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPopupMenuProxyQtDesktop_h
#define WebPopupMenuProxyQtDesktop_h

#include "WebPopupMenuProxy.h"
#include <QComboBox>
#include <QObject>
#include <QWeakPointer>

class QQuickItem;

namespace WebCore {
class QtWebComboBox;
}

namespace WebKit {

class WebPopupMenuProxyQtDesktop : public QComboBox, public WebPopupMenuProxy {
    Q_OBJECT

public:
    virtual ~WebPopupMenuProxyQtDesktop();

    static PassRefPtr<WebPopupMenuProxyQtDesktop> create(WebPopupMenuProxy::Client* client, QQuickItem* webViewItem)
    {
        return adoptRef(new WebPopupMenuProxyQtDesktop(client, webViewItem));
    }

    virtual void showPopupMenu(const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>&, const PlatformPopupMenuData&, int32_t selectedIndex);
    virtual void hidePopupMenu();

    virtual bool eventFilter(QObject* watched, QEvent*);

private Q_SLOTS:
    void setSelectedIndex(int);

private:
    WebPopupMenuProxyQtDesktop(WebPopupMenuProxy::Client*, QQuickItem* webViewItem);
    void populate(const Vector<WebPopupItem>&);

    QQuickItem* m_webViewItem;
    int32_t m_selectedIndex;
};

} // namespace WebKit

#endif // WebPopupMenuProxyQtDesktop_h
