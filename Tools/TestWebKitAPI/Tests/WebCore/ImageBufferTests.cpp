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

#include "Test.h"
#include "WebCoreTestUtilities.h"
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
    auto got = Color { SRGBA<uint8_t> { frontPixelBuffer->item(0), frontPixelBuffer->item(1), frontPixelBuffer->item(2), frontPixelBuffer->item(3) } };
    if (got != expected) {
        // Use this to debug the contents in the browser.
        // WTFLogAlways("%s", imageBuffer.toDataURL("image/png"_s).latin1().data());
        return ::testing::AssertionFailure() << "color is not expected at (" << x << ", " << y << "). Got: " << got << ", expected: " << expected << ".";
    }
    return ::testing::AssertionSuccess();
}
namespace {
struct TestPattern {
    FloatRect unitRect;
    Color color;
};

}
static TestPattern g_testPattern[] = {
    { { 0.0f, 0.0f, 0.5f, 0.5f }, Color::magenta },
    { { 0.5f, 0.0f, 0.5f, 0.5f }, Color::yellow },
    { { 0.0f, 0.5f, 0.5f, 0.5f }, Color::lightGray },
    { { 0.5f, 0.5f, 0.5f, 0.5f }, Color::transparentBlack },
};

static ::testing::AssertionResult hasTestPattern(ImageBuffer& buffer)
{
    // Test pattern draws fractional pixels when deviceScaleFactor is < 1.
    // For now, account this by sampling somewhere where the fractional pixels
    // are guaranteed to not exist (4 logical pixels inwards of the pattern
    // borders).
    static constexpr float fuzz = 4.0f;

    for (auto pattern : g_testPattern) {
        auto rect = pattern.unitRect;
        rect.scale(buffer.logicalSize());
        rect = enclosingIntRect(rect);
        auto p1 = rect.minXMinYCorner();
        p1.move(fuzz, fuzz);
        auto result = imageBufferPixelIs(pattern.color, buffer, p1.x(), p1.y());
        if (!result)
            return result;
        p1 = rect.maxXMaxYCorner();
        p1.move(-fuzz, -fuzz);
        result = imageBufferPixelIs(pattern.color, buffer, p1.x() - 1, p1.y() - 1);
        if (!result)
            return result;
    }
    return ::testing::AssertionSuccess();
}

static void drawTestPattern(ImageBuffer& buffer)
{
    for (auto pattern : g_testPattern) {
        auto rect = pattern.unitRect;
        rect.scale(buffer.logicalSize());
        rect = enclosingIntRect(rect);
        buffer.context().fillRect(rect, pattern.color);
    }
}

