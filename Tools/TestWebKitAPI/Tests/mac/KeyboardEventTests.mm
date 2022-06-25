/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/RetainPtr.h>

@interface KeyboardTestMenu : NSMenu
@end

@implementation KeyboardTestMenu

- (BOOL)_containsItemMatchingEvent:(NSEvent *)event includingDisabledItems:(BOOL)includingDisabledItems
{
    return [event.charactersIgnoringModifiers isEqualToString:@"e"] && event.modifierFlags & NSEventModifierFlagFunction;
}

@end

@interface KeyboardTestMenuItem : NSMenuItem

@end

@implementation KeyboardTestMenuItem

- (BOOL)_isSystemMenuItem
{
    return YES;
}

@end

namespace TestWebKitAPI {

TEST(KeyboardEventTests, FunctionKeyCommand)
{
    auto menu = adoptNS([[KeyboardTestMenu alloc] initWithTitle:@"Test menu"]);
    auto menuItem = adoptNS([[KeyboardTestMenuItem alloc] initWithTitle:@"Emojis & Symbols" action:@selector(description) keyEquivalent:@"e"]);
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagFunction];
    [menu setItemArray:@[ menuItem.get() ]];
    NSApp.mainMenu = menu.get();

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<script>addEventListener('load', () => document.body.focus())</script><body contenteditable></body>"];
    [webView typeCharacter:'e' modifiers:NSEventModifierFlagFunction];
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
