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

#import "WebScriptDebugServer.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebViewInternal.h"

#import <JavaScriptCore/Assertions.h>

NSString *WebScriptDebugServerProcessNameKey = @"WebScriptDebugServerProcessNameKey";
NSString *WebScriptDebugServerProcessBundleIdentifierKey = @"WebScriptDebugServerProcessBundleIdentifierKey";
NSString *WebScriptDebugServerProcessIdentifierKey = @"WebScriptDebugServerProcessIdentifierKey";

NSString *WebScriptDebugServerQueryNotification = @"WebScriptDebugServerQueryNotification";
NSString *WebScriptDebugServerQueryReplyNotification = @"WebScriptDebugServerQueryReplyNotification";

NSString *WebScriptDebugServerDidLoadNotification = @"WebScriptDebugServerDidLoadNotification";
NSString *WebScriptDebugServerWillUnloadNotification = @"WebScriptDebugServerWillUnloadNotification";

@implementation WebScriptDebugServer

static WebScriptDebugServer *sharedServer = nil;
static unsigned listenerCount = 0;

+ (WebScriptDebugServer *)sharedScriptDebugServer
{
    if (!sharedServer)
        sharedServer = [[WebScriptDebugServer alloc] init];
    return sharedServer;
}

+ (unsigned)listenerCount
{
    return listenerCount;
}

- (id)init
{
    self = [super init];

    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    serverName = [[NSString alloc] initWithFormat:@"WebScriptDebugServer-%@-%d", [processInfo processName], [processInfo processIdentifier]];

    serverConnection = [[NSConnection alloc] init];
    if ([serverConnection registerName:serverName]) {
        [serverConnection setRootObject:self];
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:[processInfo processName], WebScriptDebugServerProcessNameKey,
            [[NSBundle mainBundle] bundleIdentifier], WebScriptDebugServerProcessBundleIdentifierKey,
            [NSNumber numberWithInt:[processInfo processIdentifier]], WebScriptDebugServerProcessIdentifierKey, nil];
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName:WebScriptDebugServerDidLoadNotification object:serverName userInfo:info];
        [info release];
    } else {
        [serverConnection release];
        serverConnection = nil;
    }

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationTerminating:) name:NSApplicationWillTerminateNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverQuery:) name:WebScriptDebugServerQueryNotification object:nil];

    listeners = [[NSMutableSet alloc] init];

    return self;
}

- (void)dealloc
{
    ASSERT(listenerCount >= [listeners count]);
    listenerCount -= [listeners count];
    if (!listenerCount)
        [self detachScriptDebuggerFromAllWebViews];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillTerminateNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:WebScriptDebugServerQueryNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:WebScriptDebugServerWillUnloadNotification object:serverName];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSConnectionDidDieNotification object:nil];
    [serverConnection invalidate];
    [serverConnection release];
    [serverName release];
    [listeners release];
    [super dealloc];
}

- (void)applicationTerminating:(NSNotification *)notifiction
{
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:WebScriptDebugServerWillUnloadNotification object:serverName];
}

- (void)attachScriptDebuggerToAllWebViews
{
    [WebView _makeAllWebViewsPerformSelector:@selector(_attachScriptDebuggerToAllFrames)];
}

- (void)detachScriptDebuggerFromAllWebViews
{
    [WebView _makeAllWebViewsPerformSelector:@selector(_detachScriptDebuggerFromAllFrames)];
}

- (void)serverQuery:(NSNotification *)notification
{
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:[processInfo processName], WebScriptDebugServerProcessNameKey,
        [[NSBundle mainBundle] bundleIdentifier], WebScriptDebugServerProcessBundleIdentifierKey,
        [NSNumber numberWithInt:[processInfo processIdentifier]], WebScriptDebugServerProcessIdentifierKey, nil];
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:WebScriptDebugServerQueryReplyNotification object:serverName userInfo:info];
    [info release];
}

- (void)listenerConnectionDidDie:(NSNotification *)notification
{
    NSConnection *connection = [notification object];
    NSMutableSet *listenersToRemove = [[NSMutableSet alloc] initWithCapacity:[listeners count]];
    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject *listener = nil;

    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSConnectionDidDieNotification object:connection];

    while ((listener = [enumerator nextObject]))
        if ([[listener connectionForProxy] isEqualTo:connection])
            [listenersToRemove addObject:listener];

    ASSERT(listenerCount >= [listenersToRemove count]);
    listenerCount -= [listenersToRemove count];
    [listeners minusSet:listenersToRemove];
    [listenersToRemove release];

    if (!listenerCount)
        [self detachScriptDebuggerFromAllWebViews];
}

