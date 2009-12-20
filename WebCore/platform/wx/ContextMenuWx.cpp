/*
 * Copyright (C) 2008 Apple Computer, Inc.  All rights reserved.
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
#include "ContextMenuItem.h"
#include "PlatformMenuDescription.h"
#include "wx/menu.h"

#include <wtf/HashMap.h>

using namespace WebCore;

typedef WTF::HashMap<int, ContextMenuAction> ItemActionMap;
static ItemActionMap s_itemActions;

ContextMenuItem* ContextMenu::itemWithId(int id)
{
    return new ContextMenuItem(ActionType, s_itemActions.get(id), "");
}

ContextMenu::ContextMenu(const HitTestResult& result) : m_hitTestResult(result)
{
    m_platformDescription = new wxMenu(0);
}

ContextMenu::~ContextMenu()
{
    if (m_platformDescription)
        delete m_platformDescription;
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    if (!m_platformDescription)
        return;
    
    checkOrEnableIfNeeded(item);
        
    PlatformMenuItemDescription itemDescription = item.releasePlatformDescription();    
    wxItemKind menuKindWx = ( itemDescription.type == CheckableActionType ) ? wxITEM_CHECK : wxITEM_NORMAL;
    wxString titleWx(itemDescription.title);
    int idWx = wxID_ANY;
    wxMenuItem * itemWx;

    ItemActionMap::const_iterator end = s_itemActions.end();
    for (ItemActionMap::const_iterator it = s_itemActions.begin();  it != end; ++it) {
        if (it->second == itemDescription.action)
            idWx = it->first;
    }

    if (itemDescription.subMenu) {
        itemWx = new wxMenuItem(m_platformDescription, idWx, titleWx, wxEmptyString, wxITEM_NORMAL, itemDescription.subMenu);
    } else if (itemDescription.type != SeparatorType) {
        itemWx = new wxMenuItem(m_platformDescription, idWx, titleWx, wxT(""), menuKindWx);
    } else {
        itemWx = new wxMenuItem(m_platformDescription);
    }

    s_itemActions.add(itemWx->GetId(), item.action());
    
    m_platformDescription->Append(itemWx);
    m_platformDescription->Enable(itemWx->GetId(), itemDescription.enabled);
    
    if (menuKindWx == wxITEM_CHECK)
        m_platformDescription->Check(itemWx->GetId(), itemDescription.checked);        
}

void ContextMenu::setPlatformDescription(PlatformMenuDescription menu)
{
    if (!menu)
        return;

    if (m_platformDescription)
        delete m_platformDescription;

    m_platformDescription = menu;
}

PlatformMenuDescription ContextMenu::platformDescription() const
{
    return m_platformDescription;
}

PlatformMenuDescription ContextMenu::releasePlatformDescription()
{
    PlatformMenuDescription description = m_platformDescription;
    m_platformDescription = 0;

    return description;
}
