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

@interface WebCoreMenuTarget : NSObject {
    WebCore::ContextMenuController* _menuController;
}
+ (WebCoreMenuTarget*)sharedMenuTarget;
- (WebCore::ContextMenuController*)menuController;
- (void)setMenuController:(WebCore::ContextMenuController*)menuController;
- (void)forwardContextMenuAction:(id)sender;
@end

static WebCoreMenuTarget* target;

@implementation WebCoreMenuTarget

+ (WebCoreMenuTarget*)sharedMenuTarget
{
    if (!target)
        target = [[WebCoreMenuTarget alloc] init];
    return target;
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
    WebCore::ContextMenuItem item(WebCore::ActionType, static_cast<WebCore::ContextMenuAction>([sender tag]), [sender title], _menuController->contextMenu());
    _menuController->contextMenuItemSelected(&item);
}

@end

namespace WebCore {

ContextMenu::ContextMenu(const HitTestResult& result)
    : m_hitTestResult(result)
{
    NSMutableArray* array = [[NSMutableArray alloc] init];
    m_platformDescription = array;
    [array release];

    [[WebCoreMenuTarget sharedMenuTarget] setMenuController:controller()];
}

ContextMenu::ContextMenu(const HitTestResult& result, const PlatformMenuDescription menu)
    : m_hitTestResult(result)
    , m_platformDescription(menu)
{
    [[WebCoreMenuTarget sharedMenuTarget] setMenuController:controller()];
}

ContextMenu::~ContextMenu()
{
}
 
static void setMenuItemTarget(const ContextMenuItem& item)
{
    NSMenuItem* menuItem = item.platformDescription();
    if (item.type() == ActionType) {
        [menuItem setTarget:[WebCoreMenuTarget sharedMenuTarget]];
        [menuItem setAction:@selector(forwardContextMenuAction:)];
    }
}

void ContextMenu::appendItem(const ContextMenuItem& item)
{
    if (!item.platformDescription())
        return;
    setMenuItemTarget(item);
    [m_platformDescription.get() addObject:item.platformDescription()];
}

void ContextMenu::insertItem(unsigned position, const ContextMenuItem& item)
{
    if (!item.platformDescription())
        return;
    setMenuItemTarget(item);
    [m_platformDescription.get() insertObject:item.platformDescription() atIndex:position];
}

unsigned ContextMenu::itemCount() const
{
    return [m_platformDescription.get() count];
}

void ContextMenu::setPlatformDescription(NSMutableArray* menu)
{
    if (m_platformDescription.get() != menu)
        m_platformDescription = menu;
}

NSMutableArray* ContextMenu::platformDescription() const
{
    return m_platformDescription.get();
}

void ContextMenu::show()
{
}

void ContextMenu::hide()
{
}

}
