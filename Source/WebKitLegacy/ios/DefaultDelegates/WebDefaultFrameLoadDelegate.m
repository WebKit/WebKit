/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebDefaultFrameLoadDelegate.h"

#import <WebKitLegacy/WebFrameLoadDelegatePrivate.h>
#import "WebViewPrivate.h"

@implementation WebDefaultFrameLoadDelegate

+ (WebDefaultFrameLoadDelegate *)sharedFrameLoadDelegate
{
    static WebDefaultFrameLoadDelegate *sharedDelegate = nil;
    if (!sharedDelegate)
        sharedDelegate = [[WebDefaultFrameLoadDelegate alloc] init];
    return sharedDelegate;
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didReceiveServerRedirectForProvisionalLoadForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didCancelClientRedirectForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender willCloseFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView didClearWindowObject:(WebScriptObject *)windowObject forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject
{
}

- (void)webView:(WebView *)webView didCreateJavaScriptContext:(JSContext *)context forFrame:(WebFrame *)frame
{
}

- (void)webViewDidDisplayInsecureContent:(WebView *)webView
{
}

- (void)webView:(WebView *)webView didRunInsecureContent:(WebSecurityOrigin *)origin
{
}

- (void)webView:(WebView *)webView didDetectXSS:(NSURL *)insecureURL
{
}

- (void)webView:(WebView *)webView didClearWindowObjectForFrame:(WebFrame *)frame inScriptWorld:(WebScriptWorld *)world
{
}

- (void)webView:(WebView *)webView didPushStateWithinPageForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView didReplaceStateWithinPageForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView didPopStateWithinPageForFrame:(WebFrame *)frame
{
}

#pragma mark -
#pragma mark SPI defined in a category in WebViewPrivate.h

- (void)webView:(WebView *)sender didFirstLayoutInFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didFinishDocumentLoadForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didHandleOnloadEventsForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didFirstVisuallyNonEmptyLayoutInFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView didClearInspectorWindowObject:(WebScriptObject *)windowObject forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView didRemoveFrameFromHierarchy:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didLayout:(WebLayoutMilestones)milestones
{
}

@end

#endif // PLATFORM(IOS)
