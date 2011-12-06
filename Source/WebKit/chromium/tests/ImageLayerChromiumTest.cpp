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

#include "ImageLayerChromium.h"

#include "GraphicsLayer.h"
#include "GraphicsLayerChromium.h"
#include "NativeImageSkia.h"
#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
  public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) { }
    virtual void notifySyncRequired(const GraphicsLayer*) { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) { }
    virtual bool showDebugBorders() const { return false; }
    virtual bool showRepaintCounter() const { return false; }
};

class TestImage : public Image {
public:

    static PassRefPtr<TestImage> create(const IntSize& size, bool opaque)
    {
        return adoptRef(new TestImage(size, opaque));
    }

    explicit TestImage(const IntSize& size, bool opaque)
        : Image(0)
        , m_size(size)
    {
        m_nativeImage = adoptPtr(new NativeImageSkia());
        m_nativeImage->bitmap().setConfig(SkBitmap::kARGB_8888_Config,
                                          size.width(), size.height(), 0);
        m_nativeImage->bitmap().allocPixels();
        m_nativeImage->bitmap().setIsOpaque(opaque);
    }

    virtual bool isBitmapImage() const
    {
        return true;
    }

    virtual bool currentFrameHasAlpha()
    {
        return !m_nativeImage->bitmap().isOpaque();
    }

    virtual IntSize size() const
    {
        return m_size;
    }

    virtual NativeImagePtr nativeImageForCurrentFrame()
    {
        if (m_size.isZero())
            return 0;

        return m_nativeImage.get();
    }

    // Stub implementations of pure virtual Image functions.
    virtual void destroyDecodedData(bool)
    {
    }

    virtual unsigned int decodedSize() const
    {
        return 0u;
    }

    virtual void draw(WebCore::GraphicsContext*, const WebCore::FloatRect&,
                      const WebCore::FloatRect&, WebCore::ColorSpace,
                      WebCore::CompositeOperator)
    {
    }

private:

    IntSize m_size;

    OwnPtr<NativeImagePtr> m_nativeImage;
};

TEST(ImageLayerChromiumTest, opaqueImages)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerChromium> graphicsLayer = static_pointer_cast<GraphicsLayerChromium>(GraphicsLayer::create(&client));
    ASSERT_TRUE(graphicsLayer.get());

    RefPtr<Image> opaqueImage = TestImage::create(IntSize(100, 100), true);
    ASSERT_TRUE(opaqueImage.get());
    RefPtr<Image> nonOpaqueImage = TestImage::create(IntSize(100, 100), false);
    ASSERT_TRUE(nonOpaqueImage.get());

    ASSERT_FALSE(graphicsLayer->contentsLayer());

    graphicsLayer->setContentsToImage(opaqueImage.get());
    ASSERT_TRUE(graphicsLayer->contentsLayer()->opaque());

    graphicsLayer->setContentsToImage(nonOpaqueImage.get());
    ASSERT_FALSE(graphicsLayer->contentsLayer()->opaque());
}

} // namespace
