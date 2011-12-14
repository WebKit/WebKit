/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 *  Copyright (C) 2009-2010 ProFUSION embedded systems
 *  Copyright (C) 2011 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ContextMenu.h"

#include "NotImplemented.h"

namespace WebCore {

#if USE(CROSS_PLATFORM_CONTEXT_MENUS)
ContextMenu::ContextMenu(void* menu)
{
    getContextMenuItems(menu, m_items);
}

void ContextMenu::getContextMenuItems(void* menu, Vector<ContextMenuItem>& items)
{
    notImplemented();
}

void* ContextMenu::createNativeMenuFromItems(const Vector<ContextMenuItem>& items)
{
    notImplemented();
    return 0;
}

void* ContextMenu::nativeMenu() const
{
    return createNativeMenuFromItems(m_items);
}
#else
ContextMenu::ContextMenu()
{
    notImplemented();
}
#endif

}
