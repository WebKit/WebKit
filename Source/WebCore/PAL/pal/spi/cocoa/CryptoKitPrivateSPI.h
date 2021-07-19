/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if HAVE(RSA_BSSA)

#if USE(APPLE_INTERNAL_SDK)

// FIXME(227598): Remove conditional once CryptoKitPrivate/RSABSSA.h is available.
#if __has_include(<CryptoKitPrivate/RSABSSA.h>)
#import <CryptoKitPrivate/RSABSSA.h>
#else
#import <CryptoKitCBridging/RSABSSA.h>
#endif

#else

@interface RSABSSATokenWaitingActivation : NSObject
#if HAVE(RSA_BSSA)
- (RSABSSATokenReady*)activateTokenWithServerResponse:(NSData*)serverResponse error:(NSError* __autoreleasing *)error;
#endif
@property (nonatomic, retain, readonly) NSData* blindedMessage;
@end

@interface RSABSSATokenReady : NSObject
@property (nonatomic, retain, readonly) NSData* tokenContent;
@property (nonatomic, retain, readonly) NSData* keyId;
@property (nonatomic, retain, readonly) NSData* signature;
@end

#if HAVE(RSA_BSSA)
@interface RSABSSATokenBlinder : NSObject
- (instancetype)initWithPublicKey:(NSData*)spkiBytes error:(NSError* __autoreleasing *)error;
- (RSABSSATokenWaitingActivation*)tokenWaitingActivationWithContent:(NSData*)content error:(NSError* __autoreleasing *)error;
@property (nonatomic, retain, readonly) NSData* keyId;
@end
#endif

#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(RSA_BSSA)
