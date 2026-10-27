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

#import "AutomationConnection.h"

#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <arpa/inet.h>
#import <string.h>
#import <unistd.h>

// Each message is a JSON object with an "event" field, preceded by its length as a
// 4-byte big-endian integer. minidriver launches a MiniBrowser for each session, so this
// connection hosts a single automation target, for a single session. MiniBrowser quits
// when the session ends, or when the connection closes.

static const uint64_t automationTargetIdentifier = 1;
static const uint64_t clientConnectionIdentifier = 1;

@implementation AutomationConnection {
    __weak id<AutomationConnectionDelegate> _delegate;
    dispatch_io_t _channel;
    NSMutableData *_receiveBuffer;
    _WKAutomationSession *_session;
    BOOL _paired;
    BOOL _closed;
}

- (instancetype)initWithFileDescriptor:(int)fileDescriptor delegate:(id<AutomationConnectionDelegate>)delegate
{
    if (!(self = [super init]))
        return nil;

    _delegate = delegate;
    _receiveBuffer = [NSMutableData data];
    _channel = dispatch_io_create(DISPATCH_IO_STREAM, fileDescriptor, dispatch_get_main_queue(), ^(int error) {
        close(fileDescriptor);
    });
    if (!_channel) {
        close(fileDescriptor);
        return nil;
    }
    dispatch_io_set_low_water(_channel, 1);

    return self;
}

- (void)open
{
    __weak AutomationConnection *weakSelf = self;
    dispatch_io_read(_channel, 0, SIZE_MAX, dispatch_get_main_queue(), ^(bool done, dispatch_data_t data, int error) {
        AutomationConnection *strongSelf = weakSelf;
        if (!strongSelf || strongSelf->_closed)
            return;

        if (data)
            [strongSelf _didReceiveData:data];

        // The read is done at end of file or on error.
        if (done) {
            if (error)
                NSLog(@"Reading from the automation connection failed: %s", strerror(error));
            [strongSelf _close];
        }
    });
}

- (void)_close
{
    if (_closed)
        return;

    _closed = YES;
    dispatch_io_close(_channel, DISPATCH_IO_STOP);
    // Removing the handler disconnects the session, which ends it.
    [_session _setMessageToFrontendHandlerForTesting:nil];
    _session = nil;
    [_delegate automationConnectionDidClose:self];
}

- (void)_didReceiveData:(dispatch_data_t)data
{
    dispatch_data_apply(data, ^bool(dispatch_data_t region, size_t offset, const void *buffer, size_t size) {
        [_receiveBuffer appendBytes:buffer length:size];
        return true;
    });

    NSUInteger offset = 0;
    while (!_closed && _receiveBuffer.length - offset >= sizeof(uint32_t)) {
        uint32_t messageSize;
        [_receiveBuffer getBytes:&messageSize range:NSMakeRange(offset, sizeof(messageSize))];
        messageSize = ntohl(messageSize);
        if (_receiveBuffer.length - offset - sizeof(uint32_t) < messageSize)
            break;

        NSData *message = [_receiveBuffer subdataWithRange:NSMakeRange(offset + sizeof(uint32_t), messageSize)];
        offset += sizeof(uint32_t) + messageSize;

        NSDictionary *event = [NSJSONSerialization JSONObjectWithData:message options:0 error:nil];
        if (![event isKindOfClass:NSDictionary.class]) {
            NSLog(@"Received an automation message that isn't a JSON object.");
            continue;
        }
        [self _didReceiveEvent:event];
    }

    if (!_closed)
        [_receiveBuffer replaceBytesInRange:NSMakeRange(0, offset) withBytes:NULL length:0];
}

