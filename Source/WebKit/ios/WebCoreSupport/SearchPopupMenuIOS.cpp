/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 */

#include "SearchPopupMenuIOS.h"

#include <wtf/text/AtomicString.h>

#if PLATFORM(IOS)

using namespace WebCore;

SearchPopupMenuIOS::SearchPopupMenuIOS(PopupMenuClient* client)
    : m_popup(adoptRef(new PopupMenuIOS(client)))
{
}

PopupMenu* SearchPopupMenuIOS::popupMenu()
{
    return m_popup.get();
}

void SearchPopupMenuIOS::saveRecentSearches(const AtomicString&, const Vector<String>& /*searchItems*/)
{
}

void SearchPopupMenuIOS::loadRecentSearches(const AtomicString&, Vector<String>& /*searchItems*/)
{
}

bool SearchPopupMenuIOS::enabled()
{
    return false;
}

#endif // !PLATFORM(IOS)
