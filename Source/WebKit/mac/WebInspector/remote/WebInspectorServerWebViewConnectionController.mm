/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#if ENABLE(REMOTE_INSPECTOR)

#import "WebInspectorServerWebViewConnectionController.h"

#import "WebInspectorClient.h"
#import "WebInspectorClientRegistry.h"
#import "WebInspectorRelayDefinitions.h"
#import "WebInspectorServer.h"
#import "WebInspectorServerWebViewConnection.h"
#import "WebInspectorXPCWrapper.h"
#import "WebView.h"
#import "WebViewPrivate.h"

#if PLATFORM(IOS)
#import <WebCore/WebCoreThread.h>
#import <WebCore/WebCoreThreadRun.h>
#endif


@implementation WebInspectorServerWebViewConnectionController

- (id)initWithServer:(WebInspectorServer *)server
{
    self = [super init];
    if (!self)
        return nil;

    _server = server;

    return self;
}

- (void)dealloc
{
    ASSERT(![_openConnections count]);
    [_openConnections release];

    [super dealloc];
}

- (void)closeAllConnections
{
#if PLATFORM(IOS)
    WebThreadRun(^{
#endif

    for (NSNumber *key in _openConnections) {
        WebInspectorServerWebViewConnection *connection = [_openConnections objectForKey:key];
        [connection receivedDidClose:nil];
    }
    [_openConnections removeAllObjects];

    dispatch_async(dispatch_get_main_queue(), ^{
        [_server setHasActiveDebugSession:NO];
    });

#if PLATFORM(IOS)
    });
#endif
}

- (NSDictionary *)_listingForWebView:(WebView *)webView pageId:(NSNumber *)pageId registry:(WebInspectorClientRegistry *)registry
{
    NSMutableDictionary *webViewDetails = [NSMutableDictionary dictionary];

    // Page ID.
    [webViewDetails setObject:pageId forKey:WIRPageIdentifierKey];

    // URL.
    NSString *url = [webView mainFrameURL];
    if (!url)
        url = @"about:blank";
    [webViewDetails setObject:url forKey:WIRURLKey];

    // Title.
    NSString *title = [webView mainFrameTitle];
    [webViewDetails setObject:title forKey:WIRTitleKey];

    // [Optional] Connection Identifier (Remote Debugger UUID).
    // [Optional] Has Local Debugger Session.
    WebInspectorServerWebViewConnection *connection = [_openConnections objectForKey:pageId];
    if (connection)
        [webViewDetails setObject:connection.connectionIdentifier forKey:WIRConnectionIdentifierKey];
    else {
        WebInspectorClient* inspectorClient = [registry clientForPageId:[pageId unsignedIntValue]];
        if (inspectorClient->hasLocalSession())
            [webViewDetails setObject:[NSNumber numberWithBool:YES] forKey:WIRHasLocalDebuggerKey];
    }

#if PLATFORM(IOS)
    // [Optional] Host Application ID.
    NSString *hostApplicationId = [webView hostApplicationBundleId];
    if (hostApplicationId)
        [webViewDetails setObject:hostApplicationId forKey:WIRHostApplicationIdentifierKey];

    // [Optional] Host Application Name.
    NSString *hostApplicationName = [webView hostApplicationName];
    if (hostApplicationName)
        [webViewDetails setObject:hostApplicationName forKey:WIRHostApplicationNameKey];
#endif

    // [Optional] Remote Inspector WebView User Info Dictionary.
    NSDictionary *remoteInspectorUserInfo = [webView remoteInspectorUserInfo];
    if (remoteInspectorUserInfo)
        [webViewDetails setObject:remoteInspectorUserInfo forKey:WIRUserInfoKey];

    return webViewDetails;
}

- (void)_pushListing:(NSString *)optionalConnectionIdentifier
{
#if PLATFORM(IOS)
    ASSERT(WebThreadIsCurrent());
#endif

    _hasScheduledPush = NO;

    NSMutableDictionary *response = [[NSMutableDictionary alloc] init];
    WebInspectorClientRegistry *webInspectorClientRegistry = [WebInspectorClientRegistry sharedRegistry];
    NSDictionary *registeredViews = [webInspectorClientRegistry inspectableWebViews];
    for (NSNumber *pageId in registeredViews) {
        WebView *webView = [registeredViews objectForKey:pageId];
        NSDictionary *webViewDetails = [self _listingForWebView:webView pageId:pageId registry:webInspectorClientRegistry];
        [response setObject:webViewDetails forKey:[pageId stringValue]];
    }

    NSMutableDictionary *outgoing = [[NSMutableDictionary alloc] init];
    [outgoing setObject:response forKey:WIRListingKey];
    if (optionalConnectionIdentifier)
        [outgoing setObject:optionalConnectionIdentifier forKey:WIRConnectionIdentifierKey];

    [self sendMessageToFrontend:WIRListingMessage userInfo:outgoing];

    [outgoing release];
    [response release];
}

- (void)pushListing:(NSString *)optionalConnectionIdentifier
{
#if PLATFORM(IOS)
    ASSERT(WebThreadIsCurrent());
#endif

    if (_hasScheduledPush)
        return;

    if (optionalConnectionIdentifier) {
        [self _pushListing:optionalConnectionIdentifier];
        return;
    }

#if PLATFORM(IOS)
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.2 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        WebThreadRun(^{
            [self _pushListing:nil];
        });
    });
#else
    [self performSelector:@selector(_pushListing:) withObject:nil afterDelay:0.2];
