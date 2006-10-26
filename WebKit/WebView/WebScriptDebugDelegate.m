/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "WebScriptDebugDelegatePrivate.h"

#import "WebDataSource.h"
#import "WebDataSourceInternal.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebViewInternal.h"
#import <WebCore/FrameMac.h>
#import <WebCore/WebCoreScriptDebugger.h>

using namespace WebCore;

// FIXME: these error strings should be public for future use by WebScriptObject and in WebScriptObject.h
NSString * const WebScriptErrorDomain = @"WebScriptErrorDomain";
NSString * const WebScriptErrorDescriptionKey = @"WebScriptErrorDescription";
NSString * const WebScriptErrorLineNumberKey = @"WebScriptErrorLineNumber";

@interface WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (WebScriptCallFrame *)_initWithFrame:(WebCoreScriptCallFrame *)frame;

@end

@implementation WebScriptDebugger

- (WebScriptDebugger *)initWithWebFrame:(WebFrame *)webFrame
{
    if ((self = [super init])) {
        _webFrame = webFrame;
        _debugger = [[WebCoreScriptDebugger alloc] initWithDelegate:self];
    }
    return self;
}

- (void)dealloc
{
    [_debugger release];
    [super dealloc];
}

- (WebScriptObject *)globalObject
{
    return core(_webFrame)->windowScriptObject();
}

- (id)newWrapperForFrame:(WebCoreScriptCallFrame *)frame
{
    return [[WebScriptCallFrame alloc] _initWithFrame:frame];
}

- (void)parsedSource:(NSString *)source fromURL:(NSURL *)url sourceId:(int)sid startLine:(int)startLine errorLine:(int)errorLine errorMessage:(NSString *)errorMessage
{
    WebView *webView = [_webFrame webView];
    if (errorLine == -1) {
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:source baseLineNumber:startLine fromURL:url sourceId:sid forWebFrame:_webFrame];
        [[webView _scriptDebugDelegateForwarder] webView:webView didParseSource:source fromURL:[url absoluteString] sourceId:sid forWebFrame:_webFrame]; // deprecated delegate method
        if ([WebScriptDebugServer listenerCount])
            [[WebScriptDebugServer sharedScriptDebugServer] webView:webView didParseSource:source baseLineNumber:startLine fromURL:url sourceId:sid forWebFrame:_webFrame];
    } else {
        NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:errorMessage, WebScriptErrorDescriptionKey, [NSNumber numberWithUnsignedInt:errorLine], WebScriptErrorLineNumberKey, nil];
        NSError *error = [[NSError alloc] initWithDomain:WebScriptErrorDomain code:WebScriptGeneralErrorCode userInfo:info];
        [[webView _scriptDebugDelegateForwarder] webView:webView failedToParseSource:source baseLineNumber:startLine fromURL:url withError:error forWebFrame:_webFrame];
        if ([WebScriptDebugServer listenerCount])
            [[WebScriptDebugServer sharedScriptDebugServer] webView:webView failedToParseSource:source baseLineNumber:startLine fromURL:url withError:error forWebFrame:_webFrame];
        [error release];
        [info release];
    }
}

- (void)enteredFrame:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView didEnterCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView didEnterCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

- (void)hitStatement:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView willExecuteStatement:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView willExecuteStatement:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

- (void)leavingFrame:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView willLeaveCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView willLeaveCallFrame:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

- (void)exceptionRaised:(WebCoreScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno
{
    WebView *webView = [_webFrame webView];
    [[webView _scriptDebugDelegateForwarder] webView:webView exceptionWasRaised:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:webView exceptionWasRaised:[frame wrapper] sourceId:sid line:lineno forWebFrame:_webFrame];
}

@end



@implementation WebScriptCallFrame (WebScriptDebugDelegateInternal)

- (WebScriptCallFrame *)_initWithFrame:(WebCoreScriptCallFrame *)frame
{
    if ((self = [super init])) {
        _private = frame;
    }
    return self;
}

@end



@implementation WebScriptCallFrame

- (void) dealloc
{
    [_userInfo release];
    [super dealloc];
}

- (void)setUserInfo:(id)userInfo
{
    if (userInfo != _userInfo) {
        [_userInfo release];
        _userInfo = [userInfo retain];
    }
}

- (id)userInfo
{
    return _userInfo;
}

- (WebScriptCallFrame *)caller
{
    return [[_private caller] wrapper];
}

- (NSArray *)scopeChain
{
    return [_private scopeChain];
}

- (NSString *)functionName
{
    return [_private functionName];
}

- (id)exception
{
    return [_private exception];
}

- (id)evaluateWebScript:(NSString *)script
{
    return [_private evaluateWebScript:script];
}

@end
