/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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

namespace WebCore {

// Now we handle all of this in WebViewCore.cpp.
PopupMenu::PopupMenu(PopupMenuClient* menuList)
    : m_popupClient(menuList)
{
}

PopupMenu::~PopupMenu()
{
}

void PopupMenu::show(const IntRect&, FrameView*, int)
{
}

void PopupMenu::hide()
{
}

void PopupMenu::updateFromElement() 
{
}

bool PopupMenu::itemWritingDirectionIsNatural()
{
    return false;
}

} // namespace WebCore
