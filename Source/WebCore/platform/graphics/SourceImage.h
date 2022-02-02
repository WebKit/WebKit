/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#pragma once

#include "ImageBuffer.h"
#include "NativeImage.h"
#include "RenderingResourceIdentifier.h"

namespace WebCore {

class WEBCORE_EXPORT SourceImage {
public:
    using ImageVariant = std::variant<
        Ref<NativeImage>,
        Ref<ImageBuffer>,
        RenderingResourceIdentifier
    >;

    SourceImage(ImageVariant&&);

    NativeImage* nativeImageIfExists() const;
    NativeImage* nativeImage() const;

    ImageBuffer* imageBufferIfExists() const;
    ImageBuffer* imageBuffer() const;

    RenderingResourceIdentifier imageIdentifier() const;
    IntSize size() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SourceImage> decode(Decoder&);

private:
    ImageVariant m_imageVariant;
    mutable std::optional<ImageVariant> m_transformedImageVariant;
};

template<class Encoder>
void SourceImage::encode(Encoder& encoder) const
{
    encoder << imageIdentifier();
}

template<class Decoder>
std::optional<SourceImage> SourceImage::decode(Decoder& decoder)
{
    std::optional<RenderingResourceIdentifier> imageIdentifier;
    decoder >> imageIdentifier;
    if (!imageIdentifier)
        return std::nullopt;

    return SourceImage(*imageIdentifier);
}

} // namespace WebCore
