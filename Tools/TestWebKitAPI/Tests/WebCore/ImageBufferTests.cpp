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
#include <type_traits>
#include <wtf/MemoryFootprint.h>

namespace TestWebKitAPI {
using namespace WebCore;

static SRGBA<uint8_t> getImageBufferPixel(ImageBuffer& imageBuffer, IntPoint p)
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto frontPixelBuffer = imageBuffer.getPixelBuffer(format, { p, { 1, 1 } });
    return SRGBA<uint8_t> { frontPixelBuffer->item(0), frontPixelBuffer->item(1), frontPixelBuffer->item(2), frontPixelBuffer->item(3) };
}

static ::testing::AssertionResult imageBufferPixelIs(Color expected, ImageBuffer& imageBuffer, IntPoint p)
{
    auto got = getImageBufferPixel(imageBuffer, p);
    if (got != expected) {
        // Use this to debug the contents in the browser.
        // WTFLogAlways("%s", imageBuffer.toDataURL("image/png"_s).latin1().data());
        return ::testing::AssertionFailure() << "color is not expected at (" << p.x() << ", " << p.y() << "). Got: " << got << ", expected: " << expected << ".";
    }
    return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult imageBufferPixelIs(Color expected, ImageBuffer& imageBuffer, int x, int y)
{
    return imageBufferPixelIs(expected, imageBuffer, { x, y });
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

static ::testing::AssertionResult hasTestPattern(ImageBuffer& buffer, int seed)
{
    // Test pattern draws fractional pixels when deviceScaleFactor is < 1.
    // For now, account this by sampling somewhere where the fractional pixels
    // are guaranteed to not exist (4 logical pixels inwards of the pattern
    // borders).
    static constexpr float fuzz = 4.0f;
    constexpr auto patternCount = std::extent_v<decltype(g_testPattern)>;
    for (size_t i = 0; i < patternCount; ++i) {
        auto& pattern = g_testPattern[(i + seed) % patternCount];
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

static void drawTestPattern(ImageBuffer& buffer, int seed)
{
    auto& context = buffer.context();
    bool savedShouldAntialias = context.shouldAntialias();
    context.setShouldAntialias(false);
    constexpr auto patternCount = std::extent_v<decltype(g_testPattern)>;
    for (size_t i = 0; i < patternCount; ++i) {
        auto& pattern = g_testPattern[(i + seed) % patternCount];
        auto rect = pattern.unitRect;
        rect.scale(buffer.logicalSize());
        rect = enclosingIntRect(rect);
        context.fillRect(rect, pattern.color);
    }
    context.setShouldAntialias(savedShouldAntialias);
}

static RefPtr<PixelBuffer> createPixelBufferTestPattern(IntSize size, AlphaPremultiplication alphaFormat, int seed)
{
    auto pattern = ImageBuffer::create(size, RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!pattern)
        return nullptr;
    drawTestPattern(*pattern, 1);
    EXPECT_TRUE(hasTestPattern(*pattern, 1));
    if (!hasTestPattern(*pattern, 1)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    PixelBufferFormat testFormat { alphaFormat, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    return pattern->getPixelBuffer(testFormat, { { }, size });
}

// Tests that the specialized image buffer constructors construct the expected type of object.
// Test passes if the test compiles, there was a bug where the code wouldn't compile.
TEST(ImageBufferTests, ImageBufferSubTypeCreateCreatesSubtypes)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize size { 1.f, 1.f };
    float scale = 1.f;
    RefPtr<ImageBuffer> unaccelerated = ImageBuffer::create(size, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr<ImageBuffer> accelerated = ImageBuffer::create(size, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat, { ImageBufferOptions::Accelerated });

    EXPECT_NE(nullptr, accelerated);
    EXPECT_NE(nullptr, unaccelerated);
}

TEST(ImageBufferTests, ImageBufferSubPixelDrawing)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
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
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
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
    accelerated->flushDrawingContext();
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

// Tests that creating buffers through different createScaledImageBuffer() constructors set up the
// buffer in the same way.
TEST(ImageBufferTests, ImageBufferCreateScaledImageBuffersMatch)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    RefPtr<ImageBuffer> factory = ImageBuffer::create({ 1.f, 1.f }, RenderingPurpose::Unspecified, 1.f, colorSpace, PixelFormat::BGRA8);

    struct {
        FloatSize scale;
        FloatSize size;
        FloatSize expectedLogicalSize;
        IntSize expectedBackendSize;
    } subcases[] = {
        {
            // Unclamped case.
            { 1.23f, 1.24f },
            { 77.f, 88.f },
            { 95.f, 110.f },
            { 95, 110 }
        }, {
            // Clamped case.
            { 1.23f, 1.24f },
            { 7700.f, 28800.f },
            { 2109.3603516f, 7953.6982422f },
            { 2110, 7954 }
        }, {
            // Essentially equal case. Filters might require intermediary images that are essentially integral,
            // but not exactly due to how layout works with floats.
            { 2.f, 2.f },
            { 120.000007629f, 120.000007629f },
            { 240.f, 240.f },
            { 240, 240 }
        }
    };

    for (auto& subcase : subcases) {
        SCOPED_TRACE(::testing::Message() <<  "subcase.size=" << subcase.size);
        FloatRect rect { { }, subcase.size };
        auto bufferFromRect = factory->context().createScaledImageBuffer(rect, subcase.scale, colorSpace, std::nullopt);
        auto bufferFromSize = factory->context().createScaledImageBuffer(rect.size(), subcase.scale, colorSpace, std::nullopt);
        EXPECT_EQ(bufferFromRect->context().scaleFactor(), bufferFromSize->context().scaleFactor());
        EXPECT_EQ(bufferFromRect->logicalSize(), bufferFromSize->logicalSize());
        EXPECT_EQ(bufferFromRect->logicalSize(), subcase.expectedLogicalSize);
        EXPECT_EQ(bufferFromRect->backendSize(), bufferFromSize->backendSize());
        EXPECT_EQ(bufferFromRect->backendSize(), subcase.expectedBackendSize);

        // Drawing a full rect of Color::green should result in the backing store
        // getting all pixels to solid Color::green.
        bufferFromRect->context().fillRect(rect, Color::green);
        bufferFromSize->context().fillRect(rect, Color::green);
        // Sample the last pixel of backing store to ensure it is the same for both of the images.
        IntPoint samplePixelPoint { subcase.expectedBackendSize - IntSize { 1, 1 } };

        // FIXME: Logically this should be Color::green, as Color::green was drawn.
        // It is not due to non-integral buffer sizes, will be fixed later.
        auto expectedColor = getImageBufferPixel(*bufferFromRect, samplePixelPoint);
        EXPECT_TRUE(imageBufferPixelIs(expectedColor, *bufferFromRect, samplePixelPoint));
        EXPECT_TRUE(imageBufferPixelIs(expectedColor, *bufferFromSize, samplePixelPoint));
    }
}

enum class TestImageBufferOptions {
    Accelerated, NoOptions
};

OptionSet<ImageBufferOptions> toImageBufferOptions(TestImageBufferOptions testOptions)
{
    if (testOptions == TestImageBufferOptions::Accelerated)
        return ImageBufferOptions::Accelerated;
    return { };
}

void PrintTo(TestImageBufferOptions value, ::std::ostream* o)
{
    if (value == TestImageBufferOptions::Accelerated)
        *o << "Accelerated";
    else if (value == TestImageBufferOptions::NoOptions)
        *o << "NoOptions";
    else
        *o << "Unknown";
}

enum class TestPreserveResolution : bool { No, Yes };

void PrintTo(TestPreserveResolution value, ::std::ostream* o)
{
    if (value == TestPreserveResolution::No)
        *o << "PreserveResolution_No";
    else if (value == TestPreserveResolution::Yes)
        *o << "PreserveResolution_Yes";
    else
        *o << "Unknown";
}

// ImageBuffer test fixture for tests that are variant to the image buffer device scale factor and options
class AnyScaleTest : public testing::TestWithParam<std::tuple<float, TestImageBufferOptions>> {
public:
    float deviceScaleFactor() const { return std::get<0>(GetParam()); }
    OptionSet<ImageBufferOptions> imageBufferOptions() const
    {
        return toImageBufferOptions(std::get<1>(GetParam()));
    }
};

// Test that ImageBuffer::sinkIntoNativeImage() returns NativeImage that contains the ImageBuffer contents and
// that the returned NativeImage is of expected size (native image size * image buffer scale factor).
TEST_P(AnyScaleTest, SinkIntoNativeImageWorks)
{
    FloatSize testSize { 50, 57 };
    auto buffer = ImageBuffer::create(testSize, RenderingPurpose::Unspecified, deviceScaleFactor(), DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, imageBufferOptions());
    ASSERT_NE(buffer, nullptr);
    auto verifyBuffer = ImageBuffer::create(buffer->logicalSize(), RenderingPurpose::Unspecified, 1.f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(verifyBuffer, nullptr);
    drawTestPattern(*buffer, 0);

    auto image = ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
    ASSERT_NE(image, nullptr);

    EXPECT_EQ(image->size(), expandedIntSize(testSize.scaled(deviceScaleFactor())));
    verifyBuffer->context().drawNativeImage(*image, FloatRect { { }, verifyBuffer->logicalSize() }, { { }, image->size() }, { CompositeOperator::Copy });
    EXPECT_TRUE(hasTestPattern(*verifyBuffer, 0));
}

// Test that ImageBuffer::getPixelBuffer() returns PixelBuffer that is sized to the ImageBuffer::logicalSize() * ImageBuffer::resolutionScale().
TEST_P(AnyScaleTest, GetPixelBufferDimensionsContainScale)
{
    IntSize testSize { 50, 57 };
    auto buffer = ImageBuffer::create(testSize, RenderingPurpose::Unspecified, deviceScaleFactor(), DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, imageBufferOptions());
    ASSERT_NE(buffer, nullptr);
    drawTestPattern(*buffer, 0);

    // Test that ImageBuffer::getPixelBuffer() returns pixel buffer with dimensions that are scaled to resolutionScale() of the source.
    PixelBufferFormat testFormat { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = buffer->getPixelBuffer(testFormat, { { }, testSize });
    IntSize expectedSize = testSize;
    expectedSize.scale(deviceScaleFactor());
    EXPECT_EQ(expectedSize, pixelBuffer->size());

    // Test that the contents of the pixel buffer was as expected.
    auto verifyBuffer = ImageBuffer::create(pixelBuffer->size(), RenderingPurpose::Unspecified, 1.f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(verifyBuffer, nullptr);
    verifyBuffer->putPixelBuffer(*pixelBuffer, { { }, pixelBuffer->size() });
    EXPECT_TRUE(hasTestPattern(*verifyBuffer, 0));
}

// ImageBuffer test fixture for tests that are variant to two image buffer options. Mostly useful
// for example source - destination tests
class AnyTwoImageBufferOptionsTest : public testing::TestWithParam<std::tuple<TestImageBufferOptions, TestImageBufferOptions>> {
public:
    OptionSet<ImageBufferOptions> imageBufferOptions0() const
    {
        return toImageBufferOptions(std::get<0>(GetParam()));
    }
    OptionSet<ImageBufferOptions> imageBufferOptions1() const
    {
        return toImageBufferOptions(std::get<1>(GetParam()));
    }
};

TEST_P(AnyTwoImageBufferOptionsTest, PutPixelBufferAffectsDrawOutput)
{
    IntSize testSize { 50, 57 };
    auto source = ImageBuffer::create(testSize, RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, imageBufferOptions0());
    ASSERT_NE(source, nullptr);
    auto destination = ImageBuffer::create(testSize, RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8, imageBufferOptions1());
    ASSERT_NE(destination, nullptr);
    auto pattern1Buffer = createPixelBufferTestPattern(testSize, AlphaPremultiplication::Unpremultiplied, 1);
    ASSERT_NE(pattern1Buffer, nullptr);

    drawTestPattern(*source, 0);
    EXPECT_TRUE(hasTestPattern(*source, 0));
    destination->context().drawImageBuffer(*source, FloatRect { { }, testSize }, FloatRect { { }, testSize }, { WebCore::CompositeOperator::Copy });
    EXPECT_TRUE(hasTestPattern(*destination, 0));
    source->putPixelBuffer(*pattern1Buffer, { { }, pattern1Buffer->size() });
    destination->context().drawImageBuffer(*source, FloatRect { { }, testSize }, FloatRect { { }, testSize }, { WebCore::CompositeOperator::Copy });
    EXPECT_TRUE(hasTestPattern(*destination, 1));
}

INSTANTIATE_TEST_SUITE_P(ImageBufferTests,
    AnyScaleTest,
    testing::Combine(
        testing::Values(0.5f, 1.f, 2.f, 5.f),
        testing::Values(TestImageBufferOptions::NoOptions, TestImageBufferOptions::Accelerated)),
    TestParametersToStringFormatter());

INSTANTIATE_TEST_SUITE_P(ImageBufferTests,
    AnyTwoImageBufferOptionsTest,
    testing::Combine(
        testing::Values(TestImageBufferOptions::NoOptions, TestImageBufferOptions::Accelerated),
        testing::Values(TestImageBufferOptions::NoOptions, TestImageBufferOptions::Accelerated)),
    TestParametersToStringFormatter());

}
