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

#include "config.h"

#if USE(CG)

#include <WebCore/DestinationColorSpace.h>
#include <WebCore/GraphicsContextCG.h>

namespace TestWebKitAPI {
using namespace WebCore;

constexpr CGFloat contextWidth = 1;
constexpr CGFloat contextHeight = 1;

RetainPtr<CGImageRef> greenImage()
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG ctx(cgContext.get());
    ctx.fillRect(FloatRect(0, 0, contextWidth, contextHeight), Color::green);
    return adoptCF(CGBitmapContextCreateImage(cgContext.get()));
}

TEST(GraphicsContextTests, DrawNativeImageDoesNotLeakCompositeOperator)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG ctx(cgContext.get());

    EXPECT_EQ(ctx.compositeOperation(), CompositeOperator::SourceOver);
    EXPECT_EQ(ctx.blendMode(), BlendMode::Normal);

    auto greenPixelCG = greenImage();
    auto greenPixelNative = NativeImage::create(greenPixelCG.get());
    FloatRect rect(0, 0, contextWidth, contextHeight);
    FloatRect sourceRect(0, 0, contextWidth / 2, contextHeight / 2);
    ctx.drawNativeImage(*greenPixelNative, { contextWidth, contextHeight }, rect, sourceRect, { WebCore::CompositeOperator::Copy });

    CGContextSetRGBFillColor(cgContext.get(), 0, 0, 0, 0);
    CGContextFillRect(cgContext.get(), rect);

    // The context should have one green pixel.
    CGContextFlush(cgContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(cgContext.get()));
    EXPECT_EQ(primaryData[0], 0);
    EXPECT_EQ(primaryData[1], 255);
    EXPECT_EQ(primaryData[2], 0);

    EXPECT_EQ(ctx.compositeOperation(), CompositeOperator::SourceOver);
    EXPECT_EQ(ctx.blendMode(), BlendMode::Normal);
}

} // namespace TestWebKitAPI

#endif // USE(CG)
