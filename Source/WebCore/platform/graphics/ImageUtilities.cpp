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

#include "config.h"
#include "ImageUtilities.h"

#include "DestinationColorSpace.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "MIMETypeRegistry.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

Vector<uint8_t> encodeData(const NativeImage& source, const String& mimeType, std::optional<double> quality)
{
    RefPtr<const NativeImage> image;
    if (source.hasAlpha() && MIMETypeRegistry::isJPEGMIMEType(mimeType)) {
        // FIXME(https://bugs.webkit.org/show_bug.cgi?id=308704): The encoding should take in background color and not use drawing.
        auto colorSpace = source.colorSpace();
        if (!colorSpace.supportsOutput())
            colorSpace = DestinationColorSpace::SRGB();
        RefPtr buffer = ImageBuffer::create(source.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
        if (!buffer)
            return { };
        FloatRect imageRect { { }, source.size() };
        buffer->context().fillRect(imageRect, Color::black);
        // FIXME(https://bugs.webkit.org/show_bug.cgi?id=308705): const cast.
        buffer->context().drawNativeImage(const_cast<NativeImage&>(source), imageRect, imageRect);
        image = ImageBuffer::sinkIntoNativeImage(WTF::move(buffer));
        if (!image)
            return { };
    } else
        image = &source;
    return platformEncodeData(*image, mimeType, quality);
}

Vector<uint8_t> encodeData(const RefPtr<NativeImage>& image, const String& mimeType, std::optional<double> quality)
{
    if (!image)
        return { };
    return encodeData(*image, mimeType, quality);
}

Vector<uint8_t> encodeData(RefPtr<ImageBuffer>&& buffer, const String& mimeType, std::optional<double> quality)
{
    return encodeData(ImageBuffer::sinkIntoNativeImage(WTF::move(buffer)), mimeType, quality);
}

String encodeDataURL(const NativeImage& image, const String& mimeType, std::optional<double> quality)
{
    auto encodedData = encodeData(image, mimeType, quality);
    if (encodedData.isEmpty())
        return "data:,"_s;
    return makeString("data:"_s, mimeType, ";base64,"_s, base64Encoded(encodedData));
}

String encodeDataURL(const RefPtr<NativeImage>& image, const String& mimeType, std::optional<double> quality)
{
    if (!image)
        return "data:,"_s;
    return encodeDataURL(*image, mimeType, quality);
}

String encodeDataURL(RefPtr<ImageBuffer>&& buffer, const String& mimeType, std::optional<double> quality)
{
    if (!buffer)
        return "data:,"_s;
    return encodeDataURL(ImageBuffer::sinkIntoNativeImage(WTF::move(buffer)), mimeType, quality);
}

}