#endif

    _hasScheduledPush = YES;
}

- (void)pushListing
{
#if PLATFORM(IOS)
    WebThreadRun(^{
#endif

    [self pushListing:nil];

#if PLATFORM(IOS)
    });
#endif
}

- (void)_receivedSetup:(NSDictionary *)dictionary
{
    NSNumber *pageId = [dictionary objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    NSString *connectionIdentifier = [dictionary objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    NSString *sender = [dictionary objectForKey:WIRSenderKey];
    if (!sender)
        return;

    if (!_openConnections)
        _openConnections = [[NSMutableDictionary alloc] init];

    WebInspectorServerWebViewConnection *connection = [[WebInspectorServerWebViewConnection alloc] initWithController:self connectionIdentifier:connectionIdentifier destination:sender identifier:pageId];
    if ([connection setupChannel])
        [_openConnections setObject:connection forKey:pageId];
    [connection release];

    if ([_openConnections count] == 1) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [_server setHasActiveDebugSession:YES];
        });
    }

    [self pushListing];
}

- (void)_receivedData:(NSDictionary *)dictionary
{
    NSNumber *pageId = [dictionary objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    WebInspectorServerWebViewConnection *connection = [_openConnections objectForKey:pageId];
    ASSERT(!connection || [connection isKindOfClass:[WebInspectorServerWebViewConnection class]]);
    if (!connection)
        return;

    [connection receivedData:dictionary];
}

- (void)_receivedDidClose:(NSDictionary *)dictionary
{
    NSNumber *pageId = [dictionary objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    NSString *connectionIdentifier = [dictionary objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    WebInspectorServerWebViewConnection *connection = [_openConnections objectForKey:pageId];
    ASSERT(!connection || [connection isKindOfClass:[WebInspectorServerWebViewConnection class]]);
    if (!connection)
        return;

    if (![connectionIdentifier isEqualToString:connection.connectionIdentifier])
        return;

    [connection receivedDidClose:dictionary];
    [_openConnections removeObjectForKey:pageId];

    if (![_openConnections count]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [_server setHasActiveDebugSession:NO];
        });
    }

    [self pushListing];
}

- (void)_receivedGetListing:(NSDictionary *)dictionary
{
    [self pushListing:[dictionary objectForKey:WIRConnectionIdentifierKey]];
}

- (void)_receivedIndicate:(NSDictionary *)dictionary
{
    NSNumber *pageId = [dictionary objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    WebView *webView = [[[WebInspectorClientRegistry sharedRegistry] inspectableWebViews] objectForKey:pageId];
    if (!webView)
        return;

    BOOL indicateEnabled = [[dictionary objectForKey:WIRIndicateEnabledKey] boolValue];
    [webView setIndicatingForRemoteInspector:indicateEnabled];
}

- (void)_receivedConnectionDied:(NSDictionary *)dictionary
{
    NSString *connectionIdentifier = [dictionary objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    NSDictionary *connectionsCopy = [[_openConnections copy] autorelease];
    for (NSNumber *key in connectionsCopy) {
        WebInspectorServerWebViewConnection *connection = [connectionsCopy objectForKey:key];
        if ([connection.connectionIdentifier isEqualToString:connectionIdentifier]) {
            [connection receivedDidClose:nil];
            [_openConnections removeObjectForKey:key];
        }
    }

    if (![_openConnections count]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [_server setHasActiveDebugSession:NO];
        });
    }
}

- (void)receivedMessage:(NSString *)messageName userInfo:(NSDictionary *)userInfo
{
#if PLATFORM(IOS)
    WebThreadRun(^{
#endif

    if ([messageName isEqualToString:WIRSocketSetupMessage])
        [self _receivedSetup:userInfo];
    else if ([messageName isEqualToString:WIRSocketDataMessage])
        [self _receivedData:userInfo];
    else if ([messageName isEqualToString:WIRWebPageCloseMessage])
        [self _receivedDidClose:userInfo];
    else if ([messageName isEqualToString:WIRApplicationGetListingMessage])
        [self _receivedGetListing:userInfo];
    else if ([messageName isEqualToString:WIRIndicateMessage])
        [self _receivedIndicate:userInfo];
    else if ([messageName isEqualToString:WIRConnectionDiedMessage])
        [self _receivedConnectionDied:userInfo];
    else
        NSLog(@"Unrecognized Message: %@", messageName);

#if PLATFORM(IOS)
    });
#endif
}


#pragma mark -
#pragma mark WebInspectorServerWebViewConnection interface

- (void)connectionClosing:(WebInspectorServerWebViewConnection *)connection
{
#if PLATFORM(IOS)
    WebThreadRun(^{
#endif

    [_openConnections removeObjectForKey:[connection identifier]];

    if (![_openConnections count]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [_server setHasActiveDebugSession:NO];
        });
    }

#if PLATFORM(IOS)
    });
#endif
}


#pragma mark -
#pragma mark Thread Safe send message to frontend

- (void)sendMessageToFrontend:(NSString *)messageName userInfo:(NSDictionary *)userInfo
{
    // This may be called from either the WebThread or the MainThread. This
    // requires access to the WebInspectorServer's XPC Connection, which
    // is managed on the main thread. So, dispatch to the main thread to
    // safely use the xpc connection.
    dispatch_async(dispatch_get_main_queue(), ^{
        [[_server xpcConnection] sendMessage:messageName userInfo:userInfo];
    });
}

@end

#endif
