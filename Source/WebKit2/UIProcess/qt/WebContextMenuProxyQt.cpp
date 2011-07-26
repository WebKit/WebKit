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

WebContextMenuProxyQt::WebContextMenuProxyQt(WebPageProxy* pageProxy, ViewInterface* viewInterface)
    : m_webPageProxy(pageProxy)
    , m_viewInterface(viewInterface)
{
}

PassRefPtr<WebContextMenuProxyQt> WebContextMenuProxyQt::create(WebPageProxy* pageProxy, ViewInterface* viewInterface)
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
    if (items.isEmpty())
        return;

    OwnPtr<QMenu> menu = createContextMenu(items);

    // We call showContextMenu(), even with no items, because the client should be able to show custom items
    // if WebKit has nothing to show.
    if (!menu)
        menu = adoptPtr(new QMenu);

    menu->move(position);
    m_viewInterface->showContextMenu(QSharedPointer<QMenu>(menu.leakPtr()));
}

void WebContextMenuProxyQt::hideContextMenu()
{
    m_viewInterface->hideContextMenu();
}

PassOwnPtr<QMenu> WebContextMenuProxyQt::createContextMenu(const Vector<WebContextMenuItemData>& items) const
{
    OwnPtr<QMenu> menu = adoptPtr(new QMenu);
    for (int i = 0; i < items.size(); ++i) {
        const WebContextMenuItemData& item = items.at(i);
        switch (item.type()) {
        case WebCore::CheckableActionType: /* fall through */
        case WebCore::ActionType: {
            QAction* qtAction = new QAction(menu.get());
            qtAction->setData(QVariant::fromValue(item));
            qtAction->setText(item.title());
            qtAction->setEnabled(item.enabled());
            qtAction->setChecked(item.checked());
            qtAction->setCheckable(item.type() == WebCore::CheckableActionType);
            connect(qtAction, SIGNAL(triggered(bool)), this, SLOT(actionTriggered(bool)), Qt::DirectConnection);

            menu->addAction(qtAction);
            break;
        }
        case WebCore::SeparatorType:
            menu->addSeparator();
            break;
        case WebCore::SubmenuType:
            if (OwnPtr<QMenu> subMenu = createContextMenu(item.submenu())) {
                static_cast<QObject*>(subMenu.get())->setParent(menu.get());
                subMenu->setTitle(item.title());
                QMenu* const subMenuPtr = subMenu.leakPtr();
                menu->addMenu(subMenuPtr);
            }
            break;
        }
    }

    // Do not show sub-menus with just disabled actions.
    if (menu->isEmpty())
        return nullptr;

    bool isAnyActionEnabled = false;
    QList<QAction *> actions = menu->actions();
    for (int i = 0; i < actions.count(); ++i) {
        if (actions.at(i)->isVisible())
            isAnyActionEnabled |= actions.at(i)->isEnabled();
    }
    if (!isAnyActionEnabled)
        return nullptr;

    return menu.release();
}

#include "moc_WebContextMenuProxyQt.cpp"

} // namespace WebKit
