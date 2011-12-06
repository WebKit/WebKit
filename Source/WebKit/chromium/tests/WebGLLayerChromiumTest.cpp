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

#include "WebGLLayerChromium.h"

#include "DrawingBuffer.h"
#include "GraphicsContext3DPrivate.h"
#include "MockWebGraphicsContext3D.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

namespace {

// Test stub for WebGraphicsContext3D. Returns canned values needed for compositor initialization.
class CompositorMockWebGraphicsContext3D : public MockWebGraphicsContext3D {
public:
    static PassOwnPtr<CompositorMockWebGraphicsContext3D> create(GraphicsContext3D::Attributes attrs) { return adoptPtr(new CompositorMockWebGraphicsContext3D(attrs)); }
    virtual bool makeContextCurrent() { return true; }
    virtual WebGLId createProgram() { return 1; }
    virtual WebGLId createShader(WGC3Denum) { return 1; }
    virtual void getShaderiv(WebGLId, WGC3Denum, WGC3Dint* value) { *value = 1; }
    virtual void getProgramiv(WebGLId, WGC3Denum, WGC3Dint* value) { *value = 1; }
    virtual WebGraphicsContext3D::Attributes getContextAttributes() { return m_attrs; }

private:
    CompositorMockWebGraphicsContext3D(GraphicsContext3D::Attributes attrs) { m_attrs.alpha = attrs.alpha; }

    WebGraphicsContext3D::Attributes m_attrs;
};

static PassRefPtr<GraphicsContext3D> createGraphicsContext(GraphicsContext3D::Attributes attrs)
{
    OwnPtr<WebGraphicsContext3D> webContext = CompositorMockWebGraphicsContext3D::create(attrs);
    return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(
        webContext.release(), attrs, 0,
        GraphicsContext3D::RenderDirectlyToHostWindow,
        GraphicsContext3DPrivate::ForUseOnAnotherThread);
}

TEST(WebGLLayerChromiumTest, opaqueFormats)
{
    RefPtr<DrawingBuffer> buffer;

    GraphicsContext3D::Attributes alphaAttrs;
    alphaAttrs.alpha = true;
    GraphicsContext3D::Attributes opaqueAttrs;
    opaqueAttrs.alpha = false;

    RefPtr<GraphicsContext3D> alphaContext = createGraphicsContext(alphaAttrs);
    EXPECT_TRUE(alphaContext);
    RefPtr<GraphicsContext3D> opaqueContext = createGraphicsContext(opaqueAttrs);
    EXPECT_TRUE(opaqueContext);

    buffer = DrawingBuffer::create(alphaContext.get(), IntSize(), false);
    EXPECT_FALSE(buffer->platformLayer()->opaque());
    buffer = DrawingBuffer::create(alphaContext.get(), IntSize(), true);
    EXPECT_FALSE(buffer->platformLayer()->opaque());

    buffer = DrawingBuffer::create(opaqueContext.get(), IntSize(), false);
    EXPECT_TRUE(buffer->platformLayer()->opaque());
    buffer = DrawingBuffer::create(opaqueContext.get(), IntSize(), true);
    EXPECT_TRUE(buffer->platformLayer()->opaque());
}

} // namespace
