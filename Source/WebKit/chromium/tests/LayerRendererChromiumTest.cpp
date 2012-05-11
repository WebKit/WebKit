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
#include "LayerRendererChromium.h"

#include "CCTiledLayerTestCommon.h"
#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "cc/CCSingleThreadProxy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;

class FrameCountingMemoryAllocationSettingContext : public FakeWebGraphicsContext3D {
public:
    FrameCountingMemoryAllocationSettingContext() : m_frame(0) { }

    // WebGraphicsContext3D methods.

    // This method would normally do a glSwapBuffers under the hood.
    virtual void prepareTexture() { m_frame++; }
    virtual void setMemoryAllocationChangedCallbackCHROMIUM(WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) { m_memoryAllocationChangedCallback = callback; }
    virtual WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GraphicsContext3D::EXTENSIONS)
            return WebString("GL_CHROMIUM_set_visibility GL_CHROMIUM_gpu_memory_manager GL_CHROMIUM_discard_framebuffer");
        return WebString();
    }

    // Methods added for test.
    int frameCount() { return m_frame; }
    void setMemoryAllocation(WebGraphicsMemoryAllocation allocation) { m_memoryAllocationChangedCallback->onMemoryAllocationChanged(allocation); }

private:
    int m_frame;
    WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* m_memoryAllocationChangedCallback;
};

class FakeLayerRendererChromiumClient : public LayerRendererChromiumClient {
public:
    FakeLayerRendererChromiumClient()
        : m_setFullRootLayerDamageCount(0)
        , m_rootLayer(CCLayerImpl::create(1))
    {
        m_rootLayer->createRenderSurface();
    }

    // LayerRendererChromiumClient methods.
    virtual const IntSize& viewportSize() const OVERRIDE { static IntSize fakeSize; return fakeSize; }
    virtual const CCSettings& settings() const OVERRIDE { static CCSettings fakeSettings; return fakeSettings; }
    virtual void didLoseContext() OVERRIDE { }
    virtual void onSwapBuffersComplete() OVERRIDE { }
    virtual void setFullRootLayerDamage() OVERRIDE { m_setFullRootLayerDamageCount++; }
    virtual void setContentsMemoryAllocationLimitBytes(size_t) OVERRIDE { }

    // Methods added for test.
    int setFullRootLayerDamageCount() const { return m_setFullRootLayerDamageCount; }

    CCLayerImpl* rootLayer() { return m_rootLayer.get(); }

private:
    int m_setFullRootLayerDamageCount;
    DebugScopedSetImplThread m_implThread;
    OwnPtr<CCLayerImpl> m_rootLayer;
};

class FakeLayerRendererChromium : public LayerRendererChromium {
public:
    FakeLayerRendererChromium(LayerRendererChromiumClient* client, PassRefPtr<GraphicsContext3D> context, PassOwnPtr<TextureUploader> uploader) : LayerRendererChromium(client, context, uploader) { }

    // LayerRendererChromium methods.

    // Changing visibility to public.
    using LayerRendererChromium::initialize;
    using LayerRendererChromium::isFramebufferDiscarded;
};

class LayerRendererChromiumTest : public testing::Test {
protected:
    LayerRendererChromiumTest()
        : m_suggestHaveBackbufferYes(1, true)
        , m_suggestHaveBackbufferNo(1, false)
        , m_context(GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new FrameCountingMemoryAllocationSettingContext()), GraphicsContext3D::RenderDirectlyToHostWindow))
        , m_mockContext(*static_cast<FrameCountingMemoryAllocationSettingContext*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(m_context.get())))
        , m_layerRendererChromium(&m_mockClient, m_context.release(), adoptPtr(new FakeTextureUploader()))
    {
    }

    virtual void SetUp()
    {
        m_layerRendererChromium.initialize();
    }

    void swapBuffers()
    {
        m_layerRendererChromium.swapBuffers(IntRect());
    }

    WebGraphicsMemoryAllocation m_suggestHaveBackbufferYes;
    WebGraphicsMemoryAllocation m_suggestHaveBackbufferNo;

    RefPtr<GraphicsContext3D> m_context;
    FrameCountingMemoryAllocationSettingContext& m_mockContext;
    FakeLayerRendererChromiumClient m_mockClient;
    FakeLayerRendererChromium m_layerRendererChromium;
};

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest recreating framebuffer when one already exists.
// Expected: it does nothing.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferYesWhenItAlreadyExistsShouldDoNothing)
{
    m_mockContext.setMemoryAllocation(m_suggestHaveBackbufferYes);
    EXPECT_EQ(0, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, m_mockContext.frameCount());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest discarding framebuffer when one exists.
// Expected: it is discarded and damage tracker is reset.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferNoShouldDiscardBackbufferAndDamageRootLayer)
{
    m_mockContext.setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest discarding framebuffer when one does not exist.
// Expected: it does nothing.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferNoWhenItDoesntExistShouldDoNothing)
{
    m_mockContext.setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());

    m_mockContext.setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest discarding framebuffer, then try to swapBuffers.
// Expected: framebuffer is discarded, swaps are ignored, and damage is reset after discard and after each swap.
TEST_F(LayerRendererChromiumTest, SwapBuffersWhileBackbufferDiscardedShouldIgnoreSwapAndDamageRootLayer)
{
    m_mockContext.setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());

    swapBuffers();
    EXPECT_EQ(0, m_mockContext.frameCount());
    EXPECT_EQ(2, m_mockClient.setFullRootLayerDamageCount());
    
    swapBuffers();
    EXPECT_EQ(0, m_mockContext.frameCount());
    EXPECT_EQ(3, m_mockClient.setFullRootLayerDamageCount());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Begin drawing a frame while a framebuffer is discarded.
// Expected: will recreate framebuffer.
TEST_F(LayerRendererChromiumTest, DiscardedBackbufferIsRecreatredForScopeDuration)
{
    m_mockContext.setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());

    m_layerRendererChromium.beginDrawingFrame(m_mockClient.rootLayer()->renderSurface());
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, m_mockContext.frameCount());
}

