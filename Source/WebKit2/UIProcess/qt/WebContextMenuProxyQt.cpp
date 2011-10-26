/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "WebContextMenuProxyQt.h"

#include <IntPoint.h>
#include <OwnPtr.h>
#include <WebContextMenuItemData.h>
#include <qmenu.h>
#include <QtWebPageProxy.h>

using namespace WebCore;

Q_DECLARE_METATYPE(WebKit::WebContextMenuItemData);

namespace WebKit {

WebContextMenuProxyQt::WebContextMenuProxyQt(WebPageProxy* pageProxy, QtViewInterface* viewInterface)
    : m_webPageProxy(pageProxy)
    , m_viewInterface(viewInterface)
{
}

PassRefPtr<WebContextMenuProxyQt> WebContextMenuProxyQt::create(WebPageProxy* pageProxy, QtViewInterface* viewInterface)
{
    return adoptRef(new WebContextMenuProxyQt(pageProxy, viewInterface));
}

void WebContextMenuProxyQt::actionTriggered(bool)
{
    QAction* qtAction = qobject_cast<QAction*>(sender());
    if (!qtAction) {
        ASSERT_NOT_REACHED();
        return;
    }

    QVariant data = qtAction->data();
    if (!data.canConvert<WebContextMenuItemData>()) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_webPageProxy->contextMenuItemSelected(qtAction->data().value<WebContextMenuItemData>());
}

void WebContextMenuProxyQt::showContextMenu(const IntPoint& position, const Vector<WebContextMenuItemData>& items)
{
    // FIXME: Make this a QML compatible context menu.
}

void WebContextMenuProxyQt::hideContextMenu()
{
    m_viewInterface->hideContextMenu();
}

PassOwnPtr<QMenu> WebContextMenuProxyQt::createContextMenu(const Vector<WebContextMenuItemData>& items) const
{
    // FIXME: Make this a QML compatible context menu.
    return nullptr;
}

#include "moc_WebContextMenuProxyQt.cpp"

} // namespace WebKit
