/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerChromium.h"

#include "BitmapCanvasLayerTextureUpdater.h"
#include "BitmapSkPictureCanvasLayerTextureUpdater.h"
#include "FrameBufferSkPictureCanvasLayerTextureUpdater.h"
#include "LayerPainterChromium.h"
#include "LayerRendererChromium.h"
#include "PlatformSupport.h"
#include "cc/CCLayerTreeHost.h"
#include <public/Platform.h>
#include <wtf/CurrentTime.h>

namespace WebCore {

class ContentLayerPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ContentLayerPainter);
public:
    static PassOwnPtr<ContentLayerPainter> create(ContentLayerDelegate* delegate)
    {
        return adoptPtr(new ContentLayerPainter(delegate));
    }

    virtual void paint(GraphicsContext& context, const IntRect& contentRect)
    {
        double paintStart = currentTime();
        context.clearRect(contentRect);
        context.clip(contentRect);
        m_delegate->paintContents(context, contentRect);
        double paintEnd = currentTime();
        double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelContentPaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelContentPaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
    }
private:
    explicit ContentLayerPainter(ContentLayerDelegate* delegate)
        : m_delegate(delegate)
    {
    }

    ContentLayerDelegate* m_delegate;
};

PassRefPtr<ContentLayerChromium> ContentLayerChromium::create(ContentLayerDelegate* delegate)
{
    return adoptRef(new ContentLayerChromium(delegate));
}

ContentLayerChromium::ContentLayerChromium(ContentLayerDelegate* delegate)
    : TiledLayerChromium()
    , m_delegate(delegate)
{
}

ContentLayerChromium::~ContentLayerChromium()
{
}

bool ContentLayerChromium::drawsContent() const
{
    return TiledLayerChromium::drawsContent() && m_delegate;
}

void ContentLayerChromium::update(CCTextureUpdater& updater, const CCOcclusionTracker* occlusion)
{
    updateTileSizeAndTilingOption();
    createTextureUpdaterIfNeeded();

    IntRect layerRect;

    // Always call updateLayerRect() but with an empty layer rectangle when
    // layer doesn't draw contents.
    if (drawsContent())
        layerRect = visibleLayerRect();

    updateLayerRect(updater, layerRect, occlusion);
    m_needsDisplay = false;
}

void ContentLayerChromium::idleUpdate(CCTextureUpdater& updater, const CCOcclusionTracker* occlusion)
{
    if (!drawsContent())
        return;

    const IntRect layerRect = visibleLayerRect();
    idleUpdateLayerRect(updater, layerRect, occlusion);
    if (needsIdlePaint(layerRect))
        setNeedsCommit();
}

void ContentLayerChromium::createTextureUpdaterIfNeeded()
{
    if (m_textureUpdater)
        return;
    if (layerTreeHost()->settings().acceleratePainting)
        m_textureUpdater = FrameBufferSkPictureCanvasLayerTextureUpdater::create(ContentLayerPainter::create(m_delegate));
    else if (layerTreeHost()->settings().perTilePainting)
        m_textureUpdater = BitmapSkPictureCanvasLayerTextureUpdater::create(ContentLayerPainter::create(m_delegate), layerTreeHost()->layerRendererCapabilities().usingMapSub);
    else
        m_textureUpdater = BitmapCanvasLayerTextureUpdater::create(ContentLayerPainter::create(m_delegate), layerTreeHost()->layerRendererCapabilities().usingMapSub);
    m_textureUpdater->setOpaque(opaque());

    GC3Denum textureFormat = layerTreeHost()->layerRendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
    setSampledTexelFormat(textureUpdater()->sampledTexelFormat(textureFormat));
}

void ContentLayerChromium::setOpaque(bool opaque)
{
    LayerChromium::setOpaque(opaque);
    if (m_textureUpdater)
        m_textureUpdater->setOpaque(opaque);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
