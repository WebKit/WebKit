/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

ContextMenuItem::ContextMenuItem(NSMenuItem* item, ContextMenu* parentMenu)
    : m_parentMenu(parentMenu)
    , m_platformDescription(item)
{
    if ([item isSeparatorItem])
        m_type = SeparatorType;
    else if ([item hasSubmenu])
        m_type = SubmenuType;
    else
        m_type = ActionType;
}

ContextMenuItem::ContextMenuItem(ContextMenu* parentMenu, ContextMenu* subMenu)
    : m_parentMenu(parentMenu)
    , m_type(SeparatorType)
{
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:String() action:nil keyEquivalent:nil];
    m_platformDescription = item;
    [item release];

    [m_platformDescription.get() setTag:ContextMenuItemTagNoAction];
    if (subMenu)
        setSubMenu(subMenu);
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* parentMenu, 
    ContextMenu* subMenu)
    : m_parentMenu(parentMenu)
    , m_type(type)
{
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:nil];
    m_platformDescription = item;
    [item release];

    [m_platformDescription.get() setTag:action];
    if (subMenu)
        setSubMenu(subMenu);
}

ContextMenuItem::~ContextMenuItem()
{
}

PlatformMenuItemDescription ContextMenuItem::platformDescription() const
{
    return m_platformDescription.get();
}

ContextMenuAction ContextMenuItem::action() const
{ 
    return static_cast<ContextMenuAction>([m_platformDescription.get() tag]);
}

String ContextMenuItem::title() const 
{
    return [m_platformDescription.get() title];
}

NSMutableArray* ContextMenuItem::platformSubMenu() const
{
    if (![m_platformDescription.get() hasSubmenu])
        return nil;

    NSMenu* subMenu = [m_platformDescription.get() submenu];
    NSMutableArray* itemsArray = [NSMutableArray array];
    int total = [subMenu numberOfItems];
    for (int i = 0; i < total; i++)
        [itemsArray addObject:[subMenu itemAtIndex:i]];

    return itemsArray;
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    [m_platformDescription.get() setTag:action]; 
}

void ContextMenuItem::setTitle(String title)
{
    [m_platformDescription.get() setTitle:title];
}

void ContextMenuItem::setSubMenu(ContextMenu* menu)
{
    NSArray* subMenuArray = menu->platformDescription();
    NSMenu* subMenu = [[NSMenu alloc] init];
    for (unsigned i = 0; i < [subMenuArray count]; i++)
        [subMenu insertItem:[subMenuArray objectAtIndex:i] atIndex:i];
    [m_platformDescription.get() setSubmenu:subMenu];
    [subMenu release];
}

}
