/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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

#include "FrameView.h"

#include "NotImplemented.h"
#include <Menu.h>


namespace WebCore {

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
    , m_menu(0)
{
}

PopupMenu::~PopupMenu()
{
    delete m_menu;
    m_menu = 0;
}

void PopupMenu::show(const IntRect& rect, FrameView* view, int index)
{
    notImplemented();
}

void PopupMenu::hide()
{
    notImplemented();
}

void PopupMenu::updateFromElement()
{
    notImplemented();
}

bool PopupMenu::itemWritingDirectionIsNatural()
{
    notImplemented();
    return true;
}

} // namespace WebCore

