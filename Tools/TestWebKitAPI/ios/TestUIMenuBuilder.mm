/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TestUIMenuBuilder.h"

#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

@implementation TestUIMenuBuilder {
    RetainPtr<NSMutableDictionary<UIMenuIdentifier, UIMenu *>> _menusByIdentifier;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    // FIXME: A flat dictionary containing all UIMenus is sufficient to test all of the
    // non-default items currently supplied by WebKit. We'll want to augment this in the
    // future if we need to test menu nesting or item ordering.
    _menusByIdentifier = adoptNS([NSMutableDictionary<UIMenuIdentifier, UIMenu *> new]);
    return self;
}

- (UIMenuSystem *)system
{
    return UIMenuSystem.mainSystem;
}

- (UIMenu *)menuForIdentifier:(UIMenuIdentifier)identifier
{
    return [_menusByIdentifier objectForKey:identifier];
}

- (UIMenuElement *)findMatching:(BOOL(^)(UIMenuElement *))isMatch
{
    for (UIMenu *menu in [_menusByIdentifier allValues]) {
        for (UIMenuElement *child in menu.children) {
            if (isMatch(child))
                return child;
        }
    }
    return nil;
}

- (UIAction *)actionWithTitle:(NSString *)title
{
    return (UIAction *)[self findMatching:^BOOL(UIMenuElement *element) {
        return [dynamic_objc_cast<UIAction>(element).title isEqual:title];
    }];
}

- (void)reset
{
    [_menusByIdentifier removeAllObjects];
}

- (UIAction *)actionForIdentifier:(UIActionIdentifier)identifier
{
    return (UIAction *)[self findMatching:^BOOL(UIMenuElement *element) {
        return [dynamic_objc_cast<UIAction>(element).identifier isEqual:identifier];
    }];
}

- (UICommand *)commandForAction:(SEL)action propertyList:(id)propertyList
{
    return (UICommand *)[self findMatching:^BOOL(UIMenuElement *element) {
        auto command = dynamic_objc_cast<UICommand>(element);
        return command.action == action && (command.propertyList == propertyList || [command.propertyList isEqual:propertyList]);
    }];
}

- (void)replaceMenuForIdentifier:(UIMenuIdentifier)replacedIdentifier withMenu:(UIMenu *)replacementMenu
{
    [_menusByIdentifier setObject:replacementMenu forKey:replacedIdentifier];
}

- (void)replaceChildrenOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier fromChildrenBlock:(NSArray<UIMenuElement *> *(NS_NOESCAPE^)(NSArray<UIMenuElement *> *))childrenBlock
{
    if (auto menu = [self menuForIdentifier:parentIdentifier])
        [_menusByIdentifier setObject:[menu menuByReplacingChildren:childrenBlock(menu.children)] forKey:parentIdentifier];
}

- (void)insertChildMenu:(UIMenu *)childMenu atStartOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier
{
    [self registerMenu:childMenu];
}

- (void)insertChildMenu:(UIMenu *)childMenu atEndOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier
{
    [self registerMenu:childMenu];
}

- (void)removeMenuForIdentifier:(UIMenuIdentifier)removedIdentifier
{
    [_menusByIdentifier removeObjectForKey:removedIdentifier];
}

- (void)insertSiblingMenu:(UIMenu *)siblingMenu beforeMenuForIdentifier:(UIMenuIdentifier)siblingIdentifier
{
    [self registerMenu:siblingMenu];
}

- (void)insertSiblingMenu:(UIMenu *)siblingMenu afterMenuForIdentifier:(UIMenuIdentifier)siblingIdentifier
{
    [self registerMenu:siblingMenu];
}

- (void)registerMenu:(UIMenu *)menu
{
    [_menusByIdentifier setObject:menu forKey:menu.identifier];
}

@end

#endif // PLATFORM(IOS_FAMILY)
