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

    IntPoint point = view->contentsToWindow(menu->hitTestResult().point());
    Vector<ContextMenuItem> items = contextMenuItemVector(menu->platformDescription());

    m_page->send(Messages::WebPageProxy::ShowContextMenu(point, kitItems(items, menu)));
}

void WebContextMenu::itemSelected(const WebContextMenuItemData& item)
{
    ContextMenuItem coreItem(ActionType, static_cast<ContextMenuAction>(item.action()), item.title());
    m_page->corePage()->contextMenuController()->contextMenuItemSelected(&coreItem);
}

} // namespace WebKit
