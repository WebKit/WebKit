/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "DrawingBuffer.h"

#include "CompositorFakeGraphicsContext3D.h"
#include "LayerChromium.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

namespace {

TEST(DrawingBufferChromiumTest, opaqueFormats)
{
    RefPtr<DrawingBuffer> buffer;

    GraphicsContext3D::Attributes alphaAttrs;
    alphaAttrs.alpha = true;
    GraphicsContext3D::Attributes opaqueAttrs;
    opaqueAttrs.alpha = false;

    RefPtr<GraphicsContext3D> alphaContext = createCompositorMockGraphicsContext3D(alphaAttrs);
    EXPECT_TRUE(alphaContext);
    RefPtr<GraphicsContext3D> opaqueContext = createCompositorMockGraphicsContext3D(opaqueAttrs);
    EXPECT_TRUE(opaqueContext);

    buffer = DrawingBuffer::create(alphaContext.get(), IntSize(), DrawingBuffer::Preserve, DrawingBuffer::Alpha);
    EXPECT_FALSE(buffer->platformLayer()->opaque());
    buffer = DrawingBuffer::create(alphaContext.get(), IntSize(), DrawingBuffer::Discard, DrawingBuffer::Alpha);
    EXPECT_FALSE(buffer->platformLayer()->opaque());

    buffer = DrawingBuffer::create(opaqueContext.get(), IntSize(), DrawingBuffer::Preserve, DrawingBuffer::Opaque);
    EXPECT_TRUE(buffer->platformLayer()->opaque());
    buffer = DrawingBuffer::create(opaqueContext.get(), IntSize(), DrawingBuffer::Discard, DrawingBuffer::Opaque);
    EXPECT_TRUE(buffer->platformLayer()->opaque());
}

} // namespace
