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

TEST_F(ComplexTextControllerTest, TotalWidthWithJustification)
{
    FontCascadeDescription description;
    description.setOneFamily("Times");
    description.setComputedSize(80);
    FontCascade font(description);
    font.update(nullptr);

    Vector<CGSize> advances = { CGSizeMake(1, 0), CGSizeMake(2, 0), CGSizeMake(4, 0), CGSizeMake(8, 0), CGSizeMake(16, 0) };
#if USE_LAYOUT_SPECIFIC_ADVANCES
    Vector<CGPoint> origins = { CGPointZero, CGPointZero, CGPointZero, CGPointZero, CGPointZero };
#else
    Vector<CGPoint> origins = { };
#endif

    CGSize initialAdvance = CGSizeZero;

    UChar characters[] = { 0x644, ' ', 0x644, ' ', 0x644 };
    size_t charactersLength = WTF_ARRAY_LENGTH(characters);
    TextRun textRun(StringView(characters, charactersLength), 0, 14, DefaultExpansion, RTL);
    auto run = ComplexTextController::ComplexTextRun::createForTesting(advances, origins, { 5, 6, 7, 8, 9 }, { 4, 3, 2, 1, 0 }, initialAdvance, font.primaryFont(), characters, 0, charactersLength, CFRangeMake(0, 5), false);
    Vector<Ref<ComplexTextController::ComplexTextRun>> runs;
    runs.append(WTFMove(run));
    ComplexTextController controller(font, textRun, runs);

    EXPECT_NEAR(controller.totalWidth(), 1 + 20 + 7 + 4 + 20 + 7 + 16, 0.0001);
    GlyphBuffer glyphBuffer;
    EXPECT_NEAR(controller.runWidthSoFar(), 0, 0.0001);
    controller.advance(5, &glyphBuffer);
    EXPECT_EQ(glyphBuffer.size(), 5U);
    EXPECT_NEAR(glyphBuffer.advanceAt(0).width() + glyphBuffer.advanceAt(1).width() + glyphBuffer.advanceAt(2).width() + glyphBuffer.advanceAt(3).width() + glyphBuffer.advanceAt(4).width(), controller.totalWidth(), 0.0001);
}

}
