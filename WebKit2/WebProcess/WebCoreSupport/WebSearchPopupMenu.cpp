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


#include "WebSearchPopupMenu.h"

namespace WebKit {

PassRefPtr<WebSearchPopupMenu> WebSearchPopupMenu::create(WebCore::PopupMenuClient* client)
{
    return adoptRef(new WebSearchPopupMenu(client));
}

WebSearchPopupMenu::WebSearchPopupMenu(WebCore::PopupMenuClient* client)
    : m_popup(WebPopupMenu::create(client))
{
}

WebCore::PopupMenu* WebSearchPopupMenu::popupMenu()
{
    return m_popup.get();
}

void WebSearchPopupMenu::saveRecentSearches(const AtomicString&, const Vector<String>&)
{
}

void WebSearchPopupMenu::loadRecentSearches(const AtomicString&, Vector<String>&)
{
}

bool WebSearchPopupMenu::enabled()
{
    return false;
}

} // namespace WebKit
