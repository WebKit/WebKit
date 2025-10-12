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

#include "config.h"
#include "WebPopupMenu.h"

#include "PlatformPopupMenuData.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/LocalFrameView.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PopupMenuClient.h>

namespace WebKit {
using namespace WebCore;

Ref<WebPopupMenu> WebPopupMenu::create(WebPage* page, PopupMenuClient* client)
{
    return adoptRef(*new WebPopupMenu(page, client));
}

WebPopupMenu::WebPopupMenu(WebPage* page, PopupMenuClient* client)
    : m_popupClient(client)
    , m_page(page)
{
}

WebPopupMenu::~WebPopupMenu()
{
}

WebPage* WebPopupMenu::page()
{
    return m_page.get();
}

void WebPopupMenu::disconnectClient()
{
    m_popupClient = nullptr;
}

void WebPopupMenu::didChangeSelectedIndex(int newIndex)
{
    CheckedPtr popupClient = m_popupClient;
    if (!popupClient)
        return;

    popupClient->popupDidHide();
    if (newIndex >= 0)
        popupClient->valueChanged(newIndex);
}

void WebPopupMenu::setTextForIndex(int index)
{
    if (CheckedPtr popupClient = m_popupClient)
        popupClient->setTextFromItem(index);
}

Vector<WebPopupItem> WebPopupMenu::populateItems()
{
    CheckedPtr popupClient = m_popupClient;
    return Vector<WebPopupItem>(popupClient->listSize(), [&](size_t i) {
        if (popupClient->itemIsSeparator(i))
            return WebPopupItem(WebPopupItem::Type::Separator);
        // FIXME: Add support for styling the font.
        // FIXME: Add support for styling the foreground and background colors.
        // FIXME: Find a way to customize text color when an item is highlighted.
        PopupMenuStyle itemStyle = popupClient->itemStyle(i);
        return WebPopupItem(WebPopupItem::Type::Item, popupClient->itemText(i), itemStyle.language(), itemStyle.textDirection(), itemStyle.hasTextDirectionOverride(), popupClient->itemToolTip(i), popupClient->itemAccessibilityText(i), popupClient->itemIsEnabled(i), popupClient->itemIsLabel(i), popupClient->itemIsSelected(i));
    });
}

void WebPopupMenu::show(const IntRect& rect, LocalFrameView& view, int selectedIndex)
{
    // FIXME: We should probably inform the client to also close the menu.
    Vector<WebPopupItem> items = populateItems();
    CheckedPtr popupClient = m_popupClient;
    RefPtr page = m_page.get();

    if (items.isEmpty() || !page) {
        popupClient->popupDidHide();
        return;
    }

    RELEASE_ASSERT_WITH_MESSAGE(selectedIndex == -1 || static_cast<unsigned>(selectedIndex) < items.size(), "Invalid selectedIndex (%d) for popup menu with %zu items", selectedIndex, items.size());

    page->setActivePopupMenu(this);

    // Move to page coordinates
    IntRect pageCoordinates(view.contentsToWindow(rect.location()), rect.size());

    PlatformPopupMenuData platformData;
    setUpPlatformData(pageCoordinates, platformData);

    WebProcess::singleton().protectedParentProcessConnection()->send(Messages::WebPageProxy::ShowPopupMenuFromFrame(view.frame().frameID(), pageCoordinates, static_cast<uint64_t>(popupClient->menuStyle().textDirection()), items, selectedIndex, platformData), page->identifier());
}

void WebPopupMenu::hide()
{
    RefPtr page = m_page.get();
    CheckedPtr popupClient = m_popupClient;
    if (!page || !popupClient)
        return;

    WebProcess::singleton().protectedParentProcessConnection()->send(Messages::WebPageProxy::HidePopupMenu(), page->identifier());
    page->setActivePopupMenu(nullptr);
    popupClient->popupDidHide();
}

void WebPopupMenu::updateFromElement()
{
}

#if !PLATFORM(COCOA) && !PLATFORM(WIN)
void WebPopupMenu::setUpPlatformData(const WebCore::IntRect&, PlatformPopupMenuData&)
{
    notImplemented();
}
#endif

} // namespace WebKit
