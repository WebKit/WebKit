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
#include <WebCore/CSSFilter.h>
#include <WebCore/Color.h>
#include <WebCore/DisplayListDrawingContext.h>
#include <WebCore/FEDropShadow.h>
#include <WebCore/FEImage.h>
#include <WebCore/FilterResults.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/SVGFilter.h>
#include <WebCore/SourceGraphic.h>
#include <cmath>
#include <type_traits>
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
    auto pattern = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
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
    RefPtr<ImageBuffer> unaccelerated = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr<ImageBuffer> accelerated = ImageBuffer::create(size, RenderingMode::Accelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    EXPECT_NE(nullptr, accelerated);
    EXPECT_NE(nullptr, unaccelerated);
}

TEST(ImageBufferTests, ImageBufferSubPixelDrawing)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize logicalSize { 392, 44 };
    float scale = 1.91326535;
    auto frontImageBuffer = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    auto backImageBuffer = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

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
        auto accelerated = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
        auto fillRect = FloatRect { { }, logicalSize };
        accelerated->context().fillRect(fillRect, Color::green);
        EXPECT_TRUE(imageBufferPixelIs(Color::green, *accelerated, fillRect.maxX() - 1, fillRect.maxY() - 1));
    }
    WTF::releaseFastMallocFreeMemory();
    auto initialFootprint = memoryFootprint();
    auto lastFootprint = initialFootprint;
    EXPECT_GT(lastFootprint, 0u);

    auto accelerated = ImageBuffer::create(logicalSize, RenderingMode::Accelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    auto fillRect = FloatRect { { }, logicalSize };
    accelerated->context().fillRect(fillRect, Color::green);
    accelerated->flushDrawingContext();
    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, logicalSizeBytes, footprintError));

    auto unaccelerated = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
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

// This test records setFillGradient to a DisplayList. Before replaying back the
// DisplayList, the Gradient is altered. The expectation is the DisplayList keeps
// references only to immutable Gradient.
TEST(ImageBufferTests, DisplayListSetFillGradient)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize destinationSize { 100, 100 };
    float scale = 1;

    RefPtr destination = ImageBuffer::create(destinationSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto destinationRect = FloatRect { { }, destinationSize };

    auto gradient = Gradient::create(Gradient::LinearData { destinationRect.minXMinYCorner(), destinationRect.maxXMaxYCorner() },
        { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied },
        GradientSpreadMethod::Pad,
        { },
        RenderingResourceIdentifier::generate());

    gradient->addColorStop({ 0.0f, Color::green });
    gradient->addColorStop({ 1.0f, Color::green });

    DisplayList::DrawingContext drawingContext(destinationSize);
    drawingContext.context().setFillGradient(Ref { gradient });
    drawingContext.context().fillRect(destinationRect);

    // Mutating the pattern tileImage should not affect the DisplayList drawing.
    gradient->addColorStop({ 0.0f, Color::red });
    gradient->addColorStop({ 1.0f, Color::red });

    // The DisplayList should have cloned the gradient when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.x() + 1, destinationRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.maxX() - 1, destinationRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.x() + 1, destinationRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.maxX() - 1, destinationRect.maxY() - 1));
}

