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

#include "CCDrawQuad.h"
#include "CCPrioritizedTextureManager.h"
#include "CCSettings.h"
#include "CCSingleThreadProxy.h"
#include "CCTestCommon.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebCompositor.h>

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
    void setMemoryAllocation(WebGraphicsMemoryAllocation allocation)
    {
        ASSERT(CCProxy::isImplThread());
        // In single threaded mode we expect this callback on main thread.
        DebugScopedSetMainThread main;
        m_memoryAllocationChangedCallback->onMemoryAllocationChanged(allocation);
    }

private:
    int m_frame;
    WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* m_memoryAllocationChangedCallback;
};

class FakeCCRendererClient : public CCRendererClient {
public:
    FakeCCRendererClient()
        : m_setFullRootLayerDamageCount(0)
        , m_rootLayer(CCLayerImpl::create(1))
        , m_memoryAllocationLimitBytes(CCPrioritizedTextureManager::defaultMemoryAllocationLimit())
    {
        m_rootLayer->createRenderSurface();
        OwnPtr<CCRenderPass> rootRenderPass = CCRenderPass::create(m_rootLayer->renderSurface(), m_rootLayer->id());
        m_renderPassesInDrawOrder.append(rootRenderPass.get());
        m_renderPasses.set(m_rootLayer->id(), rootRenderPass.release());
    }

    // CCRendererClient methods.
    virtual const IntSize& deviceViewportSize() const OVERRIDE { static IntSize fakeSize(1, 1); return fakeSize; }
    virtual const CCLayerTreeSettings& settings() const OVERRIDE { static CCLayerTreeSettings fakeSettings; return fakeSettings; }
    virtual void didLoseContext() OVERRIDE { }
    virtual void onSwapBuffersComplete() OVERRIDE { }
    virtual void setFullRootLayerDamage() OVERRIDE { m_setFullRootLayerDamageCount++; }
    virtual void releaseContentsTextures() OVERRIDE { }
    virtual void setMemoryAllocationLimitBytes(size_t bytes) OVERRIDE { m_memoryAllocationLimitBytes = bytes; }

    // Methods added for test.
    int setFullRootLayerDamageCount() const { return m_setFullRootLayerDamageCount; }

    CCRenderPass* rootRenderPass() { return m_renderPassesInDrawOrder.last(); }
    const CCRenderPassList& renderPassesInDrawOrder() const { return m_renderPassesInDrawOrder; }
    const CCRenderPassIdHashMap& renderPasses() const { return m_renderPasses; }

    size_t memoryAllocationLimitBytes() const { return m_memoryAllocationLimitBytes; }

private:
    int m_setFullRootLayerDamageCount;
    DebugScopedSetImplThread m_implThread;
    OwnPtr<CCLayerImpl> m_rootLayer;
    CCRenderPassList m_renderPassesInDrawOrder;
    CCRenderPassIdHashMap m_renderPasses;
    size_t m_memoryAllocationLimitBytes;
};

class FakeLayerRendererChromium : public LayerRendererChromium {
public:
    FakeLayerRendererChromium(CCRendererClient* client, CCResourceProvider* resourceProvider) : LayerRendererChromium(client, resourceProvider, UnthrottledUploader) { }

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
        , m_context(FakeWebCompositorOutputSurface::create(adoptPtr(new FrameCountingMemoryAllocationSettingContext())))
        , m_resourceProvider(CCResourceProvider::create(m_context.get()))
        , m_layerRendererChromium(&m_mockClient, m_resourceProvider.get())
    {
    }

    virtual void SetUp()
    {
        WebKit::WebCompositor::initialize(0);
        m_layerRendererChromium.initialize();
    }

    virtual void TearDown()
    {
        WebKit::WebCompositor::shutdown();
    }

    void swapBuffers()
    {
        m_layerRendererChromium.swapBuffers();
    }

    FrameCountingMemoryAllocationSettingContext* context() { return static_cast<FrameCountingMemoryAllocationSettingContext*>(m_context->context3D()); }

    WebGraphicsMemoryAllocation m_suggestHaveBackbufferYes;
    WebGraphicsMemoryAllocation m_suggestHaveBackbufferNo;

    OwnPtr<CCGraphicsContext> m_context;
    FakeCCRendererClient m_mockClient;
    OwnPtr<CCResourceProvider> m_resourceProvider;
    FakeLayerRendererChromium m_layerRendererChromium;
    CCScopedSettings m_scopedSettings;
};

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest recreating framebuffer when one already exists.
// Expected: it does nothing.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferYesWhenItAlreadyExistsShouldDoNothing)
{
    context()->setMemoryAllocation(m_suggestHaveBackbufferYes);
    EXPECT_EQ(0, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, context()->frameCount());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is not visible.
// Expected: it is discarded and damage tracker is reset.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferNoShouldDiscardBackbufferAndDamageRootLayerWhileNotVisible)
{
    m_layerRendererChromium.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is visible.
// Expected: the allocation is ignored.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferNoDoNothingWhenVisible)
{
    m_layerRendererChromium.setVisible(true);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(0, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());
}


// Test LayerRendererChromium discardFramebuffer functionality:
// Suggest discarding framebuffer when one does not exist.
// Expected: it does nothing.
TEST_F(LayerRendererChromiumTest, SuggestBackbufferNoWhenItDoesntExistShouldDoNothing)
{
    m_layerRendererChromium.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());

    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
}

// Test LayerRendererChromium discardFramebuffer functionality:
// Begin drawing a frame while a framebuffer is discarded.
// Expected: will recreate framebuffer.
TEST_F(LayerRendererChromiumTest, DiscardedBackbufferIsRecreatedForScopeDuration)
{
    m_layerRendererChromium.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());

    m_layerRendererChromium.setVisible(true);
    m_layerRendererChromium.drawFrame(m_mockClient.renderPassesInDrawOrder(), m_mockClient.renderPasses());
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, context()->frameCount());
}

