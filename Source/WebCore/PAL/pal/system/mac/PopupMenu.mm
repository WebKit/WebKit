/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "PopupMenu.h"

#if PLATFORM(MAC)

#import "NSMenuSPI.h"
#import <wtf/RetainPtr.h>

@interface NSMenu (WebPrivate)
- (id)_menuImpl;
@end

@interface NSObject (WebPrivate)
- (void)popUpMenu:(NSMenu*)menu atLocation:(NSPoint)location width:(CGFloat)width forView:(NSView*)view withSelectedItem:(NSInteger)selectedItem withFont:(NSFont*)font withFlags:(NSPopUpMenuFlags)flags withOptions:(NSDictionary *)options;
@end

namespace PAL {

void popUpMenu(NSMenu *menu, NSPoint location, float width, NSView *view, int selectedItem, NSFont *font, NSControlSize controlSize, bool usesCustomAppearance)
{
    NSRect adjustedPopupBounds = [view.window convertRectToScreen:[view convertRect:view.bounds toView:nil]];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (controlSize != NSMiniControlSize) {
        ALLOW_DEPRECATED_DECLARATIONS_END
        adjustedPopupBounds.origin.x -= 3;
        adjustedPopupBounds.origin.y -= 1;
        adjustedPopupBounds.size.width += 6;
    }

    // These numbers were extracted from visual inspection as the menu animates shut.
    NSSize labelOffset = NSMakeSize(11, 1);
    if (menu.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft)
        labelOffset = NSMakeSize(24, 1);

    auto options = adoptNS([@{
        NSPopUpMenuPopupButtonBounds : [NSValue valueWithRect:adjustedPopupBounds],
        NSPopUpMenuPopupButtonLabelOffset : [NSValue valueWithSize:labelOffset],
        NSPopUpMenuPopupButtonSize : @(controlSize)
    } mutableCopy]);

    if (usesCustomAppearance)
        [options setObject:@"" forKey:NSPopUpMenuPopupButtonWidget];

    [[menu _menuImpl] popUpMenu:menu atLocation:location width:width forView:view withSelectedItem:selectedItem withFont:font withFlags:(usesCustomAppearance ? 0 : NSPopUpMenuIsPopupButton) withOptions:options.get()];
}

} // namespace PAL

#endif // PLATFORM(MAC)