// This test records setFillPattern to a DisplayList. Before replaying back the
// DisplayList, the tileImage of the Pattern is altered. The expectation is the
// DisplayList keeps references only to immutable ImageBuffers.
TEST(ImageBufferTests, DisplayListSetFillPattern)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize patternSize { 100, 100 };
    FloatSize destinationSize { 100, 100 };
    float scale = 1;

    RefPtr patternSource = ImageBuffer::create(patternSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(destinationSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto patternRect = FloatRect { { }, patternSize };
    patternSource->context().fillRect(patternRect, Color::green);

    DisplayList::DrawingContext drawingContext(destinationSize);
    drawingContext.context().setFillPattern(Pattern::create({ Ref { *patternSource } }));

    auto destinationRect = FloatRect { { }, destinationSize };
    drawingContext.context().fillRect(destinationRect);

    // Mutating the pattern tileImage should not affect the DisplayList drawing.
    patternSource->context().fillRect(destinationRect, Color::red);

    // The DisplayList should have cloned the patternSource ImageBuffer when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.x() + 1, destinationRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.maxX() - 1, destinationRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.x() + 1, destinationRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, destinationRect.maxX() - 1, destinationRect.maxY() - 1));
}

// This test records drawImageBuffer to a DisplayList. Before replaying back the
// DisplayList, the ImageBuffer is altered. The expectation is the DisplayList
// keeps references only to immutable ImageBuffers.
TEST(ImageBufferTests, DisplayListDrawImageBuffer)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize logicalSize { 4096, 4096 };
    float scale = 1;

    RefPtr source1 = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr source2 = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr source3 = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto redRect = FloatRect { { }, logicalSize };
    source1->context().fillRect(redRect, Color::red);

    auto greenRatio = 0.5;
    auto greenRect = FloatRect { FloatPoint { logicalSize.scaled((1 - greenRatio) / 2) }, logicalSize.scaled(greenRatio) };
    source2->context().fillRect(greenRect, Color::green);

    auto blueRatio = 0.25;
    auto blueRect = FloatRect { FloatPoint { logicalSize.scaled((1 - blueRatio) / 2) }, logicalSize.scaled(blueRatio) };
    source3->context().fillRect(blueRect, Color::blue);

    DisplayList::DrawingContext drawingContext(logicalSize);
    drawingContext.context().drawImageBuffer(*source1, FloatPoint());
    drawingContext.context().drawImageBuffer(*source2, FloatPoint());
    drawingContext.context().drawImageBuffer(*source3, FloatPoint());

    // Mutating the ImageBuffers should not affect the DisplayList drawing.
    source1->context().fillRect(redRect, Color::black);
    source2->context().fillRect(redRect, Color::black);
    source3->context().fillRect(redRect, Color::black);

    // The DisplayList should have cloned the source ImageBuffers when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::red, *destination, redRect.x() + 1, redRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::red, *destination, redRect.maxX() - 1, redRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::red, *destination, redRect.x() + 1, redRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::red, *destination, redRect.maxX() - 1, redRect.maxY() - 1));

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, greenRect.x() + 1, greenRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, greenRect.maxX() - 1, greenRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, greenRect.x() + 1, greenRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, greenRect.maxX() - 1, greenRect.maxY() - 1));

    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, blueRect.x() + 1, blueRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, blueRect.maxX() - 1, blueRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, blueRect.x() + 1, blueRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, blueRect.maxX() - 1, blueRect.maxY() - 1));
}

