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

#pragma once

#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <Security/Security.h>

typedef NS_ENUM(uint8_t, HTTPServerProtocolBridge) {
    HTTPServerProtocolBridgeHTTP = 0,
    HTTPServerProtocolBridgeHTTPS,
    HTTPServerProtocolBridgeHTTPSWithLegacyTLS,
    HTTPServerProtocolBridgeHTTP2Raw,
    HTTPServerProtocolBridgeHTTP2,
    HTTPServerProtocolBridgeHTTP3,
    HTTPServerProtocolBridgeHTTPSProxy,
    HTTPServerProtocolBridgeHTTPSProxyWithAuthentication,
    HTTPServerProtocolBridgeHTTP2Proxy,
};

typedef NS_ENUM(uint8_t, HTTPResponseBehaviorBridge) {
    HTTPResponseBehaviorBridgeSendResponseNormally = 0,
    HTTPResponseBehaviorBridgeTerminateConnectionAfterReceivingRequest,
    HTTPResponseBehaviorBridgeNeverSendResponse,
};

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface HTTPResponseDataBridge : NSObject
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStatusCode:(NSUInteger)statusCode headerFields:(NSArray<NSArray<NSString *> *> *)headerFields body:(NSData *)body behavior:(HTTPResponseBehaviorBridge)behavior shouldRespondWith304:(BOOL)shouldRespondWith304 headerFieldsFor304:(NSDictionary<NSString *, NSString *> *)headerFieldsFor304;
@end

NS_SWIFT_UI_ACTOR
@interface HTTPServerBridge : NSObject
- (instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithRoutes:(NSDictionary<NSString *, HTTPResponseDataBridge *> *)routes protocol:(HTTPServerProtocolBridge)protocol port:(uint16_t)port identity:(nullable SecIdentityRef)identity certificateVerifier:(nullable sec_protocol_verify_t)verifier;
- (nullable instancetype)initWithProtocol:(HTTPServerProtocolBridge)protocol connectionHandler:(void (^)(nw_connection_t))handler;

@property (readonly) uint16_t port;
@property (readonly) NSInteger totalRequests;
@property (readonly) NSInteger totalConnections;
@property (readonly, copy) NSString *lastRequestCookies;
@property (readonly) BOOL sawAuthorizationHeader;

- (void)startListeningWithCompletionHandler:(void (^)(NSError * _Nullable))handler;
- (void)cancelWithCompletionHandler:(void (^)(void))handler;
- (void)terminateAllConnectionsWithCompletionHandler:(void (^)(void))handler;

- (void)addResponse:(HTTPResponseDataBridge *)response forPath:(NSString *)path;
- (void)setResponse:(HTTPResponseDataBridge *)response forPath:(NSString *)path;
@end

NS_HEADER_AUDIT_END(nullability, sendability)
