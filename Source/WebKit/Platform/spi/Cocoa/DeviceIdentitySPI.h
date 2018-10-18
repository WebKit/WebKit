/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#if USE(APPLE_INTERNAL_SDK)

#if __IPHONE_OS_VERSION_MAX_ALLOWED < 110300
extern NSString * _Nonnull const kMAOptionsBAAAccessControls;
#endif

extern "C" {
#import <DeviceIdentity/DeviceIdentity.h>
}

#else

typedef void (^MABAACompletionBlock)(_Nullable SecKeyRef referenceKey, NSArray * _Nullable certificates, NSError * _Nullable error);

extern NSString * _Nonnull const kMAOptionsBAAAccessControls;
extern NSString * _Nonnull const kMAOptionsBAAIgnoreExistingKeychainItems;
extern NSString * _Nonnull const kMAOptionsBAAKeychainAccessGroup;
extern NSString * _Nonnull const kMAOptionsBAAKeychainLabel;
extern NSString * _Nonnull const kMAOptionsBAANonce;
extern NSString * _Nonnull const kMAOptionsBAAOIDNonce;
extern NSString * _Nonnull const kMAOptionsBAAOIDSToInclude;
extern NSString * _Nonnull const kMAOptionsBAASCRTAttestation;
extern NSString * _Nonnull const kMAOptionsBAAValidity;

extern "C"
void DeviceIdentityIssueClientCertificateWithCompletion(dispatch_queue_t _Nullable, NSDictionary * _Nullable options, MABAACompletionBlock _Nonnull);

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#endif // ENABLE(WEB_AUTHN)
