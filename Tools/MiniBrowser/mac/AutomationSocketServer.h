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

#import <Foundation/Foundation.h>
#import <WebKit/_WKAutomationSession.h>

// AutomationSocketServer provides a TCP socket transport for WebDriver automation.
// It listens on a local port and bridges length-prefixed JSON messages to/from
// a _WKAutomationSession, bypassing webinspectord entirely.
//
// Wire format: [4 bytes big-endian length][JSON UTF-8 payload]

@protocol AutomationSocketServerDelegate <NSObject>
- (void)automationSocketServerDidReceiveSessionRequest:(NSString *)sessionIdentifier capabilities:(NSDictionary *)capabilities;
- (void)automationSocketServerDidReceiveMessageToBackend:(NSString *)message;
- (void)automationSocketServerClientDidDisconnect;
@end

@interface AutomationSocketServer : NSObject

@property (nonatomic, weak) id<AutomationSocketServerDelegate> delegate;
@property (nonatomic, readonly) uint16_t port;
@property (nonatomic, readonly, getter=isListening) BOOL listening;

- (BOOL)startListeningOnPort:(uint16_t)port error:(NSError **)error;
- (void)stop;

// Send a message back to the connected driver (response or event).
- (void)sendMessageToDriver:(NSString *)message;

// Connect the server to a _WKAutomationSession for message routing.
// This sets up the FrontendChannel bridge so automation responses
// flow back through the socket.
- (void)connectToAutomationSession:(_WKAutomationSession *)session;

// Forward an automation protocol message to the connected session.
// This calls WebAutomationSession::dispatchMessageFromRemote() via C++ bridge.
- (void)dispatchMessageToSession:(NSString *)message;

@end
