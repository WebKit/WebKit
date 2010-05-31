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

ContextMenu::ContextMenu(const HitTestResult& result)
    : m_hitTestResult(result)
{
    m_contextMenuClient = static_cast<ContextMenuClientEfl*>(controller()->client());
    m_platformDescription = m_contextMenuClient->createPlatformDescription(this);
}

ContextMenu::ContextMenu(const HitTestResult& result, const PlatformMenuDescription menu)
    : m_hitTestResult(result)
    , m_platformDescription(menu)
{
    m_contextMenuClient = static_cast<ContextMenuClientEfl*>(controller()->client());
}

ContextMenu::~ContextMenu()
{
    if (m_platformDescription)
        m_contextMenuClient->freePlatformDescription(m_platformDescription);
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    checkOrEnableIfNeeded(item);
    m_contextMenuClient->appendItem(m_platformDescription, item);
}

void ContextMenu::setPlatformDescription(PlatformMenuDescription menu)
{
    ASSERT(!m_platformDescription);

    m_platformDescription = menu;
    m_contextMenuClient->show(m_platformDescription);
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
