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
#import <wtf/Platform.h>

#if ENABLE(PHONE_NUMBER_POSSESSION)

#if PLATFORM(IOS_FAMILY)
@class UIWindow;
typedef UIWindow *WKPhoneNumberPossessionPresentationAnchor;
#else
@class NSWindow;
typedef NSWindow *WKPhoneNumberPossessionPresentationAnchor;
#endif

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_SENDABLE
@protocol WKPhoneNumberPossessionPlatformResponse <NSObject>
@property (nonatomic, readonly) NSData *encryptedResult;
@property (nonatomic, readonly) NSString *carrierDomain;
@property (nonatomic, readonly) uint16_t carrierConfigurationID;
@property (nonatomic, readonly, nullable) NSData *encryptedDecryptionKey;
@property (nonatomic, readonly) NSData *clientDataJSON;
@end

NS_SWIFT_SENDABLE
@protocol WKPhoneNumberPossessionPlatformManaging <NSObject>
- (void)canRequestWithCompletionHandler:(void (^)(BOOL))completionHandler;
- (void)requestWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier challenge:(NSData *)challenge includesPhoneNumber:(BOOL)includesPhoneNumber phoneNumberSuffixHint:(nullable NSString *)phoneNumberSuffixHint presentationAnchor:(WKPhoneNumberPossessionPresentationAnchor)presentationAnchor completionHandler:(void (^)(id<WKPhoneNumberPossessionPlatformResponse> _Nullable, NSError * _Nullable))completionHandler NS_SWIFT_NAME(request(relyingPartyIdentifier:challenge:includesPhoneNumber:phoneNumberSuffixHint:presentationAnchor:completionHandler:));
@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
