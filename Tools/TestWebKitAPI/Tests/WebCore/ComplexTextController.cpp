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
        JSC::initializeThreading();
        RunLoop::initializeMain();
    }
};

TEST_F(ComplexTextControllerTest, InitialAdvanceWithLeftRunInRTL)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(80);
    FontCascade font(WTFMove(description));
    font.update();
    auto spaceWidth = font.primaryFont().spaceWidth();

    Vector<FloatSize> advances = { FloatSize(), FloatSize(21.640625, 0.0), FloatSize(42.3046875, 0.0), FloatSize(55.8984375, 0.0), FloatSize(22.34375, 0.0) };
    Vector<FloatPoint> origins = { FloatPoint(-15.15625, 18.046875), FloatPoint(), FloatPoint(), FloatPoint(), FloatPoint() };

    FloatSize initialAdvance = FloatSize(-15.15625, 18.046875);

    UChar characters[] = { 0x644, 0x637, 0x641, 0x627, 0x64b, 0x20 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run1 = ComplexTextController::ComplexTextRun::create({ FloatSize(21.875, 0) }, { FloatPoint() }, { 5 }, { 5 }, FloatSize(), font.primaryFont(), characters, 0, charactersLength, 5, 6, false);
    auto run2 = ComplexTextController::ComplexTextRun::create(advances, origins, { 193, 377, 447, 431, 458 }, { 4, 3, 2, 1, 0 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, 0, 5, false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run1));
    runs.append(WTFMove(run2));
    ComplexTextController controller(font, textRun, runs);

    float totalWidth = 0;
    for (size_t i = 1; i < advances.size(); ++i)
        totalWidth += advances[i].width();
    EXPECT_NEAR(controller.totalAdvance().width(), spaceWidth + totalWidth, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), advances[4].width(), 0.0001);
    controller.advance(6, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth + totalWidth, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), 0, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 6U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), advances[4].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), advances[3].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), advances[2].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).width(), advances[1].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(4).width(), -initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(5).width(), spaceWidth + initialAdvance.width(), 0.0001);
}

TEST_F(ComplexTextControllerTest, InitialAdvanceInRTL)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(80);
    FontCascade font(WTFMove(description));
    font.update();

    Vector<FloatSize> advances = { FloatSize(), FloatSize(21.640625, 0.0), FloatSize(42.3046875, 0.0), FloatSize(55.8984375, 0.0), FloatSize(22.34375, 0.0) };
    Vector<FloatPoint> origins = { FloatPoint(-15.15625, 18.046875), FloatPoint(), FloatPoint(), FloatPoint(), FloatPoint() };

    FloatSize initialAdvance = FloatSize(-15.15625, 18.046875);

    UChar characters[] = { 0x644, 0x637, 0x641, 0x627, 0x64b };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run = ComplexTextController::ComplexTextRun::create(advances, origins, { 193, 377, 447, 431, 458 }, { 4, 3, 2, 1, 0 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, 0, 5, false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    float totalWidth = 0;
    for (size_t i = 1; i < advances.size(); ++i)
        totalWidth += advances[i].width();
    EXPECT_NEAR(controller.totalAdvance().width(), totalWidth, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), advances[4].width(), 0.0001);
    controller.advance(5, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), totalWidth, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), initialAdvance.height(), 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 5U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), advances[4].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), advances[3].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), advances[2].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).width(), advances[1].width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(4).width(), -initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(4).height(), initialAdvance.height(), 0.0001);
}

TEST_F(ComplexTextControllerTest, InitialAdvanceWithLeftRunInLTR)
{
    FontCascadeDescription description;
    description.setOneFamily("LucidaGrande");
    description.setComputedSize(80);
    FontCascade font(WTFMove(description));
    font.update();
    auto spaceWidth = font.primaryFont().spaceWidth();

    Vector<FloatSize> advances = { FloatSize(76.347656, 0.000000), FloatSize(0.000000, 0.000000) };
    Vector<FloatPoint> origins = { FloatPoint(), FloatPoint(-23.281250, -8.398438) };

    FloatSize initialAdvance = FloatSize(28.144531, 0);

    UChar characters[] = { 0x20, 0x61, 0x20e3 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run1 = ComplexTextController::ComplexTextRun::create({ FloatSize(spaceWidth, 0) }, { FloatPoint() }, { 5 }, { 0 }, FloatSize(), font.primaryFont(), characters, 0, charactersLength, 0, 1, true);
    auto run2 = ComplexTextController::ComplexTextRun::create(advances, origins, { 68, 1471 }, { 1, 2 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, 1, 3, true);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run1));
    runs.append(WTFMove(run2));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalAdvance().width(), spaceWidth + 76.347656 + initialAdvance.width(), 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth, 0.0001);
    controller.advance(2, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth + advances[0].width() + initialAdvance.width(), 0.0001);
    controller.advance(3, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), spaceWidth + 76.347656 + initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), 0, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 3U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), spaceWidth + initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), 53.066406, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), 23.281250, 0.0001);
}

TEST_F(ComplexTextControllerTest, InitialAdvanceInLTR)
{
    FontCascadeDescription description;
    description.setOneFamily("LucidaGrande");
    description.setComputedSize(80);
    FontCascade font(WTFMove(description));
    font.update();

    Vector<FloatSize> advances = { FloatSize(76.347656, 0.000000), FloatSize(0.000000, 0.000000) };
    Vector<FloatPoint> origins = { FloatPoint(), FloatPoint(-23.281250, -8.398438) };

    FloatSize initialAdvance = FloatSize(28.144531, 0);

    UChar characters[] = { 0x61, 0x20e3 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run = ComplexTextController::ComplexTextRun::create(advances, origins, { 68, 1471 }, { 0, 1 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, 0, 2, true);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalAdvance().width(), 76.347656 + initialAdvance.width(), 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), advances[0].width() + initialAdvance.width(), 0.0001);
    controller.advance(2, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 76.347656 + initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), initialAdvance.height(), 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 2U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), 53.066406, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), 23.281250, 0.0001);
}

