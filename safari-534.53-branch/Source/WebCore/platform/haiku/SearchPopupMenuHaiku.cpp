/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "SearchPopupMenuHaiku.h"

#include "NotImplemented.h"
#include <wtf/text/AtomicString.h>


namespace WebCore {

SearchPopupMenuHaiku::SearchPopupMenuHaiku(PopupMenuClient* client)
    : m_popup(adoptRef(new PopupMenuHaiku(client)))
{
}

void SearchPopupMenuHaiku::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems)
{
    notImplemented();
}

void SearchPopupMenuHaiku::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems)
{
    notImplemented();
}

bool SearchPopupMenuHaiku::enabled()
{
    notImplemented();
    return false;
}

PopupMenu* SearchPopupMenuHaiku::popupMenu()
{
    return m_popup.get();
}

} // namespace WebCore

