/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include <wtf/text/CString.h>
#include <windows.h>

#if OS(WINCE)
#ifndef MFS_DISABLED
#define MFS_DISABLED MF_GRAYED
#endif
#ifndef MIIM_FTYPE
#define MIIM_FTYPE MIIM_TYPE
#endif
#ifndef MIIM_STRING
#define MIIM_STRING 0
#endif
#endif

namespace WebCore {

ContextMenuItem::ContextMenuItem(LPMENUITEMINFO item)
    : m_platformDescription(item)
{
}

ContextMenuItem::ContextMenuItem(ContextMenu* subMenu)
{
    m_platformDescription = (LPMENUITEMINFO)malloc(sizeof(MENUITEMINFO));
    if (!m_platformDescription)
        return;

    memset(m_platformDescription, 0, sizeof(MENUITEMINFO));
    m_platformDescription->cbSize = sizeof(MENUITEMINFO);

    m_platformDescription->wID = ContextMenuItemTagNoAction;
    if (subMenu) {
        m_platformDescription->fMask |= MIIM_SUBMENU;
        m_platformDescription->hSubMenu = subMenu->platformDescription();
    }
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
{
    m_platformDescription = (LPMENUITEMINFO)malloc(sizeof(MENUITEMINFO));
    if (!m_platformDescription)
        return;

    memset(m_platformDescription, 0, sizeof(MENUITEMINFO));
    m_platformDescription->cbSize = sizeof(MENUITEMINFO);
    m_platformDescription->fMask = MIIM_FTYPE;

    if (type == SeparatorType) {
        m_platformDescription->fType = MFT_SEPARATOR;
        return;
    }
    
    if (subMenu) {
        m_platformDescription->fMask |= MIIM_STRING | MIIM_SUBMENU;
        m_platformDescription->hSubMenu = subMenu->platformDescription();
    } else
        m_platformDescription->fMask |= MIIM_STRING | MIIM_ID;

    m_platformDescription->fType = MFT_STRING;
    m_platformDescription->wID = action;
    
    String t = title;
    m_platformDescription->cch = t.length();
    m_platformDescription->dwTypeData = wcsdup(t.charactersWithNullTermination());
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType, ContextMenuAction, const String&, bool, bool)
{
    // FIXME: Implement
}

ContextMenuItem::ContextMenuItem(ContextMenuAction, const String&, bool, bool, Vector<ContextMenuItem>&)
{
    // FIXME: Implement
}

ContextMenuItem::~ContextMenuItem()
{
    if (m_platformDescription) {
        if (m_platformDescription->fType == MFT_STRING)
            free(m_platformDescription->dwTypeData);
        free(m_platformDescription);
    }
}

LPMENUITEMINFO ContextMenuItem::releasePlatformDescription()
{
    LPMENUITEMINFO info = m_platformDescription;
    m_platformDescription = 0;
    return info;
}

ContextMenuItemType ContextMenuItem::type() const
{
    ContextMenuItemType type = ActionType;
    if (((m_platformDescription->fType & ~MFT_OWNERDRAW) == MFT_STRING) && m_platformDescription->hSubMenu)
        type = SubmenuType;
    else if (m_platformDescription->fType & MFT_SEPARATOR)
        type = SeparatorType;

    return type;
}

ContextMenuAction ContextMenuItem::action() const
{ 
    return static_cast<ContextMenuAction>(m_platformDescription->wID);
}

String ContextMenuItem::title() const 
{
    return String(m_platformDescription->dwTypeData, wcslen(m_platformDescription->dwTypeData));
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    return m_platformDescription->hSubMenu;
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    if (type == SeparatorType)
        m_platformDescription->fType = MFT_SEPARATOR;
    else
        m_platformDescription->fType = MFT_STRING;
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    m_platformDescription->wID = action; 
}

void ContextMenuItem::setTitle(const String& title)
{
    if (m_platformDescription->dwTypeData)
        free(m_platformDescription->dwTypeData);
    
    m_platformDescription->cch = title.length();
    String titleCopy = title;
    m_platformDescription->dwTypeData = wcsdup(titleCopy.charactersWithNullTermination());
}

void ContextMenuItem::setSubMenu(ContextMenu* subMenu)
{
    if (subMenu->platformDescription() == m_platformDescription->hSubMenu)
        return;

    if (m_platformDescription->hSubMenu)
        ::DestroyMenu(m_platformDescription->hSubMenu);

    m_platformDescription->fMask |= MIIM_SUBMENU;
    m_platformDescription->hSubMenu = subMenu->releasePlatformDescription();
}

void ContextMenuItem::setSubMenu(Vector<ContextMenuItem>&)
{
    // FIXME: Implement
}

void ContextMenuItem::setChecked(bool checked)
{
    m_platformDescription->fMask |= MIIM_STATE;
    if (checked) {
        m_platformDescription->fState &= ~MFS_UNCHECKED;
        m_platformDescription->fState |= MFS_CHECKED;
    } else {
        m_platformDescription->fState &= ~MFS_CHECKED;
        m_platformDescription->fState |= MFS_UNCHECKED;
    }
}

bool ContextMenuItem::checked() const
{
    // FIXME - Implement
    return false;
}

void ContextMenuItem::setEnabled(bool enabled)
{
    m_platformDescription->fMask |= MIIM_STATE;
    if (enabled) {
        m_platformDescription->fState &= ~MFS_DISABLED;
        m_platformDescription->fState |= MFS_ENABLED;
    } else {
        m_platformDescription->fState &= ~MFS_ENABLED;
        m_platformDescription->fState |= MFS_DISABLED;
    }
}

bool ContextMenuItem::enabled() const
{
    return !(m_platformDescription->fState & MFS_DISABLED);
}

}