class ForbidSynchronousCallContext : public FakeWebGraphicsContext3D {
public:
    ForbidSynchronousCallContext() { }

    virtual bool getActiveAttrib(WebGLId program, WGC3Duint index, ActiveInfo&) { ADD_FAILURE(); return false; }
    virtual bool getActiveUniform(WebGLId program, WGC3Duint index, ActiveInfo&) { ADD_FAILURE(); return false; }
    virtual void getAttachedShaders(WebGLId program, WGC3Dsizei maxCount, WGC3Dsizei* count, WebGLId* shaders) { ADD_FAILURE(); }
    virtual WGC3Dint getAttribLocation(WebGLId program, const WGC3Dchar* name) { ADD_FAILURE(); return 0; }
    virtual void getBooleanv(WGC3Denum pname, WGC3Dboolean* value) { ADD_FAILURE(); }
    virtual void getBufferParameteriv(WGC3Denum target, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual Attributes getContextAttributes() { ADD_FAILURE(); return m_attrs; }
    virtual WGC3Denum getError() { ADD_FAILURE(); return 0; }
    virtual void getFloatv(WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getFramebufferAttachmentParameteriv(WGC3Denum target, WGC3Denum attachment, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual void getIntegerv(WGC3Denum pname, WGC3Dint* value)
    {
        if (pname == WebCore::GraphicsContext3D::MAX_TEXTURE_SIZE)
            *value = 1024; // MAX_TEXTURE_SIZE is cached client side, so it's OK to query.
        else
            ADD_FAILURE();
    }

    // We allow querying the shader compilation and program link status in debug mode, but not release.
    virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value)
    {
#ifndef NDEBUG
        *value = 1;
#else
        ADD_FAILURE();
#endif
    }

    virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value)
    {
#ifndef NDEBUG
        *value = 1;
#else
        ADD_FAILURE();
#endif
    }

    virtual WebString getString(WGC3Denum name)
    {
        // We allow querying the extension string.
        // FIXME: It'd be better to check that we only do this before starting any other expensive work (like starting a compilation)
        if (name != WebCore::GraphicsContext3D::EXTENSIONS)
            ADD_FAILURE();
        return WebString();
    }

    virtual WebString getProgramInfoLog(WebGLId program) { ADD_FAILURE(); return WebString(); }
    virtual void getRenderbufferParameteriv(WGC3Denum target, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }

    virtual WebString getShaderInfoLog(WebGLId shader) { ADD_FAILURE(); return WebString(); }
    virtual void getShaderPrecisionFormat(WGC3Denum shadertype, WGC3Denum precisiontype, WGC3Dint* range, WGC3Dint* precision) { ADD_FAILURE(); }
    virtual WebString getShaderSource(WebGLId shader) { ADD_FAILURE(); return WebString(); }
    virtual void getTexParameterfv(WGC3Denum target, WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getTexParameteriv(WGC3Denum target, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual void getUniformfv(WebGLId program, WGC3Dint location, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getUniformiv(WebGLId program, WGC3Dint location, WGC3Dint* value) { ADD_FAILURE(); }
    virtual WGC3Dint getUniformLocation(WebGLId program, const WGC3Dchar* name) { ADD_FAILURE(); return 0; }
    virtual void getVertexAttribfv(WGC3Duint index, WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getVertexAttribiv(WGC3Duint index, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual WGC3Dsizeiptr getVertexAttribOffset(WGC3Duint index, WGC3Denum pname) { ADD_FAILURE(); return 0; }
};

// This test isn't using the same fixture as LayerRendererChromiumTest, and you can't mix TEST() and TEST_F() with the same name, hence LRC2.
TEST(LayerRendererChromiumTest2, initializationDoesNotMakeSynchronousCalls)
{
    FakeLayerRendererChromiumClient mockClient;
    FakeLayerRendererChromium layerRendererChromium(&mockClient, GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new ForbidSynchronousCallContext), GraphicsContext3D::RenderDirectlyToHostWindow), adoptPtr(new FakeTextureUploader()));

    EXPECT_TRUE(layerRendererChromium.initialize());
}
