/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006, 2007 Vladimir Olexa (vladimir.olexa@gmail.com)
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

#import "config.h"
#import "ServerConnection.h"

#import "DebuggerDocument.h"

#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/RetainPtr.h>

@implementation ServerConnection

#pragma mark -
- (id)initWithServerName:(NSString *)serverName;
{
    if (!(self = [super init]))
        return nil;

    [self switchToServerNamed:serverName];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationTerminating:) name:NSApplicationWillTerminateNotification object:nil];

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillTerminateNotification object:nil];

    [currentServerName release];
    [currentFrame release];
    [server release];
    JSGlobalContextRelease(globalContext);
    [super dealloc];
}

- (void)setGlobalContext:(JSGlobalContextRef)globalContextRef
{
    globalContext = JSGlobalContextRetain(globalContextRef);
}

#pragma mark -
#pragma mark Pause & Step

- (void)pause
{
    if ([[(NSDistantObject *)server connectionForProxy] isValid])
        [server pause];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

- (void)resume
{
    if ([[(NSDistantObject *)server connectionForProxy] isValid])
        [server resume];
}

- (void)stepInto
{
    if ([[(NSDistantObject *)server connectionForProxy] isValid])
        [server step];
}

#pragma mark -
#pragma mark Connection Handling

- (void)switchToServerNamed:(NSString *)name
{
    if (server) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSConnectionDidDieNotification object:[(NSDistantObject *)server connectionForProxy]];
        if ([[(NSDistantObject *)server connectionForProxy] isValid]) {
            [server removeListener:self];
            [self resume];
        }
    }

    id<WebScriptDebugServer> oldServer = server;
    server = [name length] ? [[NSConnection rootProxyForConnectionWithRegisteredName:name host:nil] retain] : nil;
    [oldServer release];

    NSString *oldServerName = currentServerName;
    currentServerName = [name retain];
    [oldServerName release];

    if (server) {
        @try {
            [(NSDistantObject *)server setProtocolForProxy:@protocol(WebScriptDebugServer)];
            [server addListener:self];
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(serverConnectionDidDie:) name:NSConnectionDidDieNotification object:[(NSDistantObject *)server connectionForProxy]];  
        } @catch (NSException *exception) {
            [currentServerName release];
            currentServerName = nil;
            [server release];
            server = nil;
        }
    }
}

- (void)applicationTerminating:(NSNotification *)notifiction
{
    if (server && [[(NSDistantObject *)server connectionForProxy] isValid]) {
        [self switchToServerNamed:nil];
        // call the runloop for a while to make sure our removeListener: is sent to the server
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.25]];
    }
}

- (void)serverConnectionDidDie:(NSNotification *)notifiction
{
    [self switchToServerNamed:nil];
}

#pragma mark -
#pragma mark Debug Listener Callbacks

- (void)webView:(WebView *)view didLoadMainResourceForDataSource:(WebDataSource *)dataSource
{
    // Get document source
    NSString *documentSource = nil;
    id <WebDocumentRepresentation> rep = [dataSource representation];
    if ([rep canProvideDocumentSource])
        documentSource = [rep documentSource];

    if (!documentSource)
        return;

    JSRetainPtr<JSStringRef> documentSourceJS(Adopt, JSStringCreateWithCFString((CFStringRef)documentSource));

    // Get URL
    NSString *url = [[[dataSource response] URL] absoluteString];
    JSRetainPtr<JSStringRef> urlJS(Adopt, JSStringCreateWithCFString(url ? (CFStringRef)url : CFSTR("")));

    DebuggerDocument::updateFileSource(globalContext, documentSourceJS.get(), urlJS.get());
}

- (void)webView:(WebView *)view didParseSource:(NSString *)source baseLineNumber:(unsigned)baseLine fromURL:(NSURL *)url sourceId:(int)sid forWebFrame:(WebFrame *)webFrame
{
    if (!globalContext)
        return;

    RetainPtr<NSString> sourceCopy = source;
    if (!sourceCopy.get())
        return;

    RetainPtr<NSString> documentSourceCopy;
    RetainPtr<NSString> urlCopy = [url absoluteString];

    WebDataSource *dataSource = [webFrame dataSource];
    if (!url || [[[dataSource response] URL] isEqual:url]) {
        id <WebDocumentRepresentation> rep = [dataSource representation];
        if ([rep canProvideDocumentSource])
            documentSourceCopy = [rep documentSource];
        if (!urlCopy.get())
            urlCopy = [[[dataSource response] URL] absoluteString];
    }

    JSRetainPtr<JSStringRef> sourceCopyJS(Adopt, JSStringCreateWithCFString((CFStringRef)sourceCopy.get()));  // We checked for NULL earlier.
    JSRetainPtr<JSStringRef> documentSourceCopyJS(Adopt, JSStringCreateWithCFString(documentSourceCopy.get() ? (CFStringRef)documentSourceCopy.get() : CFSTR("")));
    JSRetainPtr<JSStringRef> urlCopyJS(Adopt, JSStringCreateWithCFString(urlCopy.get() ? (CFStringRef)urlCopy.get() : CFSTR("")));
    JSValueRef sidJS = JSValueMakeNumber(globalContext, sid);
    JSValueRef baseLineJS = JSValueMakeNumber(globalContext, baseLine);

    DebuggerDocument::didParseScript(globalContext, sourceCopyJS.get(), documentSourceCopyJS.get(), urlCopyJS.get(), sidJS, baseLineJS);
}

- (void)webView:(WebView *)view failedToParseSource:(NSString *)source baseLineNumber:(unsigned)baseLine fromURL:(NSURL *)url withError:(NSError *)error forWebFrame:(WebFrame *)webFrame
{
}

- (void)webView:(WebView *)view didEnterCallFrame:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!globalContext)
        return;

    id old = currentFrame;
    currentFrame = [frame retain];
    [old release];

    JSValueRef sidJS = JSValueMakeNumber(globalContext, sid);
    JSValueRef linenoJS = JSValueMakeNumber(globalContext, lineno);

    DebuggerDocument::didEnterCallFrame(globalContext, sidJS, linenoJS);
}

- (void)webView:(WebView *)view willExecuteStatement:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!globalContext)
        return;

    JSValueRef sidJS = JSValueMakeNumber(globalContext, sid);
    JSValueRef linenoJS = JSValueMakeNumber(globalContext, lineno);

    DebuggerDocument::willExecuteStatement(globalContext, sidJS, linenoJS);
}

- (void)webView:(WebView *)view willLeaveCallFrame:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!globalContext)
        return;

    JSValueRef sidJS = JSValueMakeNumber(globalContext, sid);
    JSValueRef linenoJS = JSValueMakeNumber(globalContext, lineno);

    DebuggerDocument::willLeaveCallFrame(globalContext, sidJS, linenoJS);

    id old = currentFrame;
    currentFrame = [[frame caller] retain];
    [old release];
}

- (void)webView:(WebView *)view exceptionWasRaised:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!globalContext)
        return;

    JSValueRef sidJS = JSValueMakeNumber(globalContext, sid);
    JSValueRef linenoJS = JSValueMakeNumber(globalContext, lineno);

    DebuggerDocument::exceptionWasRaised(globalContext, sidJS, linenoJS);
}

#pragma mark -
#pragma mark Stack & Variables

- (WebScriptCallFrame *)currentFrame
{
    return currentFrame;
}

#pragma mark -
#pragma mark Server Detection Callbacks

-(NSString *)currentServerName
{
    return currentServerName;
}
@end
