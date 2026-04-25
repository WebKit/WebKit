/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Helpers/mac/NSFontPanelTesting.h"

#if PLATFORM(MAC)

#import "Helpers/mac/AppKitSPI.h"
#import <objc/runtime.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@interface NSBox (NSFontEffectsBox)

- (void)_addAttribute:(NSString *)attribute value:(id)value;
- (void)_removeAttribute:(NSString *)attribute;
- (void)_clearAttributes;

@end

@implementation NSBox (NSFontEffectsBox)

- (void)_addAttribute:(NSString *)attribute value:(id)value
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_attributesToAdd", &result);
    if (result) {
        [static_cast<NSMutableDictionary *>(result) setObject:value forKey:attribute];
        return;
    }

    RetainPtr dictionary = adoptNS([NSMutableDictionary new]);
    [dictionary setObject:value forKey:attribute];
    object_setInstanceVariableWithStrongDefault(self, "_attributesToAdd", dictionary.get());
}

- (void)_removeAttribute:(NSString *)attribute
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_attributesToRemove", &result);
    if (result) {
        [static_cast<NSMutableArray *>(result) addObject:attribute];
        return;
    }

    RetainPtr array = [NSMutableArray arrayWithObject:attribute];
    object_setInstanceVariableWithStrongDefault(self, "_attributesToRemove", array.get());
}

- (void)_clearAttributes
{
    object_setInstanceVariableWithStrongDefault(self, "_attributesToAdd", nil);
    object_setInstanceVariableWithStrongDefault(self, "_attributesToRemove", nil);
}

@end

@implementation NSFontPanel (TestWebKitAPI)

- (NSFontEffectsBox *)fontEffectsBox
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_fontEffectsBox", &result);
    return static_cast<NSFontEffectsBox *>(result);
}

- (NSColorWell *)foregroundColorToolbarColorWell
{
    return (NSColorWell *)[self _toolbarItemWithIdentifier:@"NSFontPanelTextColorToolbarItem"].view;
}

- (void)setUnderlineStyle:(NSUnderlineStyle)style
{
    [[self fontEffectsBox] _addAttribute:NSUnderlineStyleAttributeName value:@(style)];
}

- (void)setStrikethroughStyle:(NSUnderlineStyle)style
{
    [[self fontEffectsBox] _addAttribute:NSStrikethroughStyleAttributeName value:@(style)];
}

- (void)setTextShadow:(NSShadow *)shadow
{
    RetainPtr fontEffectsBox = [self fontEffectsBox];
    if (shadow)
        [fontEffectsBox _addAttribute:NSShadowAttributeName value:shadow];
    else
        [fontEffectsBox _removeAttribute:NSShadowAttributeName];
}

- (void)commitAttributeChanges
{
    [self _didChangeAttributes];
    [self.fontEffectsBox _clearAttributes];
}

- (void)_didChangeAttributes
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    if (self == [fontManager fontPanel:NO])
        [fontManager.target changeAttributes:self.fontEffectsBox];
}

- (id)_selectionAttributeValue:(NSString *)attribute
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_selection", &result);
    return [static_cast<NSDictionary *>(result) objectForKey:attribute];
}

- (NSShadow *)lastTextShadow
{
    return dynamic_objc_cast<NSShadow>([self _selectionAttributeValue:NSShadowAttributeName]);
}

- (NSToolbarItem *)_toolbarItemWithIdentifier:(NSString *)itemIdentifier
{
    for (NSToolbarItem *item in self.toolbar.items) {
        if ([item.itemIdentifier isEqualToString:itemIdentifier])
            return item;
    }
    return nil;
}

- (BOOL)hasUnderline
{
    RetainPtr underlineStyle = dynamic_objc_cast<NSNumber>([self _selectionAttributeValue:NSUnderlineStyleAttributeName]);
    return underlineStyle && ![underlineStyle isEqual:@(NSUnderlineStyleNone)];
}

- (BOOL)hasStrikeThrough
{
    RetainPtr underlineStyle = dynamic_objc_cast<NSNumber>([self _selectionAttributeValue:NSStrikethroughStyleAttributeName]);
    return underlineStyle && ![underlineStyle isEqual:@(NSUnderlineStyleNone)];
}

- (NSColor *)foregroundColor
{
    return self.foregroundColorToolbarColorWell.color;
}

@end

#endif // PLATFORM(MAC)
