/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContextMenuItem.h"

#include "ContextMenu.h"
#include "NotImplemented.h"

namespace WebCore {

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription)
{
    // It's inside WebKit that this initialization is done, as WebCore doesn't
    // know how PlatformMenuItemDescription is implemented.
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenu* submenu)
{
    m_platformDescription.type = SubmenuType;
    setSubMenu(submenu);
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
{
    m_platformDescription.type = type;
    m_platformDescription.action = action;
    m_platformDescription.title = String(title);

    setSubMenu(subMenu);
}

ContextMenuItem::~ContextMenuItem()
{
    if (m_platformDescription.subMenu)
        delete m_platformDescription.subMenu;
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    return m_platformDescription;
}

ContextMenuItemType ContextMenuItem::type() const
{
    return m_platformDescription.type;
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    m_platformDescription.type = type;
}

ContextMenuAction ContextMenuItem::action() const
{
    return m_platformDescription.action;
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    m_platformDescription.action = action;
}

String ContextMenuItem::title() const
{
    return m_platformDescription.title;
}

void ContextMenuItem::setTitle(const String& title)
{
    m_platformDescription.title = String(title);
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    if (!m_platformDescription.subMenu)
        return 0;

    return m_platformDescription.subMenu->platformDescription();

}

void ContextMenuItem::setSubMenu(ContextMenu* subMenu)
{
    delete m_platformDescription.subMenu;
    m_platformDescription.subMenu = 0;

    if (!subMenu)
        return;

    m_platformDescription.type = SubmenuType;
    m_platformDescription.subMenu = new ContextMenu(subMenu->hitTestResult(),
            subMenu->releasePlatformDescription());
}

bool ContextMenuItem::checked() const
{
    return m_platformDescription.checked;
}

void ContextMenuItem::setChecked(bool shouldCheck)
{
    m_platformDescription.checked = shouldCheck;
}

bool ContextMenuItem::enabled() const
{
    return m_platformDescription.enabled;
}

void ContextMenuItem::setEnabled(bool shouldEnable)
{
    m_platformDescription.enabled = shouldEnable;
}

}
