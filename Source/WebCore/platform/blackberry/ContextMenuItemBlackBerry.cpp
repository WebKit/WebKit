/*
 * Copyright (C) 2009 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ContextMenuItem.h"

#if ENABLE(CONTEXT_MENUS)
#include "ContextMenu.h"
#include "NotImplemented.h"

namespace WebCore {

ContextMenuItem::ContextMenuItem(ContextMenuItemType, ContextMenuAction, String const&, ContextMenu*)
{
    notImplemented();
}

ContextMenuItem::~ContextMenuItem()
{
    notImplemented();
}

void ContextMenuItem::setChecked(bool)
{
    notImplemented();
}

void ContextMenuItem::setEnabled(bool)
{
    notImplemented();
}

void ContextMenuItem::setSubMenu(ContextMenu*)
{
    notImplemented();
}

void ContextMenuItem::setTitle(String const&)
{
    notImplemented();
}

ContextMenuAction ContextMenuItem::action() const
{
    notImplemented();
    return ContextMenuItemTagNoAction;
}

ContextMenuItemType ContextMenuItem::type() const
{
    notImplemented();
    return ActionType;
}

String ContextMenuItem::title() const
{
    notImplemented();
    return String();
}

} // namespace WebCore
#endif
