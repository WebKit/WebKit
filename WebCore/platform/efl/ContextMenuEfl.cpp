/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 *  Copyright (C) 2009-2010 ProFUSION embedded systems
 *  Copyright (C) 2009-2010 Samsung Electronics
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

#include "ContextMenuClient.h"
#include "ContextMenuClientEfl.h"
#include "ContextMenuController.h"
#include "NotImplemented.h"

namespace WebCore {

ContextMenu::ContextMenu()
{
    m_platformDescription = (PlatformMenuDescription) ewk_context_menu_new(m_view, menu->controller());
}

ContextMenu::ContextMenu(const PlatformMenuDescription menu)
    : m_platformDescription(menu)
{
}

ContextMenu::~ContextMenu()
{
    if (m_platformDescription)
        ewk_context_menu_free(static_cast<Ewk_Context_Menu*>(m_platformDescription));
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    ewk_context_menu_item_append(static_cast<Ewk_Context_Menu*>(m_platformDescription), item);
}

void ContextMenu::setPlatformDescription(PlatformMenuDescription menu)
{
    ASSERT(!m_platformDescription);

    m_platformDescription = menu;
    ewk_context_menu_show(static_cast<Ewk_Context_Menu*>(m_platformDescription));
}

PlatformMenuDescription ContextMenu::platformDescription() const
{
    return m_platformDescription;
}

PlatformMenuDescription ContextMenu::releasePlatformDescription()
{
    // Ref count remains the same, just pass it and remove our ref, so it
    // will not be decremented when this object goes away.
    PlatformMenuDescription description = m_platformDescription;
    m_platformDescription = 0;

    return description;
}

}
