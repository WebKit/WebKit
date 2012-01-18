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

#include "LayerTextureUpdater.h"

#include "BitmapCanvasLayerTextureUpdater.h"
#include "BitmapSkPictureCanvasLayerTextureUpdater.h"
#include "FrameBufferSkPictureCanvasLayerTextureUpdater.h"
#include "GraphicsContext.h"
#include "LayerPainterChromium.h"
#include "PlatformContextSkia.h"
#include "SkPictureCanvasLayerTextureUpdater.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

struct PaintCallback {
    virtual void operator()(GraphicsContext&, const IntRect&) = 0;
};

class TestLayerPainterChromium : public LayerPainterChromium {
public:
    TestLayerPainterChromium(PaintCallback& callback) : m_callback(callback) { }

    virtual void paint(GraphicsContext& context, const IntRect& contentRect)
    {
        m_callback(context, contentRect);
    }

  private:
    PaintCallback& m_callback;
};

// Paint callback functions

struct PaintFillOpaque : public PaintCallback {
    virtual void operator()(GraphicsContext& context, const IntRect& contentRect)
    {
        Color opaque(255, 0, 0, 255);
        IntRect top(contentRect.x(), contentRect.y(), contentRect.width(), contentRect.height() / 2);
        IntRect bottom(contentRect.x(), contentRect.y() + contentRect.height() / 2, contentRect.width(), contentRect.height() / 2);
        context.fillRect(top, opaque, ColorSpaceDeviceRGB);
        context.fillRect(bottom, opaque, ColorSpaceDeviceRGB);
    }
};

struct PaintFillAlpha : public PaintCallback {
    virtual void operator()(GraphicsContext& context, const IntRect& contentRect)
    {
        Color alpha(0, 0, 0, 0);
        context.fillRect(contentRect, alpha, ColorSpaceDeviceRGB);
    }
};

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.maxX(), b.maxX()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.maxY(), b.maxY());

TEST(LayerTextureUpdaterTest, testOpaqueRectPresentAfterOpaquePaint)
{
    PaintFillOpaque fillOpaque;
    RefPtr<LayerTextureUpdater> updater;
    IntRect opaqueRect;
    OwnPtr<TestLayerPainterChromium> painter;

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillOpaque));
    updater = BitmapCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 400, 400), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillOpaque));
    updater = BitmapSkPictureCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 400, 400), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillOpaque));
    updater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(painter.release());
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 400, 400), opaqueRect);
}

TEST(LayerTextureUpdaterTest, testOpaqueRectNotPresentAfterNonOpaquePaint)
{
    PaintFillAlpha fillAlpha;
    RefPtr<LayerTextureUpdater> updater;
    IntRect opaqueRect;
    OwnPtr<TestLayerPainterChromium> painter;

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillAlpha));
    updater = BitmapCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillAlpha));
    updater = BitmapSkPictureCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillAlpha));
    updater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(painter.release());
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);
}

TEST(LayerTextureUpdaterTest, testOpaqueRectNotPresentForOpaqueLayerWithOpaquePaint)
{
    PaintFillOpaque fillOpaque;
    RefPtr<LayerTextureUpdater> updater;
    IntRect opaqueRect;
    OwnPtr<TestLayerPainterChromium> painter;

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillOpaque));
    updater = BitmapCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->setOpaque(true);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillOpaque));
    updater = BitmapSkPictureCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->setOpaque(true);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillOpaque));
    updater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(painter.release());
    updater->setOpaque(true);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);
}

TEST(LayerTextureUpdaterTest, testOpaqueRectNotPresentForOpaqueLayerWithNonOpaquePaint)
{
    PaintFillAlpha fillAlpha;
    RefPtr<LayerTextureUpdater> updater;
    IntRect opaqueRect;
    OwnPtr<TestLayerPainterChromium> painter;

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillAlpha));
    updater = BitmapCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->setOpaque(true);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillAlpha));
    updater = BitmapSkPictureCanvasLayerTextureUpdater::create(painter.release(), false);
    updater->setOpaque(true);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);

    opaqueRect = IntRect();
    painter = adoptPtr(new TestLayerPainterChromium(fillAlpha));
    updater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(painter.release());
    updater->setOpaque(true);
    updater->prepareToUpdate(IntRect(0, 0, 400, 400), IntSize(400, 400), 0, 1, &opaqueRect);
    EXPECT_EQ_RECT(IntRect(0, 0, 0, 0), opaqueRect);
}

} // namespace
