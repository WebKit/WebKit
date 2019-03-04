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

#import "config.h"
#import "_WKInspector.h"

#import "WKWebViewInternal.h"
#import "WebProcessProxy.h"
#import "_WKFrameHandleInternal.h"
#import "_WKInspectorInternal.h"
#import <wtf/RetainPtr.h>

@implementation _WKInspector

- (WKWebView *)webView
{
    if (auto* page = _inspector->inspectedPage())
        return fromWebPageProxy(*page);
    return nil;
}

- (BOOL)isConnected
{
    return _inspector->isConnected();
}

- (BOOL)isVisible
{
    return _inspector->isVisible();
}

- (BOOL)isFront
{
    return _inspector->isFront();
}

- (BOOL)isProfilingPage
{
    return _inspector->isProfilingPage();
}

- (BOOL)isElementSelectionActive
{
    return _inspector->isElementSelectionActive();
}

- (void)connect
{
    _inspector->connect();
}

- (void)show
{
    _inspector->show();
}

- (void)hide
{
    _inspector->hide();
}

- (void)close
{
    _inspector->close();
}

- (void)showConsole
{
    _inspector->showConsole();
}

- (void)showResources
{
    _inspector->showResources();
}

- (void)showMainResourceForFrame:(_WKFrameHandle *)frame
{
    if (auto* page = _inspector->inspectedPage())
        _inspector->showMainResourceForFrame(page->process().webFrame(frame._frameID));
}

- (void)attach
{
    _inspector->attach();
}

- (void)detach
{
    _inspector->detach();
}

- (void)showTimelines
{
    _inspector->showTimelines();
}

- (void)togglePageProfiling
{
    _inspector->togglePageProfiling();
}

- (void)toggleElementSelection
{
    _inspector->toggleElementSelection();
}

- (void)printErrorToConsole:(NSString *)error
{
    // FIXME: This should use a new message source rdar://problem/34658378
    [self.webView evaluateJavaScript:[NSString stringWithFormat:@"console.error(\"%@\");", error] completionHandler:nil];
}

- (API::Object&)_apiObject
{
    return *_inspector;
}

@end
