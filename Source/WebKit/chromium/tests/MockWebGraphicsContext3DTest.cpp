/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "MockWebGraphicsContext3D.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebKit;
using namespace testing;

TEST(MockWebGraphicsContext3DTest, BasicMockDoesNothing)
{
    MockWebGraphicsContext3D context;
    EXPECT_FALSE(context.makeContextCurrent());
}

class FrameCountingContext : public MockWebGraphicsContext3D {
public:
    FrameCountingContext() : m_frame(0) { }

    // This method would normally do a glSwapBuffers under the hood.
    virtual void prepareTexture() { m_frame++; }

    int frameCount() { return m_frame; }

private:
    int m_frame;
};

TEST(MockWebGraphicsContext3DTest, CanOverrideManually)
{
    FrameCountingContext context;

    context.makeContextCurrent();
    for (int i = 0; i < 10; i++) {
        context.clearColor(0, 0, 0, 1);
        context.prepareTexture();
    }
    context.finish();

    EXPECT_EQ(10, context.frameCount());
}

class GMockContext : public MockWebGraphicsContext3D {
public:
    MOCK_METHOD0(width, int());
    MOCK_METHOD0(height, int());
    MOCK_METHOD1(synthesizeGLError, void(WGC3Denum error));
    MOCK_METHOD0(getError, WGC3Denum());
};

TEST(MockWebGraphicsContext3DTest, CanUseGMock)
{
    GMockContext context;

    EXPECT_CALL(context, width())
        .WillRepeatedly(Return(512));

    EXPECT_CALL(context, height())
        .WillRepeatedly(Return(384));

    EXPECT_CALL(context, synthesizeGLError(_)) // Any parameter accepted
        .Times(Exactly(10));

    // No expectation for getError(), so calling that would make our test fail.

    for (int i = 0; i < 10; i++) {
        context.synthesizeGLError(i);
        EXPECT_EQ(512, context.width());
        EXPECT_EQ(384, context.height());

        // It's OK to call methods GMock doesn't know about.
        EXPECT_FALSE(context.makeContextCurrent());
    }
}
