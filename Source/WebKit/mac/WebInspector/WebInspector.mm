/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#import "WebInspectorPrivate.h"
#import "WebInspectorFrontend.h"

#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>

using namespace WebCore;

NSString *WebInspectorDidStartSearchingForNode = @"WebInspectorDidStartSearchingForNode";
NSString *WebInspectorDidStopSearchingForNode = @"WebInspectorDidStopSearchingForNode";

@implementation WebInspector
- (id)initWithInspectedWebView:(WebView *)inspectedWebView
{
    if (!(self = [super init]))
        return nil;
    _inspectedWebView = inspectedWebView; // not retained to prevent a cycle

    return self;
}

- (void)dealloc
{
    [_frontend release];
    [super dealloc];
}

- (void)inspectedWebViewClosed
{
    [self close:nil];
    _inspectedWebView = nil;
}

- (void)showWindow
{
    if (Page* inspectedPage = core(_inspectedWebView))
        inspectedPage->inspectorController().show();
}

- (void)show:(id)sender
{
    [self showWindow];
}

- (void)showConsole:(id)sender
{
    [self showWindow];
    [_frontend showConsole];
}

- (BOOL)isDebuggingJavaScript
{
    return _frontend && [_frontend isDebuggingEnabled];
}

- (void)toggleDebuggingJavaScript:(id)sender
{
    [self showWindow];

    if ([self isDebuggingJavaScript])
        [_frontend setDebuggingEnabled:false];
    else
        [_frontend setDebuggingEnabled:true];
}

- (void)startDebuggingJavaScript:(id)sender
{
    if (_frontend)
        [_frontend setDebuggingEnabled:true];
}

- (void)stopDebuggingJavaScript:(id)sender
{
    if (_frontend)
        [_frontend setDebuggingEnabled:false];
}

- (BOOL)isProfilingJavaScript
{
    // No longer supported.
    return NO;
}

- (void)toggleProfilingJavaScript:(id)sender
{
    // No longer supported.
}

- (void)startProfilingJavaScript:(id)sender
{
    // No longer supported.
}

- (void)stopProfilingJavaScript:(id)sender
{
    // No longer supported.
}

- (BOOL)isJavaScriptProfilingEnabled
{
    // No longer supported.
    return NO;
}

- (void)setJavaScriptProfilingEnabled:(BOOL)enabled
{
    // No longer supported.
}

- (BOOL)isTimelineProfilingEnabled
{
    return _frontend && [_frontend isTimelineProfilingEnabled];
}

- (void)setTimelineProfilingEnabled:(BOOL)enabled
{
    if (_frontend)
        [_frontend setTimelineProfilingEnabled:enabled];
}

- (BOOL)isOpen
{
    return !!_frontend;
}

- (void)close:(id)sender 
{
    [_frontend close];
}

- (void)attach:(id)sender
{
    [_frontend attach];
}

- (void)detach:(id)sender
{
    [_frontend detach];
}

- (void)evaluateInFrontend:(id)sender script:(NSString *)script
{
    if (Page* inspectedPage = core(_inspectedWebView))
        inspectedPage->inspectorController().evaluateForTestInFrontend(script);
}

- (void)setFrontend:(WebInspectorFrontend *)frontend
{
    _frontend = [frontend retain];
}

- (void)releaseFrontend
{
    [_frontend release];
    _frontend = nil;
}
@end
