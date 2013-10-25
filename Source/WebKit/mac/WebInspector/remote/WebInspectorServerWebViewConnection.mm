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

#import "WebInspectorServerWebViewConnection.h"

#import "WebInspectorClient.h"
#import "WebInspectorRelayDefinitions.h"
#import "WebInspectorRemoteChannel.h"
#import "WebInspectorServer.h"
#import "WebInspectorServerWebViewConnectionController.h"
#import "WebInspectorXPCWrapper.h"
#import "WebKitLogging.h"
#import <wtf/text/CString.h>

@implementation WebInspectorServerWebViewConnection

- (id)initWithController:(WebInspectorServerWebViewConnectionController *)controller connectionIdentifier:(NSString *)connectionIdentifier destination:(NSString *)destination identifier:(NSNumber *)identifier
{
    self = [super init];
    if (!self)
        return nil;

    _controller = controller;
    _connectionIdentifier = [connectionIdentifier copy];
    _destination = [destination copy];
    _identifier = [identifier copy];

    return self;
}

- (BOOL)setupChannel
{
    ASSERT(!_channel);
    unsigned pageId = [_identifier unsignedIntValue];
    _channel = [WebInspectorRemoteChannel createChannelForPageId:pageId connection:self];
    return _channel != nil;
}

- (void)dealloc
{
    ASSERT(!_channel);
    [_connectionIdentifier release];
    [_destination release];
    [_identifier release];
    [super dealloc];
}

- (NSString *)connectionIdentifier
{
    return [[_connectionIdentifier copy] autorelease];
}

- (NSNumber *)identifier
{
    return [[_identifier copy] autorelease];
}


#pragma mark -
#pragma mark Interaction with the channel to the local inspector backend

- (void)clearChannel
{
    _channel = nil;
    [_controller connectionClosing:self];
}

- (void)sendMessageToFrontend:(NSString *)message
{
    NSData *data = [message dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *outgoing = [NSDictionary dictionaryWithObjectsAndKeys:
        data, WIRRawDataKey,
        _connectionIdentifier, WIRConnectionIdentifierKey,
        _destination, WIRDestinationKey,
        nil];

    [_controller sendMessageToFrontend:WIRRawDataMessage userInfo:outgoing];
}

- (void)sendMessageToBackend:(NSString *)message
{
    ASSERT(_channel);
    LOG(RemoteInspector, "WebInspectorServerConnection: Received: |%s|", String(message).utf8().data());

    [_channel sendMessageToBackend:message];
}


#pragma mark -
#pragma mark Interaction with the server to the remote inspector frontend

- (void)receivedData:(NSDictionary *)dictionary
{
    NSData *data = [dictionary objectForKey:WIRSocketDataKey];
    NSString *message = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    [self sendMessageToBackend:message];
    [message release];
}

- (void)receivedDidClose:(NSDictionary *)dictionary
{
    if (_channel) {
        [_channel closeFromRemoteSide];
        [_channel release];
        _channel = nil;
    }
}

@end

#endif
