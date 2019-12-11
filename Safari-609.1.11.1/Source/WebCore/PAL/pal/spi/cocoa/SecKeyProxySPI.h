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

#if HAVE(SEC_KEY_PROXY)

#if USE(APPLE_INTERNAL_SDK)

#import <Security/SecKeyProxy.h>

#else

#import <Foundation/Foundation.h>
#include <Security/SecBase.h>
#include <Security/SecKey.h>

NS_ASSUME_NONNULL_BEGIN
@interface SecKeyProxy : NSObject {
@private
    id _key;
    NSData * _Nullable _certificate;
    NSXPCListener *_listener;
}
// Creates new proxy instance. Proxy holds reference to the target key or identity and allows remote access to that target key as long as the proxy instance is kept alive.
- (instancetype)initWithKey:(SecKeyRef)key;
- (instancetype)initWithIdentity:(SecIdentityRef)identity;
// Retrieve endpoint to this proxy instance. Endpoint can be transferred over NSXPCConnection and passed to +[createKeyFromEndpoint:error:] method.
@property (readonly, nonatomic) NSXPCListenerEndpoint *endpoint;
// Invalidates all connections to this proxy.
- (void)invalidate;
// Creates new SecKey/SecIdentity object which forwards all operations to the target SecKey identified by endpoint. Returned SecKeyRef can be used as long as target SecKeyProxy instance is kept alive.
+ (nullable SecKeyRef)createKeyFromEndpoint:(NSXPCListenerEndpoint *)endpoint error:(NSError **)error;
+ (nullable SecIdentityRef)createIdentityFromEndpoint:(NSXPCListenerEndpoint *)endpoint error:(NSError **)error;
@end
NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(SEC_KEY_PROXY)
