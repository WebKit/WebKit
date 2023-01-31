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

#include <WebCore/BifurcatedGraphicsContext.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListIterator.h>
#include <WebCore/DisplayListRecorderImpl.h>
#include <WebCore/FontCascade.h>
#include <WebCore/GradientImage.h>
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/InMemoryDisplayList.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

constexpr CGFloat contextWidth = 1;
constexpr CGFloat contextHeight = 1;

TEST(BifurcatedGraphicsContextTests, Basic)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());

    InMemoryDisplayList displayList;
    RecorderImpl secondaryContext(displayList, { }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.fillRect(FloatRect(0, 0, contextWidth, contextHeight), Color::red);

    // The primary context should have one red pixel.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);

    // The secondary context should have a red FillRectWithColor.
    EXPECT_FALSE(displayList.isEmpty());
    bool sawFillRect = false;
    for (auto displayListItem : displayList) {
        auto handle = displayListItem->item;
        if (handle.type() != ItemType::FillRectWithColor)
            continue;

        EXPECT_TRUE(handle.isDrawingItem());
        EXPECT_TRUE(handle.is<FillRectWithColor>());
        sawFillRect = true;
        auto& item = handle.get<FillRectWithColor>();
        EXPECT_EQ(item.rect(), FloatRect(0, 0, contextWidth, contextHeight));
        EXPECT_EQ(item.color(), Color::red);
    }

    EXPECT_GT(displayList.sizeInBytes(), 0U);
    EXPECT_TRUE(sawFillRect);
}

TEST(BifurcatedGraphicsContextTests, Text)
{
    InMemoryDisplayList primaryDisplayList;
    RecorderImpl primaryContext(primaryDisplayList, { }, FloatRect(0, 0, contextWidth, contextHeight), { });

    InMemoryDisplayList secondaryDisplayList;
    RecorderImpl secondaryContext(secondaryDisplayList, { }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    FontCascadeDescription description;
    description.setOneFamily("Times"_s);
    description.setComputedSize(80);
    FontCascade font(WTFMove(description));
    font.update();

    String string = "Hello!"_s;
    TextRun run(string);
    ctx.drawText(font, run, { });

    auto runTest = [&] (InMemoryDisplayList& displayList) {
        EXPECT_FALSE(displayList.isEmpty());
        bool sawDrawGlyphs = false;
        for (auto displayListItem : displayList) {
            auto handle = displayListItem->item;
            if (handle.type() != ItemType::DrawGlyphs)
                continue;

            EXPECT_TRUE(handle.isDrawingItem());
            EXPECT_TRUE(handle.is<DrawGlyphs>());
            sawDrawGlyphs = true;
        }

        EXPECT_GT(displayList.sizeInBytes(), 0U);
        EXPECT_TRUE(sawDrawGlyphs);
    };

    // Ensure that both contexts have text painting commands.
    runTest(primaryDisplayList);
    runTest(secondaryDisplayList);
}

TEST(BifurcatedGraphicsContextTests, DrawTiledGradientImage)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    auto secondaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());
    GraphicsContextCG secondaryContext(secondaryCGContext.get());
    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    auto gradient = Gradient::create(Gradient::LinearData { { 0, 0 }, { 1, 1 } }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0, Color::red });

    auto gradientImage = GradientImage::create(gradient, FloatSize { 1, 1 });

    ctx.drawTiledImage(gradientImage.get(), FloatRect { 0, 0, 100, 100 }, FloatRect { 0, 0, 1, 1 }, FloatSize { 1, 1 }, Image::RepeatTile, Image::RepeatTile);

    // The primary context should be red.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);

    // The secondary context should be red.
    CGContextFlush(secondaryCGContext.get());
    uint8_t* secondaryData = static_cast<uint8_t*>(CGBitmapContextGetData(secondaryCGContext.get()));
    EXPECT_EQ(secondaryData[0], 255);
    EXPECT_EQ(secondaryData[1], 0);
    EXPECT_EQ(secondaryData[2], 0);
}

