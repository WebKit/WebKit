/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "WebContextMenu.h"

#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ContextMenu.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/FrameView.h>
#include <WebCore/Page.h>

using namespace WebCore;

namespace WebKit {

WebContextMenu::WebContextMenu(WebPage* page)
    : m_page(page)
{
}

WebContextMenu::~WebContextMenu()
{
}

void WebContextMenu::show()
{
    ContextMenu* menu = m_page->corePage()->contextMenuController()->contextMenu();
    if (!menu)
        return;
    Node* node = menu->hitTestResult().innerNonSharedNode();
    if (!node)
        return;
    Frame* frame = node->document()->frame();
    if (!frame)
        return;
    FrameView* view = frame->view();
    if (!view)
        return;

    // Give the bundle client a chance to process the menu.
    Vector<ContextMenuItem> coreItems = contextMenuItemVector(menu->platformDescription());
    Vector<WebContextMenuItemData> proposedMenu = kitItems(coreItems, menu);
    Vector<WebContextMenuItemData> newMenu;
    RefPtr<APIObject> userData;
    RefPtr<InjectedBundleHitTestResult> hitTestResult = InjectedBundleHitTestResult::create(menu->hitTestResult());
    if (m_page->injectedBundleContextMenuClient().getCustomMenuFromDefaultItems(m_page, hitTestResult.get(), proposedMenu, newMenu, userData))
        proposedMenu = newMenu;

    // Notify the UIProcess.
    m_page->send(Messages::WebPageProxy::ShowContextMenu(view->contentsToWindow(menu->hitTestResult().point()), proposedMenu, InjectedBundleUserMessageEncoder(userData.get())));
}

void WebContextMenu::itemSelected(const WebContextMenuItemData& item)
{
    ContextMenuItem coreItem(ActionType, static_cast<ContextMenuAction>(item.action()), item.title());
    m_page->corePage()->contextMenuController()->contextMenuItemSelected(&coreItem);
}

} // namespace WebKit
