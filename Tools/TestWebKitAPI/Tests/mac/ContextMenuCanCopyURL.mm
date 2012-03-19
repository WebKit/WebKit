/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WTFStringUtilities.h"

#import <WebKit/WebViewPrivate.h>
#import <WebKit/DOM.h>
#import <Carbon/Carbon.h>
#import <wtf/RetainPtr.h>


@interface ContextMenuCanCopyURLDelegate : NSObject {
}
@end

static bool didFinishLoad;

@implementation ContextMenuCanCopyURLDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

static void contextMenuCopyLink(WebView* webView)
{
    [[[[webView mainFrame] frameView] documentView] layout];
    
    DOMDocument *document = [[webView mainFrame] DOMDocument];
    DOMElement *documentElement = [document documentElement];
    DOMHTMLAnchorElement *anchor = (DOMHTMLAnchorElement *)[documentElement querySelector:@"a"];

    NSWindow *window = [webView window];
    NSEvent *event = [NSEvent mouseEventWithType:NSRightMouseDown
                                        location:NSMakePoint(anchor.offsetLeft + anchor.offsetWidth / 2, window.frame.size.height - (anchor.offsetTop + anchor.offsetHeight / 2))
                                   modifierFlags:0
                                       timestamp:GetCurrentEventTime()
                                    windowNumber:[window windowNumber]
                                         context:[NSGraphicsContext currentContext] 
                                     eventNumber:0
                                      clickCount:0
                                        pressure:0.0];

    NSView *subView = [webView hitTest:[event locationInWindow]];
    if (!subView)
        return;

    NSMenu* menu = [subView menuForEvent:event];
    for (int i = 0; i < [menu numberOfItems]; ++i) {
        NSMenuItem* menuItem = [menu itemAtIndex:i];
        if ([menuItem tag] != WebMenuItemTagCopyLinkToClipboard)
            continue;
            
        [menu performActionForItemAtIndex:i];
    }
}


TEST(WebKit1, ContextMenuCanCopyURL)
{
    RetainPtr<WebView> webView(AdoptNS, [[WebView alloc] initWithFrame:NSMakeRect(0,0,800,600) frameName:nil groupName:nil]);
    RetainPtr<NSWindow> window(AdoptNS, [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 800, 600) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES]);
    RetainPtr<ContextMenuCanCopyURLDelegate> delegate(AdoptNS, [[ContextMenuCanCopyURLDelegate alloc] init]);

    [window.get().contentView addSubview:webView.get()];
    webView.get().frameLoadDelegate = delegate.get();

    [webView.get().mainFrame loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"ContextMenuCanCopyURL" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    
    Util::run(&didFinishLoad);

    contextMenuCopyLink(webView.get());
    
    NSURL *url = [NSURL URLFromPasteboard:[NSPasteboard generalPasteboard]];
    EXPECT_EQ(String("http://www.webkit.org/"), String([url absoluteString]));
}

} // namespace TestWebKitAPI