TEST_F(LayerRendererChromiumTest, FramebufferDiscardedAfterReadbackWhenNotVisible)
{
    m_layerRendererChromium.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());

    char pixels[4];
    m_layerRendererChromium.drawFrame(m_mockClient.renderPassesInDrawOrder(), m_mockClient.renderPasses());
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());

    m_layerRendererChromium.getFramebufferPixels(pixels, IntRect(0, 0, 1, 1));
    EXPECT_TRUE(m_layerRendererChromium.isFramebufferDiscarded());
    EXPECT_EQ(2, m_mockClient.setFullRootLayerDamageCount());
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
    CCScopedSettings scopedSettings;
    FakeCCRendererClient mockClient;
    OwnPtr<CCGraphicsContext> context(FakeWebCompositorOutputSurface::create(adoptPtr(new ForbidSynchronousCallContext)));
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    FakeLayerRendererChromium layerRendererChromium(&mockClient, resourceProvider.get());

    EXPECT_TRUE(layerRendererChromium.initialize());
}

class LoseContextOnFirstGetContext : public FakeWebGraphicsContext3D {
public:
    LoseContextOnFirstGetContext()
        : m_contextLost(false)
    {
    }

    virtual bool makeContextCurrent() OVERRIDE
    {
        return !m_contextLost;
    }

    virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value) OVERRIDE
    {
        m_contextLost = true;
        *value = 0;
    }

    virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value) OVERRIDE
    {
        m_contextLost = true;
        *value = 0;
    }

    virtual WGC3Denum getGraphicsResetStatusARB() OVERRIDE
    {
        return m_contextLost ? 1 : 0;
    }

private:
    bool m_contextLost;
};

TEST(LayerRendererChromiumTest2, initializationWithQuicklyLostContextDoesNotAssert)
{
    CCScopedSettings scopedSettings;
    FakeCCRendererClient mockClient;
    OwnPtr<CCGraphicsContext> context(FakeWebCompositorOutputSurface::create(adoptPtr(new LoseContextOnFirstGetContext)));
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    FakeLayerRendererChromium layerRendererChromium(&mockClient, resourceProvider.get());

    layerRendererChromium.initialize();
}

class ContextThatDoesNotSupportMemoryManagmentExtensions : public FakeWebGraphicsContext3D {
public:
    ContextThatDoesNotSupportMemoryManagmentExtensions() { }

    // WebGraphicsContext3D methods.

    // This method would normally do a glSwapBuffers under the hood.
    virtual void prepareTexture() { }
    virtual void setMemoryAllocationChangedCallbackCHROMIUM(WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) { }
    virtual WebString getString(WebKit::WGC3Denum name) { return WebString(); }
};

TEST(LayerRendererChromiumTest2, initializationWithoutGpuMemoryManagerExtensionSupportShouldDefaultToNonZeroAllocation)
{
    FakeCCRendererClient mockClient;
    OwnPtr<CCGraphicsContext> context(FakeWebCompositorOutputSurface::create(adoptPtr(new ContextThatDoesNotSupportMemoryManagmentExtensions)));
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    FakeLayerRendererChromium layerRendererChromium(&mockClient, resourceProvider.get());

    layerRendererChromium.initialize();

    EXPECT_GT(mockClient.memoryAllocationLimitBytes(), 0ul);
}

class ClearCountingContext : public FakeWebGraphicsContext3D {
public:
    ClearCountingContext() : m_clear(0) { }

    virtual void clear(WGC3Dbitfield)
    {
        m_clear++;
    }

    int clearCount() const { return m_clear; }

private:
    int m_clear;
};

TEST(LayerRendererChromiumTest2, opaqueBackground)
{
    FakeCCRendererClient mockClient;
    OwnPtr<CCGraphicsContext> ccContext(FakeWebCompositorOutputSurface::create(adoptPtr(new ClearCountingContext)));
    ClearCountingContext* context = static_cast<ClearCountingContext*>(ccContext->context3D());
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(ccContext.get()));
    FakeLayerRendererChromium layerRendererChromium(&mockClient, resourceProvider.get());

    mockClient.rootRenderPass()->setHasTransparentBackground(false);

    EXPECT_TRUE(layerRendererChromium.initialize());

    layerRendererChromium.drawFrame(mockClient.renderPassesInDrawOrder(), mockClient.renderPasses());

    // On DEBUG builds, render passes with opaque background clear to blue to
    // easily see regions that were not drawn on the screen.
#if defined(NDEBUG)
    EXPECT_EQ(0, context->clearCount());
#else
    EXPECT_EQ(1, context->clearCount());
#endif
}

TEST(LayerRendererChromiumTest2, transparentBackground)
{
    FakeCCRendererClient mockClient;
    OwnPtr<CCGraphicsContext> ccContext(FakeWebCompositorOutputSurface::create(adoptPtr(new ClearCountingContext)));
    ClearCountingContext* context = static_cast<ClearCountingContext*>(ccContext->context3D());
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(ccContext.get()));
    FakeLayerRendererChromium layerRendererChromium(&mockClient, resourceProvider.get());

    mockClient.rootRenderPass()->setHasTransparentBackground(true);

    EXPECT_TRUE(layerRendererChromium.initialize());

    layerRendererChromium.drawFrame(mockClient.renderPassesInDrawOrder(), mockClient.renderPasses());

    EXPECT_EQ(1, context->clearCount());
}
