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

#include "config.h"
#include "NSFontPanelTesting.h"

#if PLATFORM(MAC)

#import <objc/runtime.h>

@interface NSBox (NSFontEffectsBox)
// Invoked after a font effect (e.g. single strike-through) is chosen.
- (void)_openEffectsButton:(id)sender;
@end

static NSView *findSubviewOfClass(NSView *view, Class targetClass)
{
    if ([view isKindOfClass:targetClass])
        return view;

    for (NSView *subview in view.subviews) {
        if (NSView *targetView = findSubviewOfClass(subview, targetClass))
            return targetView;
    }
    return nil;
}

static NSMenuItem *findMenuItemWithTitle(NSPopUpButton *button, NSString *title)
{
    for (NSMenuItem *item in button.itemArray) {
        if ([item.title.lowercaseString isEqualToString:title.lowercaseString])
            return item;
    }
    return nil;
}

@implementation NSFontPanel (TestWebKitAPI)

- (NSBox *)fontEffectsBox
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_fontEffectsBox", &result);
    return static_cast<NSBox *>(result);
}

- (void)chooseUnderlineMenuItemWithTitle:(NSString *)itemTitle
{
    NSPopUpButton *button = (NSPopUpButton *)[self _toolbarItemWithIdentifier:@"NSFontPanelUnderlineToolbarItem"].view;
    [button selectItem:findMenuItemWithTitle(button, itemTitle)];
    [self.fontEffectsBox _openEffectsButton:button];
    [self _didChangeAttributes];
}

- (void)chooseStrikeThroughMenuItemWithTitle:(NSString *)itemTitle
{
    NSPopUpButton *button = (NSPopUpButton *)[self _toolbarItemWithIdentifier:@"NSFontPanelStrikethroughToolbarItem"].view;
    [button selectItem:findMenuItemWithTitle(button, itemTitle)];
    [self.fontEffectsBox _openEffectsButton:button];
    [self _didChangeAttributes];
}

- (void)_didChangeAttributes
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    if (self == [fontManager fontPanel:NO])
        [fontManager.target changeAttributes:self.fontEffectsBox];
}

- (NSSlider *)shadowBlurSlider
{
    return (NSSlider *)findSubviewOfClass([self _toolbarItemWithIdentifier:@"NSFontPanelShadowBlurToolbarItem"].view, NSSlider.class);
}

- (NSSlider *)shadowOpacitySlider
{
    return (NSSlider *)findSubviewOfClass([self _toolbarItemWithIdentifier:@"NSFontPanelShadowOpacityToolbarItem"].view, NSSlider.class);
}

- (NSButton *)shadowToggleButton
{
    return (NSButton *)[self _toolbarItemWithIdentifier:@"NSFontPanelShadowToggleToolbarItem"].view;
}

- (void)toggleShadow
{
    NSButton *shadowToggleButton = self.shadowToggleButton;
    shadowToggleButton.state = shadowToggleButton.state == NSControlStateValueOff ? NSControlStateValueOn : NSControlStateValueOff;
    [self _didChangeAttributes];
}

- (double)shadowOpacity
{
    return self.shadowOpacitySlider.doubleValue;
}

- (void)setShadowOpacity:(double)shadowOpacity
{
    self.shadowOpacitySlider.doubleValue = shadowOpacity;
    if (self.shadowToggleButton.state == NSControlStateValueOn)
        [self _didChangeAttributes];
}

- (double)shadowBlur
{
    return self.shadowBlurSlider.doubleValue;
}

- (void)setShadowBlur:(double)shadowBlur
{
    self.shadowBlurSlider.doubleValue = shadowBlur;
    if (self.shadowToggleButton.state == NSControlStateValueOn)
        [self _didChangeAttributes];
}

- (NSToolbarItem *)_toolbarItemWithIdentifier:(NSString *)itemIdentifier
{
    for (NSToolbarItem *item in self.toolbar.items) {
        if ([item.itemIdentifier isEqualToString:itemIdentifier])
            return item;
    }
    return nil;
}

@end

#endif // PLATFORM(MAC)