TEST(BifurcatedGraphicsContextTests, DrawGradientImage)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    auto secondaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());
    GraphicsContextCG secondaryContext(secondaryCGContext.get());
    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    auto gradient = Gradient::create(Gradient::LinearData { { 0, 0 }, { 1, 1 } }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0, Color::red });

    auto gradientImage = GradientImage::create(gradient, FloatSize { 1, 1 });

    ctx.drawImage(gradientImage.get(), FloatRect { 0, 0, 100, 100 }, FloatRect { 0, 0, 1, 1 });

    // The primary context should be red.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);

    // The secondary context should be red.
    CGContextFlush(secondaryCGContext.get());
    uint8_t* secondaryData = static_cast<uint8_t*>(CGBitmapContextGetData(secondaryCGContext.get()));
    EXPECT_EQ(secondaryData[0], 255);
    EXPECT_EQ(secondaryData[1], 0);
    EXPECT_EQ(secondaryData[2], 0);
}

TEST(BifurcatedGraphicsContextTests, Borders)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContext(primaryCGContext.get());

    InMemoryDisplayList displayList;
    RecorderImpl secondaryContext(displayList, { }, FloatRect(0, 0, contextWidth, contextHeight), { });

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.setStrokeColor(Color::red);
    ctx.setStrokeStyle(StrokeStyle::SolidStroke);
    ctx.setStrokeThickness(10);
    ctx.drawLine({ 0, 0 }, { contextWidth, 0 });

    // The primary context should have red pixels.
    CGContextFlush(primaryCGContext.get());
    uint8_t* primaryData = static_cast<uint8_t*>(CGBitmapContextGetData(primaryCGContext.get()));
    EXPECT_EQ(primaryData[0], 255);
    EXPECT_EQ(primaryData[1], 0);
    EXPECT_EQ(primaryData[2], 0);
    EXPECT_EQ(primaryData[3], 255);
}

TEST(BifurcatedGraphicsContextTests, TransformedClip)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, 100, 100, 8, 4 * 100, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContextCG(primaryCGContext.get());
    GraphicsContext& primaryContext = primaryContextCG;

    InMemoryDisplayList displayList;
    RecorderImpl secondaryContextDL(displayList, { }, FloatRect(0, 0, 100, 100), { });
    GraphicsContext& secondaryContext = secondaryContextDL;

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.clip(FloatRect(25, 25, 50, 50));

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(25, 25, 50, 50));

    ctx.scale({ 1, -1 });
    ctx.translate(0, -100);

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(25, 25, 50, 50));

    ctx.scale(2);

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(12, 12, 26, 26));

    {
        GraphicsContextStateSaver saver(ctx);

        ctx.translate(12, 12);

        EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
        EXPECT_EQ(primaryContext.clipBounds(), FloatRect(0, 0, 26, 26));

        ctx.clip(FloatRect(0, 0, 10, 10));

        EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
        EXPECT_EQ(primaryContext.clipBounds(), FloatRect(0, 0, 10, 10));

        ctx.rotate(M_PI / 6);

        EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
        EXPECT_EQ(primaryContext.clipBounds(), FloatRect(0, -5, 14, 14));
    }

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(12, 12, 26, 26));

    // Make the CTM non-invertible.
    ctx.scale({ 0, 1 });

    EXPECT_EQ(primaryContext.clipBounds(), secondaryContext.clipBounds());
    EXPECT_EQ(primaryContext.clipBounds(), FloatRect(25, 25, 50, 50));
}

TEST(BifurcatedGraphicsContextTests, ApplyDeviceScaleFactor)
{
    auto colorSpace = DestinationColorSpace::SRGB();
    auto primaryCGContext = adoptCF(CGBitmapContextCreate(nullptr, 100, 100, 8, 4 * 100, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));

    GraphicsContextCG primaryContextCG(primaryCGContext.get());
    GraphicsContext& primaryContext = primaryContextCG;

    InMemoryDisplayList displayList;
    RecorderImpl secondaryContextDL(displayList, { }, FloatRect(0, 0, 100, 100), { });
    GraphicsContext& secondaryContext = secondaryContextDL;

    BifurcatedGraphicsContext ctx(primaryContext, secondaryContext);

    ctx.applyDeviceScaleFactor(2);

    auto primaryCTM = primaryContext.getCTM(GraphicsContext::IncludeDeviceScale::DefinitelyIncludeDeviceScale);
    auto secondaryCTM = secondaryContext.getCTM(GraphicsContext::IncludeDeviceScale::DefinitelyIncludeDeviceScale);

    EXPECT_EQ(primaryCTM.xScale(), secondaryCTM.xScale());
    EXPECT_EQ(primaryCTM.yScale(), secondaryCTM.yScale());

    EXPECT_EQ(primaryCTM.xScale(), 2);
    EXPECT_EQ(primaryCTM.yScale(), 2);
}

} // namespace TestWebKitAPI

#endif // USE(CG)
