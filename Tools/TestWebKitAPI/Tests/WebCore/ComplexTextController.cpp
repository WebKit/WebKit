/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ComplexTextController.h>
#include <WebCore/FontCascade.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace TestWebKitAPI {

class ComplexTextControllerTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
        JSC::initializeThreading();
        RunLoop::initializeMainRunLoop();
    }
};

TEST_F(ComplexTextControllerTest, DISABLED_InitialAdvanceWithLeftRunInRTL)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(80);
    FontCascade font(description);
    font.update();
    auto spaceWidth = font.primaryFont().spaceWidth();

#if USE_LAYOUT_SPECIFIC_ADVANCES
    Vector<CGSize> advances = { CGSizeZero, CGSizeMake(21.640625, 0.0), CGSizeMake(42.3046875, 0.0), CGSizeMake(55.8984375, 0.0), CGSizeMake(22.34375, 0.0) };
    Vector<CGPoint> origins = { CGPointMake(-15.15625, 18.046875), CGPointZero, CGPointZero, CGPointZero, CGPointZero };
#else
    Vector<CGSize> advances = { CGSizeMake(15.15625, -18.046875), CGSizeMake(21.640625, 0.0), CGSizeMake(42.3046875, 0.0), CGSizeMake(55.8984375, 0.0), CGSizeMake(22.34375, 0.0) };
    Vector<CGPoint> origins = { };
#endif

    CGSize initialAdvance = CGSizeMake(-15.15625, 18.046875);

    UChar characters[] = { 0x644, 0x637, 0x641, 0x627, 0x64b, 0x20 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run1 = ComplexTextController::ComplexTextRun::createForTesting({ CGSizeMake(21.875, 0) }, { CGPointZero }, { 5 }, { 5 }, CGSizeZero, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(5, 1), false);
    auto run2 = ComplexTextController::ComplexTextRun::createForTesting(advances, origins, { 193, 377, 447, 431, 458 }, { 4, 3, 2, 1, 0 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(0, 5), false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run1));
    runs.append(WTFMove(run2));
    ComplexTextController controller(font, textRun, runs);

    CGFloat totalWidth = 0;
    for (size_t i = 1; i < advances.size(); ++i)
        totalWidth += advances[i].width;
    EXPECT_NEAR(controller.totalWidth(), spaceWidth + totalWidth, 0.0001);
    GlyphBuffer glyphBuffer;
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), advances[4].width, 0.0001);
    controller.advance(6, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth + totalWidth, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), 0, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 6U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), advances[4].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), advances[3].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), advances[2].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).width(), advances[1].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(4).width(), -initialAdvance.width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(5).width(), spaceWidth + initialAdvance.width, 0.0001);
}

TEST_F(ComplexTextControllerTest, DISABLED_InitialAdvanceInRTL)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(80);
    FontCascade font(description);
    font.update();

#if USE_LAYOUT_SPECIFIC_ADVANCES
    Vector<CGSize> advances = { CGSizeZero, CGSizeMake(21.640625, 0.0), CGSizeMake(42.3046875, 0.0), CGSizeMake(55.8984375, 0.0), CGSizeMake(22.34375, 0.0) };
    Vector<CGPoint> origins = { CGPointMake(-15.15625, 18.046875), CGPointZero, CGPointZero, CGPointZero, CGPointZero };
#else
    Vector<CGSize> advances = { CGSizeMake(15.15625, -18.046875), CGSizeMake(21.640625, 0.0), CGSizeMake(42.3046875, 0.0), CGSizeMake(55.8984375, 0.0), CGSizeMake(22.34375, 0.0) };
    Vector<CGPoint> origins = { };
