/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebInspector.h"
#import "WebFrameInternal.h"
#import "WebView.h"

#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>

using namespace WebCore;

@implementation WebInspector
- (id)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;
    _webView = webView; // not retained to prevent a cycle
    return self;
}

- (void)webViewClosed
{
    _webView = nil;
}

- (void)show:(id)sender
{
    if (Page* page = core(_webView))
        page->inspectorController()->show();
}

- (void)showConsole:(id)sender
{
    if (Page* page = core(_webView))
        page->inspectorController()->showConsole();
}

- (void)showTimeline:(id)sender
{
    if (Page* page = core(_webView))
        page->inspectorController()->showTimeline();
}

- (void)close:(id)sender 
{
    if (Page* page = core(_webView))
        page->inspectorController()->close();
}

- (void)attach:(id)sender
{
    if (Page* page = core(_webView))
        page->inspectorController()->attachWindow();
}

- (void)detach:(id)sender
{
    if (Page* page = core(_webView))
        page->inspectorController()->detachWindow();
}
@end

@implementation WebInspector (Obsolete)
+ (WebInspector *)sharedWebInspector
{
    // Safari 3 beta calls this method
    static BOOL logged = NO;
    if (!logged) {
        NSLog(@"+[WebInspector sharedWebInspector]: this method is obsolete.");
        logged = YES;
    }

    return [[[WebInspector alloc] init] autorelease];
}

+ (WebInspector *)webInspector
{
    // Safari 3 beta calls this method
    static BOOL logged = NO;
    if (!logged) {
        NSLog(@"+[WebInspector webInspector]: this method is obsolete.");
        logged = YES;
    }

    return [[[WebInspector alloc] init] autorelease];
}

- (void)setWebFrame:(WebFrame *)frame
{
    // Safari 3 beta calls this method
    static BOOL logged = NO;
    if (!logged) {
        NSLog(@"-[WebInspector setWebFrame:]: this method is obsolete.");
        logged = YES;
    }

    _webView = [frame webView];
}

- (NSWindow *)window
{
    // Shiira calls this internal method, return nil since we can't easily return the window
    static BOOL logged = NO;
    if (!logged) {
        NSLog(@"-[WebInspector window]: this method is obsolete and now returns nil.");
        logged = YES;
    }

    return nil;
}

- (void)showWindow:(id)sender
{
    // Safari 3 beta calls this method
    static BOOL logged = NO;
    if (!logged) {
        NSLog(@"-[WebInspector showWindow:]: this method is obsolete.");
        logged = YES;
    }

    [self show:sender];
}
@end
