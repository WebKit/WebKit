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
#include "CCGeometryTestUtils.h"
#include "CCRenderingStats.h"
#include "ContentLayerChromiumClient.h"
#include "skia/ext/platform_canvas.h"
#include <gtest/gtest.h>
#include <public/WebFloatRect.h>
#include <public/WebRect.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class MockContentLayerChromiumClient : public ContentLayerChromiumClient {
public:
    explicit MockContentLayerChromiumClient(IntRect opaqueLayerRect)
        : m_opaqueLayerRect(opaqueLayerRect)
    {
    }

    virtual void paintContents(SkCanvas*, const IntRect&, FloatRect& opaque) OVERRIDE
    {
        opaque = FloatRect(m_opaqueLayerRect);
    }

private:
    IntRect m_opaqueLayerRect;
};

TEST(ContentLayerChromiumTest, ContentLayerPainterWithDeviceScale)
{
    float contentsScale = 2;
    IntRect contentRect(10, 10, 100, 100);
    IntRect opaqueRectInLayerSpace(5, 5, 20, 20);
    IntRect opaqueRectInContentSpace = opaqueRectInLayerSpace;
    opaqueRectInContentSpace.scale(contentsScale);
    OwnPtr<SkCanvas> canvas = adoptPtr(skia::CreateBitmapCanvas(contentRect.width(), contentRect.height(), false));
    MockContentLayerChromiumClient client(opaqueRectInLayerSpace);
    RefPtr<BitmapCanvasLayerTextureUpdater> updater = BitmapCanvasLayerTextureUpdater::create(ContentLayerPainter::create(&client));

    IntRect resultingOpaqueRect;
    CCRenderingStats stats;
    updater->prepareToUpdate(contentRect, IntSize(256, 256), contentsScale, contentsScale, resultingOpaqueRect, stats);

    EXPECT_RECT_EQ(opaqueRectInContentSpace, resultingOpaqueRect);
}

} // namespace
