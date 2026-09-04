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
#include "Helpers/GraphicsTestUtilities.h"

#include "Helpers/WebCoreTestUtilities.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBuffer.h>

namespace TestWebKitAPI {
using namespace WebCore;

static Color imageBufferPixelAt(const ImageBuffer& imageBuffer, FloatPoint point)
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, ColorSpace::SRGB() };
    auto pixelBuffer = imageBuffer.getPixelBuffer(format, enclosingIntRect(FloatRect { point, FloatSize { 1, 1 } }));
    return Color { SRGBA<uint8_t> { pixelBuffer->item(0), pixelBuffer->item(1), pixelBuffer->item(2), pixelBuffer->item(3) } };
}

::testing::AssertionResult imageBufferPixelIs(Color expected, const ImageBuffer& imageBuffer, FloatPoint point, unsigned tolerance)
{
    auto got = imageBufferPixelAt(imageBuffer, point);
    auto [gotRed, gotGreen, gotBlue, gotAlpha] = got.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    auto [expectedRed, expectedGreen, expectedBlue, expectedAlpha] = expected.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    auto differs = [&](uint8_t a, uint8_t b) {
        return static_cast<unsigned>(a > b ? a - b : b - a) > tolerance;
    };
    if (differs(gotRed, expectedRed) || differs(gotGreen, expectedGreen) || differs(gotBlue, expectedBlue) || differs(gotAlpha, expectedAlpha)) {
        // Use this to debug the contents in the browser.
        // WTFLogAlways("%s", imageBuffer.toDataURL("image/png"_s).latin1().characters());
        return ::testing::AssertionFailure() << "color is not expected at " << point << ". Got: " << got << ", expected: " << expected << " with tolerance " << tolerance << ".";
    }
    return ::testing::AssertionSuccess();
}

::testing::AssertionResult imagePixelIs(Color expected, Image& image, FloatPoint point, unsigned tolerance)
{
    RefPtr buffer = ImageBuffer::create({ 1, 1 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.0f, ColorSpace::SRGB(),PixelFormat::BGRA8); // NOLINT
    if (!buffer)
        return ::testing::AssertionFailure() << "failed to allocate temp buffer";
    buffer->context().drawImage(image, { 0, 0, 1, 1 }, { point, FloatSize { 1, 1 } });
    return imageBufferPixelIs(expected, *buffer, { 0, 0 }, tolerance);
}

::testing::AssertionResult imagePixelIs(Color expected, NativeImage& image, FloatPoint point, unsigned tolerance)
{
    RefPtr buffer = ImageBuffer::create({ 1, 1 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.0f, ColorSpace::SRGB(),PixelFormat::BGRA8); // NOLINT
    if (!buffer)
        return ::testing::AssertionFailure() << "failed to allocate temp buffer";
    buffer->context().drawNativeImage(image, { 0, 0, 1, 1 }, { point, FloatSize { 1, 1 } });
    return imageBufferPixelIs(expected, *buffer, { 0, 0 }, tolerance);
}


}
