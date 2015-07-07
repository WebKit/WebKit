/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DumpRenderTreeBrowserView.h"

#if PLATFORM(IOS)

#import <WebKit/WebView.h>

@interface UIWebBrowserView (WebUIKitDelegate)
- (BOOL)webView:(WebView *)webView shouldScrollToPoint:(CGPoint)point forFrame:(WebFrame *)frame;
@end

@implementation DumpRenderTreeBrowserView

@synthesize scrollingUsesUIWebScrollView = _scrollingUsesUIWebScrollView;

// We override [UIWebBrowserView addInputString] to avoid UIKit keyboard blocking
// sending input strings to webkit because we don't have a interaction element in
// DRT. Interaction element is only set by user tapping a element on the screen.
//
// see: <rdar://problem/8040227> DumpRenderTree: make addInputString work in iPhone DRT
// see: <rdar://problem/10499625> DumpRenderTree: DRT should always use the UIScrollView for scrolling
- (void)addInputString:(NSString *)str
{
    [[self webView] insertText:str];
}

// This is temporary solution to make window.scroll work in DumpRenderTree. The reason is
// UIWebDocumentView's shouldScrollToPoint always tells WebKit not to scroll.  This makes
// sense for MobileSafari/UIWebView app that there is top scroller and the scroller is
// move to the right spot.  But DRT doesn't have scroller so the page never scrolls.
//
// see: <rdar://problem/8153438> DumpRenderTree: DRT needs to match MobileSafari/UIWebView app more closely.
- (BOOL)webView:(WebView *)webView shouldScrollToPoint:(CGPoint)point forFrame:(WebFrame *)frame
{
    if (_scrollingUsesUIWebScrollView)
        return [super webView:webView shouldScrollToPoint:point forFrame:frame];
    if ([webView mainFrame] == frame)
        [self sendScrollEventIfNecessaryWasUserScroll:NO];

    return YES;
}

- (void)webView:(WebView *)sender addMessageToConsole:(NSDictionary *)dictionary withSource:(NSString *)source
{
    // Forward this to DRT UIDelegate since iOS WebKit by default sends this message to UIKitDelegate.
    id uiDelegate = [[self webView] UIDelegate];
    [uiDelegate webView:sender addMessageToConsole:dictionary withSource:source];
}

@end

#endif // PLATFORM(IOS)
