/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <Carbon/Carbon.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

@interface ContextMenuDefaultItemsHaveTagsDelegate : NSObject <WebFrameLoadDelegate>
@end

static bool didFinishLoad;

@implementation ContextMenuDefaultItemsHaveTagsDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, ContextMenuDefaultItemsHaveTags)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) frameName:nil groupName:nil]);
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 800, 600) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    RetainPtr<ContextMenuDefaultItemsHaveTagsDelegate> delegate = adoptNS([[ContextMenuDefaultItemsHaveTagsDelegate alloc] init]);

    [window.get().contentView addSubview:webView.get()];
    webView.get().frameLoadDelegate = delegate.get();

    [webView.get().mainFrame loadHTMLString:@"<body contenteditable>" baseURL:nil];

    Util::run(&didFinishLoad);

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown location:NSMakePoint(400, 300) modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[window windowNumber] context:[NSGraphicsContext currentContext] eventNumber:0 clickCount:0 pressure:0];
    NSView *subView = [webView hitTest:[event locationInWindow]];
    NSMenu *menu = [subView menuForEvent:event];

    for (NSMenuItem *item in menu.itemArray) {
        if (!item.isSeparatorItem)
            EXPECT_NE(0, item.tag);
    }
}

} // namespace TestWebKitAPI
