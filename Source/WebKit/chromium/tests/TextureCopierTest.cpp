/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TextureCopier.h"

#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;
using testing::InSequence;
using testing::Test;
using testing::_;

namespace {

class MockContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD2(bindFramebuffer, void(WGC3Denum, WebGLId));
    MOCK_METHOD3(texParameteri, void(GC3Denum target, GC3Denum pname, GC3Dint param));

    MOCK_METHOD3(drawArrays, void(GC3Denum mode, GC3Dint first, GC3Dsizei count));
};

TEST(TextureCopierTest, testDrawArraysCopy)
{
    GraphicsContext3D::Attributes attrs;
    RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new MockContext()), GraphicsContext3D::RenderDirectlyToHostWindow);
    MockContext& mockContext = *static_cast<MockContext*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(context.get()));

    {
        InSequence sequence;

        // Here we check just some essential properties of copyTexture() to avoid mirroring the full implementation.
        EXPECT_CALL(mockContext, bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, _));

        // Make sure linear filtering is disabled during the copy.
        EXPECT_CALL(mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST));
        EXPECT_CALL(mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST));

        EXPECT_CALL(mockContext, drawArrays(_, _, _));

        // Linear filtering should be restored.
        EXPECT_CALL(mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        EXPECT_CALL(mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));

        // Default framebuffer should be restored
        EXPECT_CALL(mockContext, bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
    }

    int sourceTextureId = 1;
    int destTextureId = 2;
    IntSize size(256, 128);
    OwnPtr<AcceleratedTextureCopier> copier(AcceleratedTextureCopier::create(context));
    copier->copyTexture(context.get(), sourceTextureId, destTextureId, size);
}

} // namespace