#endif

    CGSize initialAdvance = CGSizeMake(-15.15625, 18.046875);

    UChar characters[] = { 0x644, 0x637, 0x641, 0x627, 0x64b };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run = ComplexTextController::ComplexTextRun::createForTesting(advances, origins, { 193, 377, 447, 431, 458 }, { 4, 3, 2, 1, 0 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(0, 5), false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    CGFloat totalWidth = 0;
    for (size_t i = 1; i < advances.size(); ++i)
        totalWidth += advances[i].width;
    EXPECT_NEAR(controller.totalWidth(), totalWidth, 0.0001);
    GlyphBuffer glyphBuffer;
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), advances[4].width, 0.0001);
    controller.advance(5, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), totalWidth, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), initialAdvance.width, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), initialAdvance.height, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 5U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), advances[4].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), advances[3].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), advances[2].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).width(), advances[1].width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(4).width(), -initialAdvance.width, 0.0001);
}

TEST_F(ComplexTextControllerTest, DISABLED_InitialAdvanceWithLeftRunInLTR)
{
    FontCascadeDescription description;
    description.setOneFamily("LucidaGrande");
    description.setComputedSize(80);
    FontCascade font(description);
    font.update();
    auto spaceWidth = font.primaryFont().spaceWidth();

#if USE_LAYOUT_SPECIFIC_ADVANCES
    Vector<CGSize> advances = { CGSizeMake(76.347656, 0.000000), CGSizeMake(0.000000, 0.000000) };
    Vector<CGPoint> origins = { CGPointZero, CGPointMake(-23.281250, -8.398438) };
#else
    Vector<CGSize> advances = { CGSizeMake(53.066406, -8.398438), CGSizeMake(23.281250, 8.398438) };
    Vector<CGPoint> origins = { };
#endif

    CGSize initialAdvance = CGSizeMake(28.144531, 0);

    UChar characters[] = { 0x20, 0x61, 0x20e3 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run1 = ComplexTextController::ComplexTextRun::createForTesting({ CGSizeMake(spaceWidth, 0) }, { CGPointZero }, { 5 }, { 0 }, CGSizeZero, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(0, 1), true);
    auto run2 = ComplexTextController::ComplexTextRun::createForTesting(advances, origins, { 68, 1471 }, { 1, 2 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(1, 2), true);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run1));
    runs.append(WTFMove(run2));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalWidth(), spaceWidth + 76.347656 + initialAdvance.width, 0.0001);
    GlyphBuffer glyphBuffer;
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth, 0.0001);
    controller.advance(2, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth + advances[0].width + initialAdvance.width, 0.0001);
    controller.advance(3, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth + 76.347656 + initialAdvance.width, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), 0, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 3U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), spaceWidth + initialAdvance.width, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), 53.066406, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), 23.281250, 0.0001);
}

TEST_F(ComplexTextControllerTest, DISABLED_InitialAdvanceInLTR)
{
    FontCascadeDescription description;
    description.setOneFamily("LucidaGrande");
    description.setComputedSize(80);
    FontCascade font(description);
    font.update();

#if USE_LAYOUT_SPECIFIC_ADVANCES
    Vector<CGSize> advances = { CGSizeMake(76.347656, 0.000000), CGSizeMake(0.000000, 0.000000) };
    Vector<CGPoint> origins = { CGPointZero, CGPointMake(-23.281250, -8.398438) };
#else
    Vector<CGSize> advances = { CGSizeMake(53.066406, -8.398438), CGSizeMake(23.281250, 8.398438) };
    Vector<CGPoint> origins = { };
#endif

    CGSize initialAdvance = CGSizeMake(28.144531, 0);

    UChar characters[] = { 0x61, 0x20e3 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run = ComplexTextController::ComplexTextRun::createForTesting(advances, origins, { 68, 1471 }, { 0, 1 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(0, 2), true);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalWidth(), 76.347656 + initialAdvance.width, 0.0001);
    GlyphBuffer glyphBuffer;
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), advances[0].width + initialAdvance.width, 0.0001);
    controller.advance(2, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 76.347656 + initialAdvance.width, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), initialAdvance.width, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), initialAdvance.height, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 2U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), 53.066406, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), 23.281250, 0.0001);
}

}
