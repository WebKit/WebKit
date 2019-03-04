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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

static bool contextMenuShown = false;

@interface ContextMenuImgWithVideoDelegate : NSObject <WKUIDelegate>
@end

@implementation ContextMenuImgWithVideoDelegate
- (NSMenu*)_webView:(WKWebView*)webView contextMenu:(NSMenu*)menu forElement:(_WKContextMenuElementInfo*)element
{
    return nil;
}

- (void)_webView:(WKWebView *)webView getContextMenuFromProposedMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element userInfo:(id <NSSecureCoding>)userInfo completionHandler:(void (^)(NSMenu *))completionHandler
{
    contextMenuShown = true;
    completionHandler(nil);
}

@end

namespace TestWebKitAPI {

TEST(WebKit, ContextMenuImgWithVideo)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    webView.get().UIDelegate = [[[ContextMenuImgWithVideoDelegate alloc] init] autorelease];
    [webView synchronouslyLoadTestPageNamed:@"ContextMenuImgWithVideo"];

    NSWindow* window = [webView window];
    NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
        location:NSMakePoint(100, window.frame.size.height - 100)
        modifierFlags:0
        timestamp:GetCurrentEventTime()
        windowNumber:[window windowNumber]
        context:[NSGraphicsContext currentContext]
        eventNumber:0
        clickCount:0
        pressure:0.0];

    NSView* subView = [webView hitTest:[event locationInWindow]];
    if (!subView)
        return;

    contextMenuShown = false;
    [subView mouseDown:event];
    Util::run(&contextMenuShown);
}

} // namespace TestWebKitAPI