// This test records drawFilteredImageBuffer to a DisplayList. Before replaying back
// the DisplayList, the sourceImage of the Filter is altered. The expectation is the
// DisplayList keeps references only to immutable ImageBuffers.
TEST(ImageBufferTests, DisplayListDrawFilteredImageBufferSourceImage)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize sourceSize { 100, 100 };
    FloatSize destinationSize { 200, 100 };
    float scale = 1;

    RefPtr source = ImageBuffer::create(sourceSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(destinationSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto greenRect = FloatRect { { }, sourceSize };
    source->context().fillRect(greenRect, Color::green);

    Vector<Ref<FilterFunction>> functions;
    functions.append(SourceGraphic::create());
    functions.append(FEDropShadow::create(0, 0, 100, 0, Color::blue, 1));

    auto filterRegion = FloatRect { { }, destinationSize };
    RefPtr filter = CSSFilter::create(WTFMove(functions), FilterRenderingMode::Software, { 1, 1 }, filterRegion);

    DisplayList::DrawingContext drawingContext(destinationSize);

    FilterResults results;
    drawingContext.context().drawFilteredImageBuffer(source.get(), greenRect, *filter, results);

    // Mutating the ImageBuffers should not affect the DisplayList drawing.
    auto redRect = FloatRect { { }, sourceSize };
    source->context().fillRect(redRect, Color::red);

    // The DisplayList should have cloned the source ImageBuffer when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.x() + 1, filterRegion.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, filterRegion.maxX() - 1, filterRegion.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.x() + 1, filterRegion.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, filterRegion.maxX() - 1, filterRegion.maxY() - 1));
}

// This test records drawFilteredImageBuffer to a DisplayList. Before replaying back
// the DisplayList, the Filter is altered. The expectation is the DisplayList keeps
// references only to immutable Filters.
TEST(ImageBufferTests, DisplayListDrawFilteredImageBufferFilter)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize sourceSize { 100, 100 };
    FloatSize destinationSize { 200, 100 };
    float scale = 1;

    RefPtr source = ImageBuffer::create(sourceSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(destinationSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto greenRect = FloatRect { { }, sourceSize };
    source->context().fillRect(greenRect, Color::green);

    Vector<Ref<FilterFunction>> functions;
    functions.append(SourceGraphic::create());

    Ref dropShadow = FEDropShadow::create(0, 0, 100, 0, Color::blue, 1);
    functions.append(Ref { dropShadow });

    auto filterRegion = FloatRect { { }, destinationSize };
    RefPtr filter = CSSFilter::create(WTFMove(functions), FilterRenderingMode::Software, { 1, 1 }, filterRegion);

    DisplayList::DrawingContext drawingContext(destinationSize);

    FilterResults results;
    drawingContext.context().drawFilteredImageBuffer(source.get(), greenRect, *filter, results);

    // Mutating the filter should not affect the DisplayList drawing.
    dropShadow->setShadowColor(Color::red);

    // The DisplayList should have cloned the filter when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.x() + 1, filterRegion.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, filterRegion.maxX() - 1, filterRegion.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.x() + 1, filterRegion.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::blue, *destination, filterRegion.maxX() - 1, filterRegion.maxY() - 1));
}

// This test records drawFilteredImageBuffer to a DisplayList. Before replaying
// back the DisplayList, the SourceImage of an FEImage in the Filter is altered.
// The expectation is the DisplayList keeps references only to immutable Filters.
TEST(ImageBufferTests, DisplayListDrawFilteredImageBufferFEImage)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize logicalSize { 100, 100 };
    float scale = 1;

    RefPtr source = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto fillRect = FloatRect { { }, logicalSize };
    source->context().fillRect(fillRect, Color::green);

    FilterEffectVector effects;
    effects.append(FEImage::create({ *source }, fillRect, { }));

    SVGFilterExpression expression;
    expression.append({ 0, 0, std::nullopt });

    auto targetBoundingBox = fillRect;
    auto filterRegion = fillRect;
    RefPtr filter = SVGFilter::create(targetBoundingBox, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE, WTFMove(expression), WTFMove(effects), std::nullopt, FilterRenderingMode::Software, { 1, 1 }, filterRegion);

    DisplayList::DrawingContext drawingContext(logicalSize);

    FilterResults results;
    drawingContext.context().drawFilteredImageBuffer(nullptr, fillRect, *filter, results);

    // Mutating the filter should not affect the DisplayList drawing.
    source->context().fillRect(fillRect, Color::red);

    // The DisplayList should have cloned the filter feImage SourceImages when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.x() + 1, filterRegion.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.maxX() - 1, filterRegion.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.x() + 1, filterRegion.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, filterRegion.maxX() - 1, filterRegion.maxY() - 1));
}

// This test records clipToImageBuffer to a DisplayList. Before replaying back the
// DisplayList, the ImageBuffer is altered. The expectation is the DisplayList keeps
// references only to immutable ImageBuffers.
TEST(ImageBufferTests, DisplayListClipToImageBuffer)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize logicalSize { 100, 100 };
    float scale = 1;

    RefPtr maskSource = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto ratio = 0.5;
    auto maskRect = FloatRect { FloatPoint { logicalSize.scaled((1 - ratio) / 2) }, logicalSize.scaled(ratio) };
    maskSource->context().fillRect(maskRect, Color::white);

    DisplayList::DrawingContext drawingContext(logicalSize);

    auto fillRect = FloatRect { { }, logicalSize };
    drawingContext.context().fillRect(fillRect, Color::green);
    drawingContext.context().clipToImageBuffer(*maskSource, fillRect);
    drawingContext.context().fillRect(fillRect, Color::red);

    // Mutating the ImageBuffers should not affect the DisplayList drawing.
    maskSource->context().fillRect(fillRect, Color::white);

    // The DisplayList should have cloned the maskSource ImageBuffer when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.x() + 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.maxX() - 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.x() + 1, fillRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.maxX() - 1, fillRect.maxY() - 1));
}

