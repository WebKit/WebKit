/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#import "WebInspectorServer.h"

#import "WebInspectorClient.h"
#import "WebInspectorRelayDefinitions.h"
#import "WebInspectorServerWebViewConnectionController.h"
#import "WebInspectorXPCWrapper.h"
#import "WebViewPrivate.h"
#import <notify.h>
#import <xpc/xpc.h>


#pragma mark -
#pragma mark Private Class Details

@interface WebInspectorServer () <WebInspectorXPCWrapperDelegate>
- (void)setupXPCConnectionIfNeeded;
- (void)xpcConnection:(WebInspectorXPCWrapper *)connection receivedMessage:(NSString *)messageName userInfo:(NSDictionary *)userInfo;
- (void)xpcConnectionFailed:(WebInspectorXPCWrapper *)connection;
@end


#pragma mark -
#pragma mark Server Implementation

@implementation WebInspectorServer

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

    _connectionController = [[WebInspectorServerWebViewConnectionController alloc] initWithServer:self];

    [[WebInspectorClientRegistry sharedRegistry] setDelegate:self];

    return self;
}

- (void)dealloc
{
    // Singleton is not expected to be released.

    [self stop];

    ASSERT(!_xpcConnection);
    [_connectionController release];

    [[WebInspectorClientRegistry sharedRegistry] setDelegate:nil];

    [super dealloc];
}

- (void)start
{
    if (_isEnabled)
        return;

    _isEnabled = YES;

    notify_register_dispatch(WIRServiceAvailableNotification, &_notifyToken, dispatch_get_main_queue(), ^(int token) {
        [self setupXPCConnectionIfNeeded];
    });

    notify_post(WIRServiceAvailabilityCheckNotification);
}

- (void)stop
{
    if (!_isEnabled)
        return;

    _isEnabled = NO;

    [_connectionController closeAllConnections];

    if (_xpcConnection) {
        [_xpcConnection close];
        _xpcConnection.delegate = nil;
        [_xpcConnection release];
        _xpcConnection = nil;
    }

    notify_cancel(_notifyToken);
}

- (BOOL)isEnabled
{
    return _isEnabled;
}

- (WebInspectorXPCWrapper *)xpcConnection
{
    return _xpcConnection;
}

- (void)setupXPCConnectionIfNeeded
{
    if (_xpcConnection)
        return;

    xpc_connection_t connection = xpc_connection_create_mach_service(WIRXPCMachPortName, dispatch_get_main_queue(), 0);
    if (connection) {
        _xpcConnection = [[WebInspectorXPCWrapper alloc] initWithConnection:connection];
        _xpcConnection.delegate = self;
        [_xpcConnection sendMessage:@"syn" userInfo:nil];
        xpc_release(connection);
    }
}

- (void)pushListing
{
    if (!_isEnabled || !_xpcConnection)
        return;

    [_connectionController pushListing];
}

- (BOOL)hasActiveDebugSession
{
    return _hasActiveDebugSession;
}

- (void)setHasActiveDebugSession:(BOOL)hasSession
{
    if (_hasActiveDebugSession == hasSession)
        return;

    _hasActiveDebugSession = hasSession;

    [[NSNotificationCenter defaultCenter] postNotificationName:_WebViewRemoteInspectorHasSessionChangedNotification object:nil];
}


#pragma mark -
#pragma mark WebInspectorXPCWrapperDelegate Implementation

- (void)xpcConnection:(WebInspectorXPCWrapper *)xpcConnection receivedMessage:(NSString *)messageName userInfo:(NSDictionary *)userInfo
{
    ASSERT(xpcConnection == _xpcConnection);

    if ([messageName isEqualToString:WIRPermissionDenied])
        [self stop];
    else
        [_connectionController receivedMessage:messageName userInfo:userInfo];
}

- (void)xpcConnectionFailed:(WebInspectorXPCWrapper *)connection
{
    [_connectionController closeAllConnections];

    if (_xpcConnection) {
        _xpcConnection.delegate = nil;
        [_xpcConnection release];
        _xpcConnection = nil;
    }
}


#pragma mark -
#pragma mark WebInspectorClientRegistryDelegate implementation

- (void)didRegisterClient:(WebInspectorClient*)client
{
    // When a WebView is registered it has no content. But some WebViews
    // never load content from a URL, such as UITextFields on iOS.
    [self pushListing];
}

- (void)didUnregisterClient:(WebInspectorClient*)client
{
    [self pushListing];
}

@end

#endif
