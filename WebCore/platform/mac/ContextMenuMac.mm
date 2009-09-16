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

#if ENABLE(CONTEXT_MENUS)

#include "ContextMenuController.h"

@interface WebCoreMenuTarget : NSObject {
    WebCore::ContextMenuController* _menuController;
}
+ (WebCoreMenuTarget*)sharedMenuTarget;
- (WebCore::ContextMenuController*)menuController;
- (void)setMenuController:(WebCore::ContextMenuController*)menuController;
- (void)forwardContextMenuAction:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem *)item;
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
    WebCore::ContextMenuItem item(WebCore::ActionType, static_cast<WebCore::ContextMenuAction>([sender tag]), [sender title]);
    _menuController->contextMenuItemSelected(&item);
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    WebCore::ContextMenuItem coreItem(item);
    ASSERT(_menuController->contextMenu());
    _menuController->contextMenu()->checkOrEnableIfNeeded(coreItem);
    return coreItem.enabled();
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
 
static void setMenuItemTarget(NSMenuItem* menuItem)
{
    [menuItem setTarget:[WebCoreMenuTarget sharedMenuTarget]];
    [menuItem setAction:@selector(forwardContextMenuAction:)];
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    checkOrEnableIfNeeded(item);

    ContextMenuItemType type = item.type();
    NSMenuItem* platformItem = item.releasePlatformDescription();
    if (type == ActionType)
        setMenuItemTarget(platformItem);

    [m_platformDescription.get() addObject:platformItem];
    [platformItem release];
}

void ContextMenu::insertItem(unsigned position, ContextMenuItem& item)
{
    checkOrEnableIfNeeded(item);

    ContextMenuItemType type = item.type();
    NSMenuItem* platformItem = item.releasePlatformDescription();
    if (type == ActionType)
        setMenuItemTarget(platformItem);

    [m_platformDescription.get() insertObject:platformItem atIndex:position];
    [platformItem release];
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

NSMutableArray* ContextMenu::releasePlatformDescription()
{
    return m_platformDescription.releaseRef();
}

} // namespace WebCore

#endif // ENABLE(CONTEXT_MENUS)
