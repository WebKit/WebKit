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

#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "cc/CCSingleThreadProxy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

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
    virtual const IntSize& viewportSize() const { static IntSize fakeSize; return fakeSize; }
    virtual const CCSettings& settings() const { static CCSettings fakeSettings; return fakeSettings; }
    virtual CCLayerImpl* rootLayer() { return m_rootLayer.get(); }
    virtual const CCLayerImpl* rootLayer() const { return m_rootLayer.get(); }
    virtual void didLoseContext() { }
    virtual void onSwapBuffersComplete() { }
    virtual void setFullRootLayerDamage() { m_setFullRootLayerDamageCount++; }

    // Methods added for test.
    int setFullRootLayerDamageCount() const { return m_setFullRootLayerDamageCount; }

private:
    int m_setFullRootLayerDamageCount;
    DebugScopedSetImplThread m_implThread;
    OwnPtr<CCLayerImpl> m_rootLayer;
};

class FakeLayerRendererChromium : public LayerRendererChromium {
public:
    FakeLayerRendererChromium(LayerRendererChromiumClient* client, PassRefPtr<GraphicsContext3D> context) : LayerRendererChromium(client, context) { }

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
        , m_layerRendererChromium(&m_mockClient, m_context.release())
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

    m_layerRendererChromium.beginDrawingFrame();
    EXPECT_FALSE(m_layerRendererChromium.isFramebufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, m_mockContext.frameCount());
}
