/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "GraphicsTestUtilities.h"

#include "WebCoreTestUtilities.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBuffer.h>

namespace TestWebKitAPI {
using namespace WebCore;

::testing::AssertionResult imageBufferPixelIs(Color expected, const ImageBuffer& imageBuffer, FloatPoint point)
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = imageBuffer.getPixelBuffer(format, enclosingIntRect(FloatRect { point, FloatSize { 1, 1 } }));
    auto got = Color { SRGBA<uint8_t> { pixelBuffer->item(0), pixelBuffer->item(1), pixelBuffer->item(2), pixelBuffer->item(3) } };
    if (got != expected) {
        // Use this to debug the contents in the browser.
        // WTFLogAlways("%s", imageBuffer.toDataURL("image/png"_s).latin1().data());
        return ::testing::AssertionFailure() << "color is not expected at " << point << ". Got: " << got << ", expected: " << expected << ".";
    }
    return ::testing::AssertionSuccess();
}

::testing::AssertionResult imagePixelIs(Color expected, Image& image, FloatPoint point)
{
    RefPtr buffer = ImageBuffer::create({ 1, 1 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(),PixelFormat::BGRA8); // NOLINT
    if (!buffer)
        return ::testing::AssertionFailure() << "failed to allocate temp buffer";
    buffer->context().drawImage(image, { 0, 0, 1, 1 }, { point, FloatSize { 1, 1 } });
    return imageBufferPixelIs(expected, *buffer, { 0, 0 });
}

::testing::AssertionResult imagePixelIs(Color expected, NativeImage& image, FloatPoint point)
{
    RefPtr buffer = ImageBuffer::create({ 1, 1 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(),PixelFormat::BGRA8); // NOLINT
    if (!buffer)
        return ::testing::AssertionFailure() << "failed to allocate temp buffer";
    buffer->context().drawNativeImage(image, { 0, 0, 1, 1 }, { point, FloatSize { 1, 1 } });
    return imageBufferPixelIs(expected, *buffer, { 0, 0 });
}


}