// This test records drawPattern to a DisplayList. Before replaying back the DisplayList,
// the ImageBuffer is altered. The expectation is the DisplayList keeps references only
// to immutable ImageBuffers.
TEST(ImageBufferTests, DisplayListDrawPattern)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;
    FloatSize logicalSize { 100, 100 };
    float scale = 1;

    RefPtr patternSource = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);
    RefPtr destination = ImageBuffer::create(logicalSize, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, scale, colorSpace, pixelFormat);

    auto fillRect = FloatRect { { }, logicalSize };
    patternSource->context().fillRect(fillRect, Color::green);

    DisplayList::DrawingContext drawingContext(logicalSize);

    FloatPoint phase;
    FloatSize spacing;
    drawingContext.context().drawPattern(*patternSource, fillRect, fillRect, AffineTransform(), phase, spacing, ImagePaintingOptions());

    // Mutating the ImageBuffers should not affect the DisplayList drawing.
    patternSource->context().fillRect(fillRect, Color::red);

    // The DisplayList should have cloned the ImageBuffer when recording drawImageBuffer().
    drawingContext.replayDisplayList(destination->context());

    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.x() + 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.maxX() - 1, fillRect.y() + 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.x() + 1, fillRect.maxY() - 1));
    EXPECT_TRUE(imageBufferPixelIs(Color::green, *destination, fillRect.maxX() - 1, fillRect.maxY() - 1));
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
class AnyScaleTest : public testing::TestWithParam<std::tuple<float, RenderingMode>> {
public:
    float deviceScaleFactor() const { return std::get<0>(GetParam()); }
    RenderingMode renderingMode() const { return std::get<1>(GetParam()); }
};

// Test that ImageBuffer::sinkIntoNativeImage() returns NativeImage that contains the ImageBuffer contents and
// that the returned NativeImage is of expected size (native image size * image buffer scale factor).
TEST_P(AnyScaleTest, SinkIntoNativeImageWorks)
{
    FloatSize testSize { 50, 57 };
    auto buffer = ImageBuffer::create(testSize, renderingMode(), RenderingPurpose::Unspecified, deviceScaleFactor(), DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(buffer, nullptr);
    auto verifyBuffer = ImageBuffer::create(buffer->logicalSize(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
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
    auto buffer = ImageBuffer::create(testSize, renderingMode(), RenderingPurpose::Unspecified, deviceScaleFactor(), DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(buffer, nullptr);
    drawTestPattern(*buffer, 0);

    // Test that ImageBuffer::getPixelBuffer() returns pixel buffer with dimensions that are scaled to resolutionScale() of the source.
    PixelBufferFormat testFormat { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = buffer->getPixelBuffer(testFormat, { { }, testSize });
    IntSize expectedSize = testSize;
    expectedSize.scale(deviceScaleFactor());
    EXPECT_EQ(expectedSize, pixelBuffer->size());

    // Test that the contents of the pixel buffer was as expected.
    auto verifyBuffer = ImageBuffer::create(pixelBuffer->size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1.f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(verifyBuffer, nullptr);
    verifyBuffer->putPixelBuffer(*pixelBuffer, { { }, pixelBuffer->size() });
    EXPECT_TRUE(hasTestPattern(*verifyBuffer, 0));
}

// ImageBuffer test fixture for tests that are variant to two image buffer options. Mostly useful
// for example source - destination tests
class AnyTwoImageBufferOptionsTest : public testing::TestWithParam<std::tuple<RenderingMode, RenderingMode>> {
public:
    RenderingMode renderingMode0() const { return std::get<0>(GetParam()); }
    RenderingMode renderingMode1() const { return std::get<1>(GetParam()); }
};

TEST_P(AnyTwoImageBufferOptionsTest, PutPixelBufferAffectsDrawOutput)
{
    IntSize testSize { 50, 57 };
    auto source = ImageBuffer::create(testSize, renderingMode0(), RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(source, nullptr);
    auto destination = ImageBuffer::create(testSize, renderingMode1(), RenderingPurpose::Unspecified, 1.0f, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
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
        testing::Values(RenderingMode::Unaccelerated, RenderingMode::Accelerated)),
    TestParametersToStringFormatter());

INSTANTIATE_TEST_SUITE_P(ImageBufferTests,
    AnyTwoImageBufferOptionsTest,
    testing::Combine(
        testing::Values(RenderingMode::Unaccelerated, RenderingMode::Accelerated),
        testing::Values(RenderingMode::Unaccelerated, RenderingMode::Accelerated)),
    TestParametersToStringFormatter());

}
