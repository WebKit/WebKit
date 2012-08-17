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

#include "ContentLayerChromium.h"

#include "BitmapCanvasLayerTextureUpdater.h"
#include "CCLayerTreeTestCommon.h"
#include "CCRenderingStats.h"
#include "GraphicsContext.h"
#include "OpaqueRectTrackingContentLayerDelegate.h"
#include "skia/ext/platform_canvas.h"
#include <gtest/gtest.h>
#include <public/WebFloatRect.h>
#include <public/WebRect.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class OpaqueRectDrawingGraphicsContextPainter : public GraphicsContextPainter {
public:
    explicit OpaqueRectDrawingGraphicsContextPainter(const IntRect& opaqueRect, const IntRect& contentRect)
        : m_opaqueRect(opaqueRect)
        , m_contentRect(contentRect)
    {
    }

    virtual ~OpaqueRectDrawingGraphicsContextPainter()
    {
    }

    virtual void paint(GraphicsContext& context, const IntRect& clip) OVERRIDE
    {
        Color alpha(0, 0, 0, 0);
        context.fillRect(m_contentRect, alpha, ColorSpaceDeviceRGB);

        Color white(255, 255, 255, 255);
        context.fillRect(m_opaqueRect, white, ColorSpaceDeviceRGB);
    }

private:
    IntRect m_opaqueRect;
    IntRect m_contentRect;
};

class MockContentLayerDelegate : public ContentLayerDelegate {
public:
    explicit MockContentLayerDelegate(OpaqueRectTrackingContentLayerDelegate* client)
        : m_client(client)
    {
    }

    virtual void paintContents(SkCanvas* canvas, const IntRect& clip, FloatRect& opaque) OVERRIDE
    {
        WebFloatRect resultingOpaqueRect(opaque.x(), opaque.y(), opaque.width(), opaque.height());
        WebRect webClipRect(clip.x(), clip.y(), clip.width(), clip.height());
        m_client->paintContents(canvas, webClipRect, resultingOpaqueRect);
        opaque = FloatRect(resultingOpaqueRect.x, resultingOpaqueRect.y, resultingOpaqueRect.width, resultingOpaqueRect.height);
    }

private:
    OpaqueRectTrackingContentLayerDelegate* m_client;
};

TEST(ContentLayerChromiumTest, ContentLayerPainterWithDeviceScale)
{
    float contentsScale = 2;
    IntRect contentRect(10, 10, 100, 100);
    IntRect opaqueRectInLayerSpace(5, 5, 20, 20);
    IntRect opaqueRectInContentSpace = opaqueRectInLayerSpace;
    opaqueRectInContentSpace.scale(contentsScale);
    OwnPtr<SkCanvas> canvas = adoptPtr(skia::CreateBitmapCanvas(contentRect.width(), contentRect.height(), false));
    OpaqueRectDrawingGraphicsContextPainter painter(opaqueRectInLayerSpace, contentRect);
    OpaqueRectTrackingContentLayerDelegate opaqueRectTrackingContentLayerDelegate(&painter);
    MockContentLayerDelegate delegate(&opaqueRectTrackingContentLayerDelegate);
    RefPtr<BitmapCanvasLayerTextureUpdater> updater = BitmapCanvasLayerTextureUpdater::create(ContentLayerPainter::create(&delegate));

    IntRect resultingOpaqueRect;
    CCRenderingStats stats;
    updater->prepareToUpdate(contentRect, IntSize(256, 256), contentsScale, contentsScale, resultingOpaqueRect, stats);

    EXPECT_INT_RECT_EQ(opaqueRectInContentSpace, resultingOpaqueRect);
}

} // namespace
