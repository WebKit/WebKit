/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebPageBridge.h"

#import "WebChromeClient.h"
#import "WebDefaultUIDelegate.h"
#import "WebFrameBridge.h"
#import "WebFrameView.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebUIDelegate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/Assertions.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceLoader.h>
#import <WebCore/WebCoreFrameNamespaces.h>
#import <wtf/Forward.h>

using namespace WebCore;

@implementation WebPageBridge

- (id)initWithMainFrameName:(NSString *)frameName webView:(WebView *)webView frameView:(WebFrameView *)frameView
{
    self = [super initWithChromeClient:WebChromeClient::create(webView)];
    if (self) {
        _webView = webView;
        WebFrameBridge *mainFrame = [[WebFrameBridge alloc] initMainFrameWithPage:self frameName:frameName view:frameView];
        [self setMainFrame:mainFrame];
        [mainFrame release];
    }
    return self;
}

- (WebView *)webView
{
    return _webView;
}

- (NSView *)outerView
{
    return [[_webView mainFrame] frameView];
}

- (void)setWindowFrame:(NSRect)frameRect
{
    ASSERT(_webView != nil);
    [[_webView _UIDelegateForwarder] webView:_webView setFrame:frameRect];
}

- (NSRect)windowFrame
{
    ASSERT(_webView != nil);
    return [[_webView _UIDelegateForwarder] webViewFrame:_webView];
}

- (WebCorePageBridge *)createModalDialogWithURL:(NSURL *)URL referrer:(NSString *)referrer
{
    ASSERT(_webView != nil);

    NSMutableURLRequest *request = nil;

    if (URL != nil && ![URL _web_isEmpty]) {
        request = [NSMutableURLRequest requestWithURL:URL];
        [request _web_setHTTPReferrer:referrer];
    }

    id UIDelegate = [_webView UIDelegate];

    WebView *newWebView = nil;
    if ([UIDelegate respondsToSelector:@selector(webView:createWebViewModalDialogWithRequest:)])
        newWebView = [UIDelegate webView:_webView createWebViewModalDialogWithRequest:request];
    else if ([UIDelegate respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWebView = [UIDelegate webView:_webView createWebViewWithRequest:request];
    else
        newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:_webView createWebViewWithRequest:request];

    return [newWebView _pageBridge];
}

@end
