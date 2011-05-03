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
#include <qwkpage.h>

using namespace WebCore;

namespace WebKit {

static QWKPage::WebAction webActionForContextMenuAction(WebCore::ContextMenuAction action)
{
    switch (action) {
    case WebCore::ContextMenuItemTagOpenLink:
        return QWKPage::OpenLink;
    case WebCore::ContextMenuItemTagOpenLinkInNewWindow:
        return QWKPage::OpenLinkInNewWindow;
    case WebCore::ContextMenuItemTagCopyLinkToClipboard:
        return QWKPage::CopyLinkToClipboard;
    case WebCore::ContextMenuItemTagOpenImageInNewWindow:
        return QWKPage::OpenImageInNewWindow;
    case WebCore::ContextMenuItemTagGoBack:
        return QWKPage::Back;
    case WebCore::ContextMenuItemTagGoForward:
        return QWKPage::Forward;
    case WebCore::ContextMenuItemTagStop:
        return QWKPage::Stop;
    case WebCore::ContextMenuItemTagReload:
        return QWKPage::Reload;
    case WebCore::ContextMenuItemTagCut:
        return QWKPage::Cut;
    case WebCore::ContextMenuItemTagCopy:
        return QWKPage::Copy;
    case WebCore::ContextMenuItemTagPaste:
        return QWKPage::Paste;
    case WebCore::ContextMenuItemTagSelectAll:
        return QWKPage::SelectAll;
    default:
        break;
    }
    return QWKPage::NoWebAction;
}

WebContextMenuProxyQt::WebContextMenuProxyQt(QWKPage* page)
    : m_page(page)
{
}

PassRefPtr<WebContextMenuProxyQt> WebContextMenuProxyQt::create(QWKPage* page)
{
    return adoptRef(new WebContextMenuProxyQt(page));
}

void WebContextMenuProxyQt::showContextMenu(const IntPoint& position, const Vector<WebContextMenuItemData>& items)
{
    if (items.isEmpty())
        return;

    OwnPtr<QMenu> menu = createContextMenu(items);

    // We send the signal, even with no items, because the client should be able to show custom items
    // even if WebKit has nothing to show.
    if (!menu)
        menu = adoptPtr(new QMenu);

    menu->move(position);
    emit m_page->showContextMenu(QSharedPointer<QMenu>(menu.leakPtr()));
}

void WebContextMenuProxyQt::hideContextMenu()
{
}

PassOwnPtr<QMenu> WebContextMenuProxyQt::createContextMenu(const Vector<WebContextMenuItemData>& items) const
{
    OwnPtr<QMenu> menu = adoptPtr(new QMenu);
    for (int i = 0; i < items.size(); ++i) {
        const WebContextMenuItemData& item = items.at(i);
        switch (item.type()) {
        case WebCore::CheckableActionType: /* fall through */
        case WebCore::ActionType: {
            QWKPage::WebAction action = webActionForContextMenuAction(item.action());
            QAction* qtAction = m_page->action(action);
            if (qtAction) {
                qtAction->setEnabled(item.enabled());
                qtAction->setChecked(item.checked());
                qtAction->setCheckable(item.type() == WebCore::CheckableActionType);

                menu->addAction(qtAction);
            }
            break;
        }
        case WebCore::SeparatorType:
            menu->addSeparator();
            break;
        case WebCore::SubmenuType:
            if (OwnPtr<QMenu> subMenu = createContextMenu(item.submenu())) {
                subMenu->setParent(menu.get());
                QMenu* const subMenuPtr = subMenu.leakPtr();
                subMenu->setTitle(item.title());
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

} // namespace WebKit
