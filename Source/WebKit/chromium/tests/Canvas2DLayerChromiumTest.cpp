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

#include "Canvas2DLayerChromium.h"

#include "CCSchedulerTestCommon.h"
#include "FakeCCLayerTreeHostClient.h"
#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "Region.h"
#include "TextureCopier.h"
#include "TextureManager.h"
#include "WebCompositor.h"
#include "WebKit.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCTextureLayerImpl.h"
#include "cc/CCTextureUpdater.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebThread.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace {

class FakeCCLayerTreeHost : public CCLayerTreeHost {
public:
    static PassOwnPtr<FakeCCLayerTreeHost> create()
    {
        OwnPtr<FakeCCLayerTreeHost> host(adoptPtr(new FakeCCLayerTreeHost));
        host->initialize();
        return host.release();
    }

private:
    FakeCCLayerTreeHost()
        : CCLayerTreeHost(&m_client, CCSettings())
    {
    }

    FakeCCLayerTreeHostClient m_client;
};

class MockCanvasContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD0(flush, void(void));
};

class MockTextureAllocator : public TextureAllocator {
public:
    MOCK_METHOD2(createTexture, unsigned(const IntSize&, GC3Denum));
    MOCK_METHOD3(deleteTexture, void(unsigned, const IntSize&, GC3Denum));
};

class MockTextureCopier : public TextureCopier {
public:
    MOCK_METHOD4(copyTexture, void(GraphicsContext3D*, unsigned, unsigned, const IntSize&));
};

class MockTextureUploader : public TextureUploader {
public:
    MOCK_METHOD5(uploadTexture, void(GraphicsContext3D*, LayerTextureUpdater::Texture*, TextureAllocator*, const IntRect, const IntRect));
};

} // namespace

class Canvas2DLayerChromiumTest : public Test {
protected:
    void fullLifecycleTest(bool threaded, bool deferred)
    {
        GraphicsContext3D::Attributes attrs;

        RefPtr<GraphicsContext3D> mainContext = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new MockCanvasContext()), GraphicsContext3D::RenderDirectlyToHostWindow);
        RefPtr<GraphicsContext3D> implContext = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new MockCanvasContext()), GraphicsContext3D::RenderDirectlyToHostWindow);

        MockCanvasContext& mainMock = *static_cast<MockCanvasContext*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(mainContext.get()));
        MockCanvasContext& implMock = *static_cast<MockCanvasContext*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(implContext.get()));

        MockTextureAllocator allocatorMock;
        MockTextureCopier copierMock;
        MockTextureUploader uploaderMock;
        CCTextureUpdater updater;

        const IntSize size(300, 150);

        OwnPtr<WebThread> thread;
        if (threaded)
            thread = adoptPtr(WebKit::Platform::current()->createThread("Canvas2DLayerChromiumTest"));
        WebCompositor::initialize(thread.get());

        OwnPtr<FakeCCLayerTreeHost> layerTreeHost(FakeCCLayerTreeHost::create());
        // Force an update, so that we get a valid TextureManager.
        layerTreeHost->updateLayers(updater);

        const WebGLId backTextureId = 1;
        const WebGLId frontTextureId = 2;
        {
            InSequence sequence;

            // Paint canvas contents on the main thread.
            EXPECT_CALL(mainMock, flush());

            // Note that the canvas backing texture is doublebuffered only when using the threaded
            // compositor and not using deferred canvas rendering
            if (threaded && !deferred) {
                // Create texture and do the copy (on the impl thread).
                EXPECT_CALL(allocatorMock, createTexture(size, GraphicsContext3D::RGBA))
                    .WillOnce(Return(frontTextureId));
                EXPECT_CALL(copierMock, copyTexture(implContext.get(), backTextureId, frontTextureId, size));
                EXPECT_CALL(implMock, flush());

                // Teardown TextureManager.
                EXPECT_CALL(allocatorMock, deleteTexture(frontTextureId, size, GraphicsContext3D::RGBA));
            }
        }

        RefPtr<Canvas2DLayerChromium> canvas = Canvas2DLayerChromium::create(mainContext.get(), size, deferred ? Deferred : NonDeferred);
        canvas->setIsDrawable(true);
        canvas->setLayerTreeHost(layerTreeHost.get());
        canvas->setBounds(IntSize(600, 300));
        canvas->setTextureId(backTextureId);

        canvas->setNeedsDisplay();
        EXPECT_TRUE(canvas->needsDisplay());
        canvas->update(updater, 0);
        EXPECT_FALSE(canvas->needsDisplay());
        {
            DebugScopedSetImplThread scopedImplThread;

            OwnPtr<CCLayerImpl> layerImpl = canvas->createCCLayerImpl();
            EXPECT_EQ(0u, static_cast<CCTextureLayerImpl*>(layerImpl.get())->textureId());

            updater.update(implContext.get(), &allocatorMock, &copierMock, &uploaderMock, 1);
            canvas->pushPropertiesTo(layerImpl.get());

            if (threaded && !deferred)
                EXPECT_EQ(frontTextureId, static_cast<CCTextureLayerImpl*>(layerImpl.get())->textureId());
            else
                EXPECT_EQ(backTextureId, static_cast<CCTextureLayerImpl*>(layerImpl.get())->textureId());
        }
        canvas.clear();
        layerTreeHost->contentsTextureManager()->reduceMemoryToLimit(0);
        layerTreeHost->contentsTextureManager()->deleteEvictedTextures(&allocatorMock);
        layerTreeHost.clear();
        WebCompositor::shutdown();
    }
};

namespace {

TEST_F(Canvas2DLayerChromiumTest, testFullLifecycleSingleThread)
{
    fullLifecycleTest(false, false);
}

TEST_F(Canvas2DLayerChromiumTest, testFullLifecycleThreaded)
{
    fullLifecycleTest(true, false);
}

TEST_F(Canvas2DLayerChromiumTest, testFullLifecycleSingleThreadDeferred)
{
    fullLifecycleTest(false, true);
}

TEST_F(Canvas2DLayerChromiumTest, testFullLifecycleThreadedDeferred)
{
    fullLifecycleTest(true, true);
}

} // namespace
