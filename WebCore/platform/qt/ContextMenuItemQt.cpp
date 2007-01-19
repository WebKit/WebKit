/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
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

namespace WebCore {

ContextMenuItem::ContextMenuItem(ContextMenu* subMenu)
{
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action,
                                 const String& title, ContextMenu* subMenu)
{
}

ContextMenuItem::~ContextMenuItem()
{
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    return PlatformMenuItemDescription();
}

ContextMenuItemType ContextMenuItem::type() const
{
    return ActionType;
}

void ContextMenuItem::setType(ContextMenuItemType)
{
}

ContextMenuAction ContextMenuItem::action() const
{ 
    return ContextMenuAction();
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
}

String ContextMenuItem::title() const 
{
    return String();
}

void ContextMenuItem::setTitle(const String& title)
{
}


PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    return PlatformMenuDescription();
}

void ContextMenuItem::setSubMenu(ContextMenu* menu)
{
}

void ContextMenuItem::setChecked(bool)
{
}

void ContextMenuItem::setEnabled(bool)
{
}

bool ContextMenuItem::enabled() const
{
    return true;
}

}