TEST_F(ComplexTextControllerTest, InitialAdvanceInRTLNoOrigins)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(48);
    FontCascade font(WTFMove(description));
    font.update();

    FloatSize initialAdvance = FloatSize(4.33996383363472, 12.368896925859);

    UChar characters[] = { 0x633, 0x20, 0x627, 0x650 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run1 = ComplexTextController::ComplexTextRun::create({ FloatSize(-4.33996383363472, -12.368896925859), FloatSize(14.0397830018083, 0) }, { }, { 884, 240 }, { 3, 2 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, 2, 4, false);
    auto run2 = ComplexTextController::ComplexTextRun::create({ FloatSize(12.0, 0) }, { }, { 3 }, { 1 }, FloatSize(), font.primaryFont(), characters, 0, charactersLength, 1, 2, false);
    auto run3 = ComplexTextController::ComplexTextRun::create({ FloatSize(43.8119349005425, 0) }, { }, { 276 }, { 0 }, FloatSize(), font.primaryFont(), characters, 0, charactersLength, 0, 1, false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run1));
    runs.append(WTFMove(run2));
    runs.append(WTFMove(run3));
    ComplexTextController controller(font, textRun, runs);

    float totalWidth = 14.0397830018083 + 12.0 + 43.8119349005425;
    EXPECT_NEAR(controller.totalAdvance().width(), totalWidth, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 43.8119349005425, 0.0001);
    controller.advance(2, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 43.8119349005425 + 12.0, 0.0001);
    controller.advance(3, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), totalWidth, 0.0001);
    controller.advance(4, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), totalWidth, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), initialAdvance.width(), 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), initialAdvance.height(), 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 4U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), 43.8119349005425, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), 12.0, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), 14.0397830018083, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).width(), -4.33996383363472, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).height(), 12.368896925859, 0.0001);
}

TEST_F(ComplexTextControllerTest, LeadingExpansion)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(48);
    FontCascade font(WTFMove(description));
    font.update();

    UChar characters[] = { 'a' };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength), 0, 100, ForceLeadingExpansion);
    auto run = ComplexTextController::ComplexTextRun::create({ FloatSize(24, 0) }, { }, { 16 }, { 0 }, FloatSize(), font.primaryFont(), characters, 0, charactersLength, 0, 1, true);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    float totalWidth = 100 + 24;
    EXPECT_NEAR(controller.totalAdvance().width(), totalWidth, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), totalWidth, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), 100, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), 0, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 1U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), 24, 0.0001);
}

TEST_F(ComplexTextControllerTest, VerticalAdvances)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(48);
    FontCascade font(WTFMove(description));
    font.update();

    UChar characters[] = { 'a', 'b', 'c', 'd' };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength));
    auto run1 = ComplexTextController::ComplexTextRun::create({ FloatSize(0, 1), FloatSize(0, 2) }, { FloatPoint(0, 4), FloatPoint(0, 8) }, { 16, 17 }, { 0, 1 }, FloatSize(0, 16), font.primaryFont(), characters, 0, charactersLength, 0, 2, true);
    auto run2 = ComplexTextController::ComplexTextRun::create({ FloatSize(0, 32), FloatSize(0, 64) }, { FloatPoint(0, 128), FloatPoint(0, 256) }, { 18, 19 }, { 2, 3 }, FloatSize(0, 512), font.primaryFont(), characters, 0, charactersLength, 2, 4, true);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run1));
    runs.append(WTFMove(run2));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalAdvance().width(), 0, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(0, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(1, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(2, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(3, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(4, &glyphBuffer);
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.initialAdvance().height(), 16, 0.0001);
    EXPECT_EQ(glyphBuffer.size(), 4U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).height(), 4 - 1 -8, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(1).height(), 8 - 2 - 512, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(2).height(), 128 - 32 - 256, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).width(), 0, 0.0001);
    EXPECT_NEAR(glyphBuffer.advanceAt(3).height(), 256 - 64, 0.0001);
}

TEST_F(ComplexTextControllerTest, TotalWidthWithJustification)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(80);
    FontCascade font(WTFMove(description));
    font.update();

    Vector<FloatSize> advances = { FloatSize(1, 0), FloatSize(2, 0), FloatSize(4, 0), FloatSize(8, 0), FloatSize(16, 0) };
    Vector<FloatPoint> origins = { FloatPoint(), FloatPoint(), FloatPoint(), FloatPoint(), FloatPoint() };

    FloatSize initialAdvance = FloatSize();

    UChar characters[] = { 0x644, ' ', 0x644, ' ', 0x644 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength), 0, 14, DefaultExpansion, TextDirection::RTL);
    auto run = ComplexTextController::ComplexTextRun::create(advances, origins, { 5, 6, 7, 8, 9 }, { 4, 3, 2, 1, 0 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, 0, 5, false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalAdvance().width(), 1 + 20 + 7 + 4 + 20 + 7 + 16, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(5, &glyphBuffer);
    EXPECT_EQ(glyphBuffer.size(), 5U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width() + glyphBuffer.advanceAt(1).width() + glyphBuffer.advanceAt(2).width() + glyphBuffer.advanceAt(3).width() + glyphBuffer.advanceAt(4).width(), controller.totalAdvance().width(), 0.0001);
}

}
