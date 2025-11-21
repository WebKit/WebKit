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
#import "_WKAuthenticatorResponseInternal.h"

#import "_WKAuthenticationExtensionsClientOutputs.h"
#import "_WKAuthenticationExtensionsClientOutputsInternal.h"
#import <WebCore/AuthenticationExtensionsClientOutputs.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKAuthenticatorResponse {
    RetainPtr<_WKAuthenticationExtensionsClientOutputs> _extensions;
}

- (instancetype)initWithClientDataJSON:(NSData *)clientDataJSON rawId:(NSData *)rawId extensions:(RetainPtr<_WKAuthenticationExtensionsClientOutputs>&&)extensions attachment:(_WKAuthenticatorAttachment)attachment
{
    if (!(self = [super init]))
        return nil;

    _clientDataJSON = [clientDataJSON retain];
    _rawId = [rawId retain];
    _extensions = WTFMove(extensions);
    _attachment = attachment;

    return self;
}

- (instancetype)initWithClientDataJSON:(NSData *)clientDataJSON rawId:(NSData *)rawId extensionOutputsCBOR:(NSData *)extensionOutputsCBOR attachment:(_WKAuthenticatorAttachment)attachment
{
    if (!(self = [super init]))
        return nil;

    _clientDataJSON = [clientDataJSON retain];
    _rawId = [rawId retain];
    _extensionOutputsCBOR = [extensionOutputsCBOR copy];
    _attachment = attachment;

    return self;
}

- (void)dealloc
{
    SUPPRESS_UNRETAINED_ARG [_clientDataJSON release];
    SUPPRESS_UNRETAINED_ARG [_rawId release];
    SUPPRESS_UNRETAINED_ARG [_extensionOutputsCBOR release];
    [super dealloc];
}

- (_WKAuthenticationExtensionsClientOutputs *)extensions
{
    if (!_extensions && _extensionOutputsCBOR) {
        // Parse CBOR to create extensions object
        auto extensionOutputs = WebCore::AuthenticationExtensionsClientOutputs::fromCBOR(makeVector(_extensionOutputsCBOR));
        if (extensionOutputs) {
            // Only create the Cocoa extensions object if there are actual extension values
            bool hasExtensions = extensionOutputs->appid.has_value() || extensionOutputs->largeBlob.has_value();
            if (!hasExtensions)
                return nil;

            RetainPtr<_WKAuthenticationExtensionsLargeBlobOutputs> largeBlobOutputs;

            if (extensionOutputs->largeBlob) {
                BOOL supported = extensionOutputs->largeBlob->supported.value_or(false);
                RetainPtr<NSData> blob;
                if (extensionOutputs->largeBlob->blob)
                    blob = adoptNS([[NSData alloc] initWithBytes:extensionOutputs->largeBlob->blob->data() length:extensionOutputs->largeBlob->blob->byteLength()]);
                BOOL written = extensionOutputs->largeBlob->written.value_or(false);

                largeBlobOutputs = adoptNS([[_WKAuthenticationExtensionsLargeBlobOutputs alloc] initWithSupported:supported blob:blob.get() written:written]);
            }

            _extensions = adoptNS([[_WKAuthenticationExtensionsClientOutputs alloc] initWithAppid:extensionOutputs->appid.value_or(false) largeBlob:largeBlobOutputs.get()]);
        }
    }
    return _extensions.get();
}

@end
