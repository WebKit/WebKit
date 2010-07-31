/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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
#include "PopupMenu.h"

#include "Chrome.h"
#include "ChromeClientEfl.h"
#include "FrameView.h"
#include "NotImplemented.h"

namespace WebCore {

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
    , m_view(0)
{
}

PopupMenu::~PopupMenu()
{
    // Tell client to destroy data related to this popup since this object is
    // going away.
    hide();
}

void PopupMenu::show(const IntRect& rect, FrameView* view, int index)
{
    ASSERT(m_popupClient);
    ChromeClientEfl* chromeClient = static_cast<ChromeClientEfl*>(view->frame()->page()->chrome()->client());
    ASSERT(chromeClient);

    m_view = view;
    chromeClient->createSelectPopup(m_popupClient, index, rect);
}

void PopupMenu::hide()
{
    ASSERT(m_view);
    ChromeClientEfl* chromeClient = static_cast<ChromeClientEfl*>(m_view->frame()->page()->chrome()->client());
    ASSERT(chromeClient);

    chromeClient->destroySelectPopup();
}

void PopupMenu::updateFromElement()
{
    client()->setTextFromItem(client()->selectedIndex());
}

bool PopupMenu::itemWritingDirectionIsNatural()
{
    return true;
}

}