- (oneway void)addListener:(id<WebScriptDebugListener>)listener
{
    // can't use isKindOfClass: here because that will send over the wire and not check the proxy object
    if (!listener || [listener class] != [NSDistantObject class] || ![listener conformsToProtocol:@protocol(WebScriptDebugListener)])
        return;
    listenerCount++;
    [listeners addObject:listener];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(listenerConnectionDidDie:) name:NSConnectionDidDieNotification object:[(NSDistantObject *)listener connectionForProxy]];
    if (listenerCount == 1)
        [self attachScriptDebuggerToAllWebViews];
}

- (oneway void)removeListener:(id<WebScriptDebugListener>)listener
{
    if (!listener || ![listeners containsObject:listener])
        return;
    ASSERT(listenerCount >= 1);
    listenerCount--;
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSConnectionDidDieNotification object:[(NSDistantObject *)listener connectionForProxy]];
    [listeners removeObject:listener];
    if (!listenerCount)
        [self detachScriptDebuggerFromAllWebViews];
}

- (oneway void)step
{
    step = YES;
    paused = NO;
}

- (oneway void)pause
{
    paused = YES;
    step = NO;
}

- (oneway void)resume
{
    paused = NO;
    step = NO;
}

- (oneway BOOL)isPaused
{
    return paused;
}

- (void)suspendProcessIfPaused
{
    // this method will suspend this process when called during the dubugging callbacks
    // we need to do this to implement breakpoints and pausing of JavaScript

    while (paused)
        [[NSRunLoop currentRunLoop] runMode:NSConnectionReplyMode beforeDate:[NSDate distantFuture]];

    if (step) {
        step = NO;
        paused = YES;
    }
}

- (void)webView:(WebView *)webView didLoadMainResourceForDataSource:(WebDataSource *)dataSource
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView didLoadMainResourceForDataSource:dataSource];
    }

    inCallback = NO;
}

- (void)webView:(WebView *)webView       didParseSource:(NSString *)source
                                         baseLineNumber:(WebNSUInteger)lineNumber
                                                fromURL:(NSURL *)url
                                               sourceId:(int)sid
                                            forWebFrame:(WebFrame *)webFrame
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView didParseSource:source baseLineNumber:lineNumber fromURL:url sourceId:sid forWebFrame:webFrame];
    }

    inCallback = NO;
}

- (void)webView:(WebView *)webView  failedToParseSource:(NSString *)source
                                         baseLineNumber:(WebNSUInteger)lineNumber
                                                fromURL:(NSURL *)url
                                              withError:(NSError *)error
                                            forWebFrame:(WebFrame *)webFrame
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView failedToParseSource:source baseLineNumber:lineNumber fromURL:url withError:error forWebFrame:webFrame];
    }

    inCallback = NO;
}

- (void)webView:(WebView *)webView    didEnterCallFrame:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView didEnterCallFrame:frame sourceId:sid line:lineno forWebFrame:webFrame];
    }

    [self suspendProcessIfPaused];

    inCallback = NO;
}

- (void)webView:(WebView *)webView willExecuteStatement:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView willExecuteStatement:frame sourceId:sid line:lineno forWebFrame:webFrame];
    }

    [self suspendProcessIfPaused];

    inCallback = NO;
}

- (void)webView:(WebView *)webView   willLeaveCallFrame:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView willLeaveCallFrame:frame sourceId:sid line:lineno forWebFrame:webFrame];
    }

    [self suspendProcessIfPaused];

    inCallback = NO;
}

- (void)webView:(WebView *)webView   exceptionWasRaised:(WebScriptCallFrame *)frame
                                               sourceId:(int)sid
                                                   line:(int)lineno
                                            forWebFrame:(WebFrame *)webFrame
{
    if (![listeners count] || inCallback)
        return;

    inCallback = YES;

    NSEnumerator *enumerator = [listeners objectEnumerator];
    NSDistantObject <WebScriptDebugListener> *listener = nil;

    while ((listener = [enumerator nextObject])) {
        if ([[listener connectionForProxy] isValid])
            [listener webView:webView exceptionWasRaised:frame sourceId:sid line:lineno forWebFrame:webFrame];
    }

    [self suspendProcessIfPaused];

    inCallback = NO;
}

@end
