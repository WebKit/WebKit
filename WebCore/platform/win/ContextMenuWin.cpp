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
#include "ContextMenu.h"

#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "Node.h"
#include <tchar.h>
#include <windows.h>
#include <wtf/text/CString.h>

#ifndef MIIM_FTYPE
#define MIIM_FTYPE MIIM_TYPE
#endif
#ifndef MIIM_STRING
#define MIIM_STRING MIIM_TYPE
#endif

namespace WebCore {

ContextMenu::ContextMenu(const HitTestResult& result)
    : m_hitTestResult(result)
    , m_platformDescription(0)
#if OS(WINCE)
    , m_itemCount(0)
#endif
{
    setPlatformDescription(::CreatePopupMenu());
}

ContextMenu::ContextMenu(const HitTestResult& result, const PlatformMenuDescription menu)
    : m_hitTestResult(result)
    , m_platformDescription(0)
#if OS(WINCE)
    , m_itemCount(0)
#endif
{
    setPlatformDescription(menu);
}

ContextMenu::~ContextMenu()
{
    if (m_platformDescription)
        ::DestroyMenu(m_platformDescription);
}

unsigned ContextMenu::itemCount() const
{
#if OS(WINCE)
    return m_itemCount;
#else
    if (!m_platformDescription)
        return 0;

    return ::GetMenuItemCount(m_platformDescription);
#endif
}

#if OS(WINCE)
static bool insertMenuItem(PlatformMenuDescription menu, unsigned int position, ContextMenuItem& item)
{
    UINT flags = MF_BYPOSITION;
    UINT newItem = 0;
    LPCWSTR title = 0;

    if (item.type() == SeparatorType)
        flags |= MF_SEPARATOR;
    else {
        flags |= MF_STRING;
        flags |= item.checked() ? MF_CHECKED : MF_UNCHECKED;
        flags |= item.enabled() ? MF_ENABLED : MF_GRAYED;

        PlatformMenuItemDescription description = item.releasePlatformDescription();
        title = description->dwTypeData;
        description->dwTypeData = 0;

        if (description->hSubMenu) {
            flags |= MF_POPUP;
            newItem = reinterpret_cast<UINT>(description->hSubMenu);
            description->hSubMenu = 0;
        } else
            newItem = description->wID;

        free(description);
    }

    return ::InsertMenuW(menu, position, flags, newItem, title);
}
#endif

void ContextMenu::insertItem(unsigned int position, ContextMenuItem& item)
{
    if (!m_platformDescription)
        return;

    checkOrEnableIfNeeded(item);

#if OS(WINCE)
    if (insertMenuItem(m_platformDescription, position, item))
        ++m_itemCount;
#else
    ::InsertMenuItem(m_platformDescription, position, TRUE, item.releasePlatformDescription());
#endif
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    insertItem(itemCount(), item);
}

static ContextMenuItem* contextMenuItemByIdOrPosition(HMENU menu, unsigned id, BOOL byPosition)
{
    if (!menu)
        return 0;
    LPMENUITEMINFO info = static_cast<LPMENUITEMINFO>(malloc(sizeof(MENUITEMINFO)));
    if (!info)
        return 0;

    memset(info, 0, sizeof(MENUITEMINFO));

    info->cbSize = sizeof(MENUITEMINFO);

    // Setting MIIM_DATA which is useful for WebKit clients who store data in this member for their custom menu items.
    info->fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_DATA;

    if (!::GetMenuItemInfo(menu, id, byPosition, info)) {
        free(info);
        return 0;
    }

    if (info->fType & MFT_STRING) {
        LPTSTR buffer = static_cast<LPTSTR>(malloc(++info->cch * sizeof(TCHAR)));
        if (!buffer) {
            free(info);
            return 0;
        }
        info->dwTypeData = buffer;
        ::GetMenuItemInfo(menu, id, byPosition, info);
    }

    return new ContextMenuItem(info);
}

ContextMenuItem* ContextMenu::itemWithAction(unsigned action)
{
    return contextMenuItemByIdOrPosition(m_platformDescription, action, FALSE);
}

ContextMenuItem* ContextMenu::itemAtIndex(unsigned index, const PlatformMenuDescription platformDescription)
{
    return contextMenuItemByIdOrPosition(platformDescription, index, TRUE);
}

void ContextMenu::setPlatformDescription(HMENU menu)
{
    if (menu == m_platformDescription)
        return;
    
    if (m_platformDescription)
        ::DestroyMenu(m_platformDescription);

    m_platformDescription = menu;
    if (!m_platformDescription)
        return;

#if !OS(WINCE)
    MENUINFO menuInfo = {0};
    menuInfo.cbSize = sizeof(MENUINFO);
    menuInfo.fMask = MIM_STYLE;
    ::GetMenuInfo(m_platformDescription, &menuInfo);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle |= MNS_NOTIFYBYPOS;
    ::SetMenuInfo(m_platformDescription, &menuInfo);
#endif
}

HMENU ContextMenu::platformDescription() const
{
    return m_platformDescription;
}

HMENU ContextMenu::releasePlatformDescription()
{
    HMENU description = m_platformDescription;
    m_platformDescription = 0;
    return description;
}

Vector<ContextMenuItem> contextMenuItemVector(PlatformMenuDescription)
{
    // FIXME - Implement
    return Vector<ContextMenuItem>();
}

PlatformMenuDescription platformMenuDescription(Vector<ContextMenuItem>& menuItemVector)
{
    // FIXME - Implement
    return 0;
}

} // namespace WebCore
