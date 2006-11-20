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
#include "ContextMenu.h"

#include "ContextMenuController.h"
 
@interface MenuTarget : NSObject {
    WebCore::ContextMenuController* _menuController;
}
- (WebCore::ContextMenuController*)menuController;
- (void)setMenuController:(WebCore::ContextMenuController*)menuController;
- (void)forwardContextMenuAction:(id)sender;
@end

@implementation MenuTarget

- (id)initWithContextMenuController:(WebCore::ContextMenuController*)menuController
{
    self = [super init];
    if (!self)
        return nil;
    
    _menuController = menuController;
    return self;
}

- (WebCore::ContextMenuController*)menuController
{
    return _menuController;
}

- (void)setMenuController:(WebCore::ContextMenuController*)menuController
{
    _menuController = menuController;
}

- (void)forwardContextMenuAction:(id)sender
{
    WebCore::ContextMenuAction action = static_cast<WebCore::ContextMenuAction>([sender tag]);
    _menuController->contextMenuActionSelected(action, [sender title]);
}

@end

using namespace WebCore;

static MenuTarget* target;
 
static NSMenuItem* getNSMenuItem(ContextMenu* menu, ContextMenuItem item)
{
    if (!menu->platformMenuDescription())
        menu->setPlatformMenuDescription([[[NSMutableArray alloc] init] autorelease]);
    
    ContextMenuController* currentController = menu->controller();
    if (!target)
        target = [[MenuTarget alloc] initWithContextMenuController:currentController];
    else if (currentController != [target menuController])
        [target setMenuController:currentController];
    
    NSMenuItem* menuItem = [[[NSMenuItem alloc] init] autorelease];
    [menuItem setTag: item.action];
    [menuItem setTitle:item.title];
    [menuItem setTarget:target];
    [menuItem setAction:@selector(forwardContextMenuAction:)];
    
    return menuItem;
}

void ContextMenu::appendItem(ContextMenuItem item)
{
    NSMenuItem* menuItem = getNSMenuItem(this, item);
    [m_menu addObject:menuItem];
}

unsigned ContextMenu::itemCount()
{
    return [m_menu count];
}

void ContextMenu::insertItem(unsigned position, ContextMenuItem item)
{
    NSMenuItem* menuItem = getNSMenuItem(this, item);
    [m_menu insertObject:menuItem atIndex:position];
}

void ContextMenu::setPlatformMenuDescription(NSMutableArray* menu)
{
    m_menu = menu;
}

void ContextMenu::show()
{
}

void ContextMenu::hide()
{
}