- (void)_sendEvent:(NSDictionary *)event
{
    if (_closed)
        return;

    NSData *payload = [NSJSONSerialization dataWithJSONObject:event options:0 error:nil];
    if (!payload)
        return;

    uint32_t messageSize = htonl((uint32_t)payload.length);
    NSMutableData *message = [NSMutableData dataWithBytes:&messageSize length:sizeof(messageSize)];
    [message appendData:payload];

    dispatch_data_t data = dispatch_data_create(message.bytes, message.length, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    dispatch_io_write(_channel, 0, data, dispatch_get_main_queue(), ^(bool done, dispatch_data_t remainingData, int error) {
        if (done && error)
            NSLog(@"Writing to the automation connection failed: %s", strerror(error));
    });
}

static NSString *stringFromJSONObject(id object)
{
    NSData *data = [NSJSONSerialization dataWithJSONObject:object options:0 error:nil];
    return data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"";
}

static id JSONObjectFromString(id string)
{
    if (![string isKindOfClass:NSString.class])
        return nil;
    return [NSJSONSerialization JSONObjectWithData:[string dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
}

- (void)_didReceiveEvent:(NSDictionary *)event
{
    NSString *name = event[@"event"];
    if ([name isEqualToString:@"StartAutomationSession"])
        [self _startAutomationSession:JSONObjectFromString(event[@"message"])];
    else if ([name isEqualToString:@"Setup"])
        [self _setupTarget:event];
    else if ([name isEqualToString:@"SendMessageToBackend"]) {
        if ([event[@"targetID"] unsignedLongLongValue] == automationTargetIdentifier && [event[@"message"] isKindOfClass:NSString.class])
            [_session _dispatchMessageFromRemoteForTesting:event[@"message"]];
    } else if ([name isEqualToString:@"FrontendDidClose"])
        [self _close];
    else
        NSLog(@"Received an unknown automation event: %@", name);
}

- (void)_startAutomationSession:(NSDictionary *)message
{
    // A second request is invalid, because the connection hosts a single session.
    _WKAutomationSession *session = nil;
    NSString *sessionIdentifier = [message isKindOfClass:NSDictionary.class] ? message[@"sessionID"] : nil;
    if (![sessionIdentifier isKindOfClass:NSString.class] || _session)
        NSLog(@"Ignoring an invalid request to start an automation session.");
    else {
        _WKAutomationSessionConfiguration *configuration = [[_WKAutomationSessionConfiguration alloc] init];
        NSDictionary *capabilities = message[@"capabilities"];
        if ([capabilities isKindOfClass:NSDictionary.class])
            configuration.acceptInsecureCertificates = [capabilities[@"acceptInsecureCerts"] boolValue];

        session = [_delegate automationConnection:self createSessionWithIdentifier:sessionIdentifier configuration:configuration];
        _session = session;
    }

    // Answer every request, so that minidriver doesn't wait for a session that won't start. It gives
    // up on the session if the target list doesn't have its automation target.

    [self _sendEvent:@{
        @"event": @"StartAutomationSession_Return",
        @"message": stringFromJSONObject(@{
            @"browserName": [_delegate browserNameForAutomationConnection:self],
            @"browserVersion": [_delegate browserVersionForAutomationConnection:self],
        }),
    }];

    NSArray *targetList = @[ ];
    if (session) {
        targetList = @[ @{
            @"type": @"automation",
            @"name": session.sessionIdentifier,
            @"targetID": @(automationTargetIdentifier),
            @"isPaired": @NO,
        } ];
    }

    [self _sendEvent:@{
        @"event": @"SetTargetList",
        @"connectionID": @(clientConnectionIdentifier),
        @"message": stringFromJSONObject(targetList),
    }];
}

- (void)_setupTarget:(NSDictionary *)event
{
    // Installing another handler would disconnect the session, and end it.
    if (!_session || _paired || [event[@"targetID"] unsignedLongLongValue] != automationTargetIdentifier)
        return;

    _paired = YES;
    __weak AutomationConnection *weakSelf = self;
    [_session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
        [weakSelf _sendEvent:@{
            @"event": @"SendMessageToFrontend",
            @"connectionID": @(clientConnectionIdentifier),
            @"targetID": @(automationTargetIdentifier),
            @"message": message,
        }];
    }];
}

@end
