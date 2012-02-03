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

#include "GraphicsContext3D.h"

#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

class FrameCountingContext : public FakeWebGraphicsContext3D {
public:
    FrameCountingContext() : m_frame(0) { }

    // This method would normally do a glSwapBuffers under the hood.
    virtual void prepareTexture() { m_frame++; }

    int frameCount() { return m_frame; }

private:
    int m_frame;
};

TEST(FakeGraphicsContext3DTest, CanOverrideManually)
{
    GraphicsContext3D::Attributes attrs;
    RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new FrameCountingContext()), attrs, 0, GraphicsContext3D::RenderDirectlyToHostWindow, GraphicsContext3DPrivate::ForUseOnThisThread);
    FrameCountingContext& mockContext = *static_cast<FrameCountingContext*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(context.get()));

    for (int i = 0; i < 10; i++) {
        context->clearColor(0, 0, 0, 1);
        context->prepareTexture();
    }
    context->finish();

    EXPECT_EQ(10, mockContext.frameCount());
}


class GMockContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD0(getError, WGC3Denum());
};

TEST(FakeGraphicsContext3DTest, CanUseGMock)
{
    GraphicsContext3D::Attributes attrs;
    RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new GMockContext()), attrs, 0, GraphicsContext3D::RenderDirectlyToHostWindow, GraphicsContext3DPrivate::ForUseOnThisThread);
    GMockContext& mockContext = *static_cast<GMockContext*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(context.get()));

    EXPECT_CALL(mockContext, getError())
            .WillRepeatedly(testing::Return(314));

    // It's OK to call methods GMock doesn't know about.
    context->makeContextCurrent();

    // Check that the mocked method is returning as intended.
    for (int i = 0; i < 10; i++)
        EXPECT_EQ((int)context->getError(), 314);
}

class ContextThatCountsMakeCurrents : public FakeWebGraphicsContext3D {
public:
    ContextThatCountsMakeCurrents() : m_makeCurrentCount(0) { }
    virtual bool makeContextCurrent()
    {
        m_makeCurrentCount++;
        return true;
    }
    int makeCurrentCount() { return m_makeCurrentCount; }
private:
    int m_makeCurrentCount;
};


TEST(FakeGraphicsContext3DTest, ContextForThisThreadShouldNotMakeCurrent)
{
    GraphicsContext3D::Attributes attrs;
    RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new ContextThatCountsMakeCurrents()), attrs, 0, GraphicsContext3D::RenderDirectlyToHostWindow, GraphicsContext3DPrivate::ForUseOnThisThread);
    EXPECT_TRUE(context);
    ContextThatCountsMakeCurrents& mockContext = *static_cast<ContextThatCountsMakeCurrents*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(context.get()));
    EXPECT_EQ(0, mockContext.makeCurrentCount());
}

TEST(FakeGraphicsContext3DTest, ContextForAnotherThreadShouldNotMakeCurrent)
{
    GraphicsContext3D::Attributes attrs;
    RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new ContextThatCountsMakeCurrents()), attrs, 0, GraphicsContext3D::RenderDirectlyToHostWindow, GraphicsContext3DPrivate::ForUseOnAnotherThread);
    EXPECT_TRUE(context);
    ContextThatCountsMakeCurrents& mockContext = *static_cast<ContextThatCountsMakeCurrents*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(context.get()));
    EXPECT_EQ(0, mockContext.makeCurrentCount());
}

