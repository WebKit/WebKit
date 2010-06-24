/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 * Copyright (C) 2010 Company 100, Inc.
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
 *
 */

#include "config.h"
#include "PopupMenu.h"

#include "Chrome.h"
#include "ChromeClientBrew.h"
#include "FrameView.h"
#include "NotImplemented.h"

namespace WebCore {

PopupMenu::PopupMenu(PopupMenuClient* menuList)
    : m_popupClient(menuList)
    , m_view(0)
{
}

PopupMenu::~PopupMenu()
{
}

void PopupMenu::show(const IntRect& rect, FrameView* view, int index)
{
    ASSERT(m_popupClient);
    ChromeClientBrew* chromeClient = static_cast<ChromeClientBrew*>(view->frame()->page()->chrome()->client());
    ASSERT(chromeClient);

    m_view = view;
    chromeClient->createSelectPopup(m_popupClient, index, rect);
}

void PopupMenu::hide()
{
    ASSERT(m_view);
    ChromeClientBrew* chromeClient = static_cast<ChromeClientBrew*>(m_view->frame()->page()->chrome()->client());
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

} // namespace WebCore