// Tests that the specialized image buffer constructors construct the expected type of object.
// Test passes if the test compiles, there was a bug where the code wouldn't compile.
TEST(ImageBufferTests, ImageBufferSubTypeCreateCreatesSubtypes)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = PixelFormat::BGRA8;
    FloatSize size { 1.f, 1.f };
    float scale = 1.f;
    RefPtr<ImageBuffer> unaccelerated = ImageBuffer::create(size, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr<ImageBuffer> accelerated = ImageBuffer::create(size, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated });
    RefPtr<ImageBuffer> displayListAccelerated = ImageBuffer::create(size, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::UseDisplayList });
    RefPtr<ImageBuffer> displayListUnaccelerated = ImageBuffer::create(size, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated, ImageBufferOptions::UseDisplayList });

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
    auto frontImageBuffer = ImageBuffer::create(logicalSize, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated });
    auto backImageBuffer = ImageBuffer::create(logicalSize, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated });

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
        auto accelerated = ImageBuffer::create(logicalSize, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated });
        auto fillRect = FloatRect { { }, logicalSize };
        accelerated->context().fillRect(fillRect, Color::green);
        EXPECT_TRUE(imageBufferPixelIs(Color::green, *accelerated, fillRect.maxX() - 1, fillRect.maxY() - 1));
    }
    WTF::releaseFastMallocFreeMemory();
    auto initialFootprint = memoryFootprint();
    auto lastFootprint = initialFootprint;
    EXPECT_GT(lastFootprint, 0u);

    auto accelerated = ImageBuffer::create(logicalSize, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated });
    auto fillRect = FloatRect { { }, logicalSize };
    accelerated->context().fillRect(fillRect, Color::green);
    accelerated->flushContext();
    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, logicalSizeBytes, footprintError));

    auto unaccelerated = ImageBuffer::create(logicalSize, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
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

enum class TestImageBufferOptions {
    Accelerated, NoOptions
};

void PrintTo(TestImageBufferOptions value, ::std::ostream* o)
{
    if (value == TestImageBufferOptions::Accelerated)
        *o << "Accelerated";
    else if (value == TestImageBufferOptions::NoOptions)
        *o << "NoOptions";
    else
        *o << "Unknown";
}

enum class TestPreserveResolution {
    No,
    Yes
};

void PrintTo(TestPreserveResolution value, ::std::ostream* o)
{
    if (value == TestPreserveResolution::No)
        *o << "PreserveResolution_No";
    else if (value == TestPreserveResolution::Yes)
        *o << "PreserveResolution_Yes";
    else
        *o << "Unknown";
}

// ImageBuffer test fixture for tests that are variant to the image buffer device scale factor, options and the operation argument of preserving resolution
class PreserveResolutionOperationTest : public testing::TestWithParam<std::tuple<float, TestImageBufferOptions, TestPreserveResolution>> {
public:
    float deviceScaleFactor() const { return std::get<0>(GetParam()); }
    OptionSet<ImageBufferOptions> imageBufferOptions() const
    {
        auto testOptions = std::get<1>(GetParam());
        if (testOptions == TestImageBufferOptions::Accelerated)
            return ImageBufferOptions::Accelerated;
        return { };
    }
    PreserveResolution operationPreserveResolution()
    {
        if (std::get<2>(GetParam()) == TestPreserveResolution::No)
            return PreserveResolution::No;
        return PreserveResolution::Yes;
    }
};

// Test that ImageBuffer::sinkIntoImage() returns Image that contains the ImageBuffer contents and
// that the returned Image is of expected size.
TEST_P(PreserveResolutionOperationTest, SinkIntoImageWorks)
{
    FloatSize testSize { 50, 57 };
    auto buffer = ImageBuffer::create(testSize, RenderingPurpose::Unspecified, deviceScaleFactor(), DestinationColorSpace::SRGB(), PixelFormat::BGRA8, imageBufferOptions());
    ASSERT_NE(buffer, nullptr);
    auto verifyBuffer = ImageBuffer::create(buffer->logicalSize(), RenderingPurpose::Unspecified, 1.f, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    ASSERT_NE(verifyBuffer, nullptr);
    drawTestPattern(*buffer);

    auto image = ImageBuffer::sinkIntoImage(WTFMove(buffer), operationPreserveResolution());
    ASSERT_NE(image, nullptr);

    if (operationPreserveResolution() == PreserveResolution::Yes)
        EXPECT_EQ(image->size(), expandedIntSize(testSize.scaled(deviceScaleFactor())));
    else
        EXPECT_EQ(image->size(), testSize);
    verifyBuffer->context().drawImage(*image, FloatRect { { }, verifyBuffer->logicalSize() }, CompositeOperator::Copy);
    EXPECT_TRUE(hasTestPattern(*verifyBuffer));
}

// ImageBuffer test fixture for tests that are variant to the image buffer device scale factor and options
class AnyScaleTest : public testing::TestWithParam<std::tuple<float, TestImageBufferOptions>> {
public:
    float deviceScaleFactor() const { return std::get<0>(GetParam()); }
    OptionSet<ImageBufferOptions> imageBufferOptions() const
    {
        auto testOptions = std::get<1>(GetParam());
        if (testOptions == TestImageBufferOptions::Accelerated)
            return ImageBufferOptions::Accelerated;
        return { };
    }
};

// Test that ImageBuffer::getPixelBuffer() returns PixelBuffer that is sized to the ImageBuffer::logicalSize() * ImageBuffer::resolutionScale().
TEST_P(AnyScaleTest, GetPixelBufferDimensionsContainScale)
{
    IntSize testSize { 50, 57 };
    auto buffer = ImageBuffer::create(testSize, RenderingPurpose::Unspecified, deviceScaleFactor(), DestinationColorSpace::SRGB(), PixelFormat::BGRA8, imageBufferOptions());
    ASSERT_NE(buffer, nullptr);
    drawTestPattern(*buffer);

    // Test that ImageBuffer::getPixelBuffer() returns pixel buffer with dimensions that are scaled to resolutionScale() of the source.
    PixelBufferFormat testFormat { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = buffer->getPixelBuffer(testFormat, { { }, testSize });
    IntSize expectedSize = testSize;
    expectedSize.scale(deviceScaleFactor());
    EXPECT_EQ(expectedSize, pixelBuffer->size());

    // Test that the contents of the pixel buffer was as expected.
    auto verifyBuffer = ImageBuffer::create(pixelBuffer->size(), RenderingPurpose::Unspecified, 1.f, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    ASSERT_NE(verifyBuffer, nullptr);
    verifyBuffer->putPixelBuffer(*pixelBuffer, { { }, pixelBuffer->size() });
    EXPECT_TRUE(hasTestPattern(*verifyBuffer));
}

INSTANTIATE_TEST_SUITE_P(ImageBufferTests,
    PreserveResolutionOperationTest,
    testing::Combine(
        testing::Values(0.5f, 1.f, 2.f),
        testing::Values(TestImageBufferOptions::NoOptions, TestImageBufferOptions::Accelerated),
        testing::Values(TestPreserveResolution::No, TestPreserveResolution::Yes)),
    TestParametersToStringFormatter());

INSTANTIATE_TEST_SUITE_P(ImageBufferTests,
    AnyScaleTest,
    testing::Combine(
        testing::Values(0.5f, 1.f, 2.f, 5.f),
        testing::Values(TestImageBufferOptions::NoOptions, TestImageBufferOptions::Accelerated)),
    TestParametersToStringFormatter());

}
