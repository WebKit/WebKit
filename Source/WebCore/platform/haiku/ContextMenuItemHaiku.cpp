/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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

#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <String.h>


using namespace WebCore;

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription item)
{
    m_platformDescription = item;
}

ContextMenuItem::ContextMenuItem(ContextMenu* subMenu)
{
    m_platformDescription = new BMenuItem(subMenu->platformDescription(),
                                          new BMessage(ContextMenuItemTagNoAction));
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action,
                                 const String& title, ContextMenu* subMenu)
{
    if (type == ActionType)
        m_platformDescription = new BMenuItem(BString(title).String(), new BMessage(action));
    else if (type == SeparatorType)
        m_platformDescription = new BSeparatorItem();
    else {
        m_platformDescription = new BMenuItem(subMenu->platformDescription(), new BMessage(action));
        m_platformDescription->SetLabel(BString(title).String());
    }
}

ContextMenuItem::~ContextMenuItem()
{
    delete m_platformDescription;
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    BMenuItem* item = m_platformDescription;
    m_platformDescription = 0;
    return item;
}

ContextMenuItemType ContextMenuItem::type() const
{
    if (dynamic_cast<BSeparatorItem*>(m_platformDescription))
        return SeparatorType;
    if (m_platformDescription->Submenu())
        return SubmenuType;
    return ActionType;
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    ContextMenuAction theAction = action();
    String theTitle = title();
    BMenu* subMenu = platformSubMenu();
    delete m_platformDescription;

    if (type == ActionType)
        m_platformDescription = new BMenuItem(BString(theTitle).String(), new BMessage(theAction));
    else if (type == SeparatorType)
        m_platformDescription = new BSeparatorItem();
    else {
        if (subMenu) {
            m_platformDescription = new BMenuItem(subMenu, new BMessage(theAction));
            m_platformDescription->SetLabel(BString(theTitle).String());
        } else
            m_platformDescription = new BMenuItem(BString(theTitle).String(), new BMessage(theAction));
    }
}

ContextMenuAction ContextMenuItem::action() const
{
    if (!m_platformDescription)
        return ContextMenuItemTagNoAction;
    return static_cast<WebCore::ContextMenuAction>(m_platformDescription->Message()->what);
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    if (m_platformDescription)
        m_platformDescription->Message()->what = action;
}

String ContextMenuItem::title() const
{
    if (m_platformDescription)
        return "";
    return BString(m_platformDescription->Label());
}

void ContextMenuItem::setTitle(const String& title)
{
    // FIXME: We need to find a better way to convert WebKit Strings into c strings
    m_platformDescription->SetLabel(BString(title).String());
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    return m_platformDescription->Submenu();
}

void ContextMenuItem::setSubMenu(ContextMenu* menu)
{
    // FIXME: We assume m_platformDescription is valid
    const char* title = m_platformDescription->Label();
    delete m_platformDescription;
    m_platformDescription = new BMenuItem(menu->platformDescription(), new BMessage(action()));
    m_platformDescription->SetLabel(title);
}

void ContextMenuItem::setChecked(bool checked)
{
    if (m_platformDescription)
        m_platformDescription->SetMarked(checked);
}

void ContextMenuItem::setEnabled(bool enable)
{
    if (m_platformDescription)
        m_platformDescription->SetEnabled(enable);
}

bool ContextMenuItem::enabled() const
{
    if (!m_platformDescription)
        return true;
    return m_platformDescription->IsEnabled();
}

