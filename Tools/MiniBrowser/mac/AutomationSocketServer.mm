/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "AutomationSocketServer.h"

#import <WebKit/_WKAutomationSession.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

static const size_t kHeaderSize = 4;

@implementation AutomationSocketServer {
    int _listenSocket;
    int _clientSocket;
    dispatch_source_t _listenSource;
    dispatch_source_t _clientReadSource;
    NSMutableData *_readBuffer;
    _WKAutomationSession *_connectedSession;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _listenSocket = -1;
        _clientSocket = -1;
        _readBuffer = [[NSMutableData alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [self stop];
}

- (BOOL)startListeningOnPort:(uint16_t)port error:(NSError **)error
{
    _listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenSocket < 0) {
        if (error)
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSLocalizedDescriptionKey: @"Failed to create socket" }];
        return NO;
    }

    int reuse = 1;
    setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(_listenSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (error)
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSLocalizedDescriptionKey: @"Failed to bind" }];
        close(_listenSocket);
        _listenSocket = -1;
        return NO;
    }

    if (listen(_listenSocket, 1) < 0) {
        if (error)
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{ NSLocalizedDescriptionKey: @"Failed to listen" }];
        close(_listenSocket);
        _listenSocket = -1;
        return NO;
    }

    _port = port;
    _listening = YES;

    _listenSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, _listenSocket, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_listenSource, ^{
        [self acceptClient];
    });
    dispatch_resume(_listenSource);

    NSLog(@"AutomationSocketServer: Listening on port %u", port);
    return YES;
}

- (void)stop
{
    if (_clientReadSource) {
        dispatch_source_cancel(_clientReadSource);
        _clientReadSource = nil;
    }
    if (_listenSource) {
        dispatch_source_cancel(_listenSource);
        _listenSource = nil;
    }
    if (_clientSocket >= 0) {
        close(_clientSocket);
        _clientSocket = -1;
    }
    if (_listenSocket >= 0) {
        close(_listenSocket);
        _listenSocket = -1;
    }
    _listening = NO;
    _connectedSession = nil;
}

- (void)acceptClient
{
    struct sockaddr_in clientAddr = { };
    socklen_t clientLen = sizeof(clientAddr);
    int clientFd = accept(_listenSocket, (struct sockaddr *)&clientAddr, &clientLen);
    if (clientFd < 0)
        return;

    // Only one client at a time.
    if (_clientSocket >= 0) {
        NSLog(@"AutomationSocketServer: Rejecting second client, already connected");
        close(clientFd);
        return;
    }

    _clientSocket = clientFd;
    [_readBuffer setLength:0];

    NSLog(@"AutomationSocketServer: Client connected");

    _clientReadSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, _clientSocket, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_clientReadSource, ^{
        [self readFromClient];
    });
    dispatch_source_set_cancel_handler(_clientReadSource, ^{
        // Socket cleanup handled in stop/disconnect.
    });
    dispatch_resume(_clientReadSource);
}

- (void)readFromClient
{
    uint8_t chunk[4096];
    ssize_t bytesRead = read(_clientSocket, chunk, sizeof(chunk));
    if (bytesRead <= 0) {
        NSLog(@"AutomationSocketServer: Client disconnected (read returned %zd)", bytesRead);
        [self clientDisconnected];
        return;
    }

    [_readBuffer appendBytes:chunk length:bytesRead];

    // Process complete messages.
    while (_readBuffer.length >= kHeaderSize) {
        const uint8_t *bytes = (const uint8_t *)_readBuffer.bytes;
        uint32_t messageLength = 0;
        memcpy(&messageLength, bytes, kHeaderSize);
        messageLength = ntohl(messageLength);

        if (_readBuffer.length < kHeaderSize + messageLength)
            break;

        NSData *jsonData = [_readBuffer subdataWithRange:NSMakeRange(kHeaderSize, messageLength)];
        [_readBuffer replaceBytesInRange:NSMakeRange(0, kHeaderSize + messageLength) withBytes:NULL length:0];

        NSError *parseError = nil;
        NSDictionary *message = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&parseError];
        if (!message || ![message isKindOfClass:[NSDictionary class]]) {
            NSLog(@"AutomationSocketServer: Failed to parse message: %@", parseError);
            continue;
        }

        [self handleMessage:message];
    }
}

- (void)handleMessage:(NSDictionary *)message
{
    NSString *type = message[@"type"];
    if (!type) {
        NSLog(@"AutomationSocketServer: Message missing 'type' field");
        return;
    }

    if ([type isEqualToString:@"automationSessionRequest"]) {
        NSString *sessionIdentifier = message[@"sessionIdentifier"];
        NSDictionary *capabilities = message[@"capabilities"];
        NSLog(@"AutomationSocketServer: Received session request for %@", sessionIdentifier);
        [self.delegate automationSocketServerDidReceiveSessionRequest:sessionIdentifier capabilities:capabilities ?: @{ }];
        return;
    }

    if ([type isEqualToString:@"sendMessageToBackend"]) {
        NSString *payload = message[@"message"];
        if (payload)
            [self.delegate automationSocketServerDidReceiveMessageToBackend:payload];
        return;
    }

    NSLog(@"AutomationSocketServer: Unknown message type: %@", type);
}

- (void)sendRawMessageToDriver:(NSDictionary *)envelope
{
    if (_clientSocket < 0)
        return;

    NSData *envelopeData = [NSJSONSerialization dataWithJSONObject:envelope options:0 error:nil];
    if (!envelopeData)
        return;

    uint32_t length = htonl((uint32_t)envelopeData.length);
    write(_clientSocket, &length, kHeaderSize);
    write(_clientSocket, envelopeData.bytes, envelopeData.length);
}

- (void)sendMessageToDriver:(NSString *)message
{
    [self sendRawMessageToDriver:@{
        @"type": @"sendMessageToFrontend",
        @"message": message,
    }];
}

- (void)connectToAutomationSession:(_WKAutomationSession *)session
{
    _connectedSession = session;

    // Use the _WKAutomationSession SPI to connect a block-based FrontendChannel.
    // The block is called whenever the session sends a response or event to the driver.
    __weak AutomationSocketServer *weakSelf = self;
    [session _connectWithMessageHandler:^(NSString *message) {
        [weakSelf sendMessageToDriver:message];
    }];

    NSLog(@"AutomationSocketServer: Connected to automation session %@", session.sessionIdentifier);

    // Send success response to driver.
    [self sendRawMessageToDriver:@{
        @"type": @"automationSessionCreated",
        @"sessionIdentifier": session.sessionIdentifier ?: @"",
        @"browserName": @"MiniBrowser",
        @"browserVersion": [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"unknown",
    }];
}

- (void)dispatchMessageToSession:(NSString *)message
{
    if (!_connectedSession) {
        NSLog(@"AutomationSocketServer: No connected session for message dispatch");
        return;
    }

    // Use the _WKAutomationSession SPI to dispatch the automation protocol message.
    [_connectedSession _dispatchMessageFromRemote:message];
}

- (void)clientDisconnected
{
    if (_clientReadSource) {
        dispatch_source_cancel(_clientReadSource);
        _clientReadSource = nil;
    }
    if (_clientSocket >= 0) {
        close(_clientSocket);
        _clientSocket = -1;
    }
    _connectedSession = nil;
    [_readBuffer setLength:0];

    [self.delegate automationSocketServerClientDidDisconnect];
}

@end
