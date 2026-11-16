/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "NativeImageCocoa.h"

#import "NativeImage.h"
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <ranges>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebCore {

static RetainPtr<CFDataRef> tiffRepresentationOfImages(const std::ranges::sized_range auto& nativeImages)
{
    // If nativeImages is empty, we know for certain this image doesn't have valid data
    // Even though the call to CGImageDestinationCreateWithData will fail and we'll handle it gracefully,
    // in certain circumstances that call will spam the console with an error message
    if (std::ranges::empty(nativeImages))
        return nullptr;

    RetainPtr data = adoptCF(CFDataCreateMutable(0, 0));

    RetainPtr destination = adoptCF(CGImageDestinationCreateWithData(data.get(), bridge_cast(UTTypeTIFF.identifier), std::ranges::size(nativeImages), 0));
    if (!destination)
        return nullptr;

    for (Ref nativeImage : nativeImages)
        CGImageDestinationAddImage(destination.get(), nativeImage->platformImage().get(), 0);

    CGImageDestinationFinalize(destination.get());
    return data;
}

RetainPtr<CFDataRef> tiffRepresentation(const NativeImage& nativeImage)
{
    return tiffRepresentationOfImages(singleElementSpan(nativeImage));
}

RetainPtr<CFDataRef> tiffRepresentation(std::span<const Ref<NativeImage>> nativeImages)
{
    return tiffRepresentationOfImages(nativeImages);
}

} // namespace WebCore
