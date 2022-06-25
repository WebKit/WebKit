/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "CocoaImage.h"

#import <WebCore/UTIRegistry.h>

#if HAVE(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK)
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#else
#import <CoreServices/CoreServices.h>
#endif

namespace WebKit {

RetainPtr<NSData> transcode(CGImageRef image, CFStringRef typeIdentifier)
{
    if (!image)
        return nil;

    auto data = adoptNS([[NSMutableData alloc] init]);
    auto destination = adoptCF(CGImageDestinationCreateWithData((__bridge CFMutableDataRef)data.get(), typeIdentifier, 1, nil));
    CGImageDestinationAddImage(destination.get(), image, nil);
    if (!CGImageDestinationFinalize(destination.get()))
        return nil;

    return data;
}

std::pair<RetainPtr<NSData>, RetainPtr<CFStringRef>> transcodeWithPreferredMIMEType(CGImageRef image, CFStringRef preferredMIMEType)
{
    ASSERT(CFStringGetLength(preferredMIMEType));
#if HAVE(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK)
    auto preferredTypeIdentifier = RetainPtr { (__bridge CFStringRef)[UTType typeWithMIMEType:(__bridge NSString *)preferredMIMEType conformingToType:UTTypeImage].identifier };
#else
    auto preferredTypeIdentifier = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, preferredMIMEType, kUTTypeImage));
#endif
    if (WebCore::isSupportedImageType(preferredTypeIdentifier.get())) {
        if (auto data = transcode(image, preferredTypeIdentifier.get()); [data length])
            return { WTFMove(data), WTFMove(preferredTypeIdentifier) };
    }

    return { nil, nil };
}

} // namespace WebKit
