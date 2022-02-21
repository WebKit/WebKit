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

#include "TestUtilities.h"
#include <WebCore/Color.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PixelBuffer.h>
#include <cmath>
#include <wtf/MemoryFootprint.h>

namespace TestWebKitAPI {
using namespace WebCore;

static ::testing::AssertionResult imageBufferPixelIs(Color expected, ImageBuffer& imageBuffer, int x, int y)
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto frontPixelBuffer = imageBuffer.getPixelBuffer(format, { x, y, 1, 1 });
    auto& data = frontPixelBuffer->data();
    auto got = Color { SRGBA<uint8_t> { data.item(0), data.item(1), data.item(2), data.item(3) } };
    if (got != expected)
        return ::testing::AssertionFailure() << "color is not expected. Got: " << got << ", expected: " << expected << ".";
    return ::testing::AssertionSuccess();
}

// Tests that the specialized image buffer constructors construct the expected type of object.
// Test passes if the test compiles, there was a bug where the code wouldn't compile.
TEST(ImageBufferTests, ImageBufferSubTypeCreateCreatesSubtypes)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = PixelFormat::BGRA8;
    FloatSize size { 1.f, 1.f };
    float scale = 1.f;
    RefPtr<ImageBuffer> unaccelerated = ImageBuffer::create(size, RenderingMode::Unaccelerated, scale, colorSpace, pixelFormat, nullptr);
    RefPtr<ImageBuffer> accelerated = ImageBuffer::create(size, RenderingMode::Accelerated, scale, colorSpace, pixelFormat, nullptr);
    RefPtr<ImageBuffer> displayListAccelerated = ImageBuffer::create(size, RenderingMode::Unaccelerated, ShouldUseDisplayList::Yes, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, nullptr);
    RefPtr<ImageBuffer> displayListUnaccelerated = ImageBuffer::create(size, RenderingMode::Accelerated, ShouldUseDisplayList::Yes, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, nullptr);

    EXPECT_NE(nullptr, accelerated);
    EXPECT_NE(nullptr, unaccelerated);
    EXPECT_NE(nullptr, displayListAccelerated);
    EXPECT_NE(nullptr, displayListUnaccelerated);
}

TEST(ImageBufferTests, ImageBufferSubPixelDrawing)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = PixelFormat::BGRA8;
    FloatSize logicalSize { 392, 44 };
    float scale = 1.91326535;
    auto frontImageBuffer = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, scale, colorSpace, pixelFormat, nullptr);
    auto backImageBuffer = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, scale, colorSpace, pixelFormat, nullptr);
    
    auto strokeRect = FloatRect { { }, logicalSize };
    strokeRect.inflate(-0.5);
    auto fillRect = strokeRect;
    fillRect.inflate(-1);

    auto& frontContext = frontImageBuffer->context();
    auto& backContext = backImageBuffer->context();

    frontContext.setShouldAntialias(false);
    backContext.setShouldAntialias(false);

    frontContext.setStrokeColor(Color::red);
    frontContext.strokeRect(strokeRect, 1);
    
    frontContext.fillRect(fillRect, Color::green);

    for (int i = 0; i < 1000; ++i) {
        backContext.drawImageBuffer(*frontImageBuffer, WebCore::FloatPoint { }, { WebCore::CompositeOperator::Copy });
        frontContext.drawImageBuffer(*backImageBuffer, WebCore::FloatPoint { }, { WebCore::CompositeOperator::Copy });
    }

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *frontImageBuffer, fillRect.x() + 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *frontImageBuffer, fillRect.maxX() - 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *frontImageBuffer, fillRect.x() + 1, fillRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *frontImageBuffer, fillRect.maxX() - 1, fillRect.maxY() - 1));

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *backImageBuffer, fillRect.x() + 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *backImageBuffer, fillRect.maxX() - 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *backImageBuffer, fillRect.x() + 1, fillRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *backImageBuffer, fillRect.maxX() - 1, fillRect.maxY() - 1));
}

// Test that drawing an accelerated ImageBuffer to an unaccelerated does not store extra
// memory to the accelerated ImageBuffer.
// FIXME: The test is disabled as it appears that WTF::memoryFootprint() is not exact enough to
// test that GraphicsContext::drawImageBitmap() does not keep extra memory around.
// However, if the test is paused at the memory measurement location and the process is inspected
// manually with the memory tools, the footprint is as expected, e.g. drawBitmapImage does not
// persist additional memory.
TEST(ImageBufferTests, DISABLED_DrawImageBufferDoesNotReferenceExtraMemory)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = PixelFormat::BGRA8;
    FloatSize logicalSize { 4096, 4096 };
    float scale = 1;
    size_t footprintError = 1024 * 1024;
    size_t logicalSizeBytes = logicalSize.width() * logicalSize.height() * 4;
    // FIXME: Logically this fuzz amount  should not exist.
    // WTF::memoryFootprint() does not return the same amount of memory as
    // the `footprint` command or the leak tools.
    // At the time of writing, the bug case would report drawImageBitmap footprint
    // as ~130mb, and fixed case would report ~67mb.
    size_t drawImageBitmapUnaccountedFootprint = logicalSizeBytes + 3 * 1024 * 1024;

    {
        // Make potential accelerated drawing backend instantiate roughly the global structures needed for this test.
        auto accelerated = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, scale, colorSpace, pixelFormat, nullptr);
        auto fillRect = FloatRect { { }, logicalSize };
        accelerated->context().fillRect(fillRect, Color::green);
        EXPECT_TRUE(imageBufferPixelIs(Color::green, *accelerated, fillRect.maxX() - 1, fillRect.maxY() - 1));
    }
    WTF::releaseFastMallocFreeMemory();
    auto initialFootprint = memoryFootprint();
    auto lastFootprint = initialFootprint;
    EXPECT_GT(lastFootprint, 0u);

    auto accelerated = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, scale, colorSpace, pixelFormat, nullptr);
    auto fillRect = FloatRect { { }, logicalSize };
    accelerated->context().fillRect(fillRect, Color::green);
    accelerated->flushContext();
    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, logicalSizeBytes, footprintError));

    auto unaccelerated = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, scale, colorSpace, pixelFormat, nullptr);
    unaccelerated->context().fillRect(fillRect, Color::yellow);
    EXPECT_TRUE(imageBufferPixelIs(Color::yellow, *unaccelerated, fillRect.maxX() - 1, fillRect.maxY() - 1));
    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, logicalSizeBytes, footprintError));

    // The purpose of the whole test is to test that drawImageBuffer does not increase
    // memory footprint.
    unaccelerated->context().drawImageBuffer(*accelerated, FloatRect { { }, logicalSize }, FloatRect { { }, logicalSize }, { WebCore::CompositeOperator::Copy });
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *unaccelerated, fillRect.maxX() - 1, fillRect.maxY() - 1));
    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, 0 + drawImageBitmapUnaccountedFootprint, footprintError));
    // sleep(10000); // Enable this to inspect the process manually.
    accelerated = nullptr;
    unaccelerated = nullptr;
    lastFootprint = initialFootprint;
    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, 0, footprintError));
}
}
