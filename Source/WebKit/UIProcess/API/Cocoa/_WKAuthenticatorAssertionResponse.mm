/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKAuthenticatorAssertionResponse.h"

#import "_WKAuthenticationExtensionsClientOutputs.h"
#import "_WKAuthenticatorResponseInternal.h"
#import <wtf/RetainPtr.h>

@implementation _WKAuthenticatorAssertionResponse {
    RetainPtr<NSData> m_authenticatorData;
    RetainPtr<NSData> m_signature;
    RetainPtr<NSData> m_userHandle;
}

- (instancetype)initWithClientDataJSON:(NSData *)clientDataJSON rawId:(NSData *)rawId extensions:(RetainPtr<_WKAuthenticationExtensionsClientOutputs>&&)extensions authenticatorData:(NSData *)authenticatorData signature:(NSData *)signature userHandle:(NSData *)userHandle attachment:(_WKAuthenticatorAttachment)attachment
{
    if (!(self = [super initWithClientDataJSON:clientDataJSON rawId:rawId extensions:WTF::move(extensions) attachment:attachment]))
        return nil;

    m_authenticatorData = authenticatorData;
    m_signature = signature;
    m_userHandle = userHandle;
    return self;
}

- (instancetype)initWithClientDataJSON:(NSData *)clientDataJSON rawId:(NSData *)rawId extensionOutputsCBOR:(NSData *)extensionOutputsCBOR authenticatorData:(NSData *)authenticatorData signature:(NSData *)signature userHandle:(NSData *)userHandle attachment:(_WKAuthenticatorAttachment)attachment
{
    if (!(self = [super initWithClientDataJSON:clientDataJSON rawId:rawId extensionOutputsCBOR:extensionOutputsCBOR attachment:attachment]))
        return nil;

    m_authenticatorData = authenticatorData;
    m_signature = signature;
    m_userHandle = userHandle;
    return self;
}

- (NSData *)authenticatorData
{
    return m_authenticatorData.get();
}

- (NSData *)signature
{
    return m_signature.get();
}

- (NSData *)userHandle
{
    return m_userHandle.get();
}

- (void)dealloc
{
    [super dealloc];
}

@end
