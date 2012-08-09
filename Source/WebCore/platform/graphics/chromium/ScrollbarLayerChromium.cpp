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

#if USE(ACCELERATED_COMPOSITING)

#include "ScrollbarLayerChromium.h"

#include "BitmapCanvasLayerTextureUpdater.h"
#include "LayerPainterChromium.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCScrollbarLayerImpl.h"
#include "cc/CCTextureUpdateQueue.h"
#include <public/WebRect.h>

using WebKit::WebRect;

namespace WebCore {

PassOwnPtr<CCLayerImpl> ScrollbarLayerChromium::createCCLayerImpl()
{
    return CCScrollbarLayerImpl::create(id());
}

PassRefPtr<ScrollbarLayerChromium> ScrollbarLayerChromium::create(PassOwnPtr<WebKit::WebScrollbar> scrollbar, WebKit::WebScrollbarThemePainter painter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry> geometry, int scrollLayerId)
{
    return adoptRef(new ScrollbarLayerChromium(scrollbar, painter, geometry, scrollLayerId));
}

ScrollbarLayerChromium::ScrollbarLayerChromium(PassOwnPtr<WebKit::WebScrollbar> scrollbar, WebKit::WebScrollbarThemePainter painter, PassOwnPtr<WebKit::WebScrollbarThemeGeometry> geometry, int scrollLayerId)
    : m_scrollbar(scrollbar)
    , m_painter(painter)
    , m_geometry(geometry)
    , m_scrollLayerId(scrollLayerId)
    , m_textureFormat(GraphicsContext3D::INVALID_ENUM)
{
}

void ScrollbarLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCScrollbarLayerImpl* scrollbarLayer = static_cast<CCScrollbarLayerImpl*>(layer);

    if (!scrollbarLayer->scrollbarGeometry())
        scrollbarLayer->setScrollbarGeometry(adoptPtr(m_geometry->clone()));

    scrollbarLayer->setScrollbarData(m_scrollbar.get());

    if (m_backTrack && m_backTrack->texture()->haveBackingTexture())
        scrollbarLayer->setBackTrackResourceId(m_backTrack->texture()->resourceId());
    else
        scrollbarLayer->setBackTrackResourceId(0);

    if (m_foreTrack && m_foreTrack->texture()->haveBackingTexture())
        scrollbarLayer->setForeTrackResourceId(m_foreTrack->texture()->resourceId());
    else
        scrollbarLayer->setForeTrackResourceId(0);

    if (m_thumb && m_thumb->texture()->haveBackingTexture())
        scrollbarLayer->setThumbResourceId(m_thumb->texture()->resourceId());
    else
        scrollbarLayer->setThumbResourceId(0);
}

class ScrollbarBackgroundPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ScrollbarBackgroundPainter);
public:
    static PassOwnPtr<ScrollbarBackgroundPainter> create(WebKit::WebScrollbar* scrollbar, WebKit::WebScrollbarThemePainter painter, WebKit::WebScrollbarThemeGeometry* geometry, WebKit::WebScrollbar::ScrollbarPart trackPart)
    {
        return adoptPtr(new ScrollbarBackgroundPainter(scrollbar, painter, geometry, trackPart));
    }

    virtual void paint(SkCanvas* skCanvas, const IntRect& contentRect, FloatRect&) OVERRIDE
    {
        WebKit::WebCanvas* canvas = skCanvas;
        // The following is a simplification of ScrollbarThemeComposite::paint.
        WebKit::WebRect contentWebRect(contentRect.x(), contentRect.y(), contentRect.width(), contentRect.height());
        m_painter.paintScrollbarBackground(canvas, contentWebRect);

        if (m_geometry->hasButtons(m_scrollbar)) {
            WebRect backButtonStartPaintRect = m_geometry->backButtonStartRect(m_scrollbar);
            m_painter.paintBackButtonStart(canvas, backButtonStartPaintRect);

            WebRect backButtonEndPaintRect = m_geometry->backButtonEndRect(m_scrollbar);
            m_painter.paintBackButtonEnd(canvas, backButtonEndPaintRect);

            WebRect forwardButtonStartPaintRect = m_geometry->forwardButtonStartRect(m_scrollbar);
            m_painter.paintForwardButtonStart(canvas, forwardButtonStartPaintRect);

            WebRect forwardButtonEndPaintRect = m_geometry->forwardButtonEndRect(m_scrollbar);
            m_painter.paintForwardButtonEnd(canvas, forwardButtonEndPaintRect);
        }

        WebRect trackPaintRect = m_geometry->trackRect(m_scrollbar);
        m_painter.paintTrackBackground(canvas, trackPaintRect);

        bool thumbPresent = m_geometry->hasThumb(m_scrollbar);
        if (thumbPresent) {
            if (m_trackPart == WebKit::WebScrollbar::ForwardTrackPart)
                m_painter.paintForwardTrackPart(canvas, trackPaintRect);
            else
                m_painter.paintBackTrackPart(canvas, trackPaintRect);
        }

        m_painter.paintTickmarks(canvas, trackPaintRect);
    }
private:
    ScrollbarBackgroundPainter(WebKit::WebScrollbar* scrollbar, WebKit::WebScrollbarThemePainter painter, WebKit::WebScrollbarThemeGeometry* geometry, WebKit::WebScrollbar::ScrollbarPart trackPart)
        : m_scrollbar(scrollbar)
        , m_painter(painter)
        , m_geometry(geometry)
        , m_trackPart(trackPart)
    {
    }

    WebKit::WebScrollbar* m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    WebKit::WebScrollbarThemeGeometry* m_geometry;
    WebKit::WebScrollbar::ScrollbarPart m_trackPart;
};

class ScrollbarThumbPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ScrollbarThumbPainter);
public:
    static PassOwnPtr<ScrollbarThumbPainter> create(WebKit::WebScrollbar* scrollbar, WebKit::WebScrollbarThemePainter painter, WebKit::WebScrollbarThemeGeometry* geometry)
    {
        return adoptPtr(new ScrollbarThumbPainter(scrollbar, painter, geometry));
    }

    virtual void paint(SkCanvas* skCanvas, const IntRect& contentRect, FloatRect& opaque) OVERRIDE
    {
        WebKit::WebCanvas* canvas = skCanvas;

        // Consider the thumb to be at the origin when painting.
        WebRect thumbRect = m_geometry->thumbRect(m_scrollbar);
        thumbRect.x = 0;
        thumbRect.y = 0;
        m_painter.paintThumb(canvas, thumbRect);
    }

private:
    ScrollbarThumbPainter(WebKit::WebScrollbar* scrollbar, WebKit::WebScrollbarThemePainter painter, WebKit::WebScrollbarThemeGeometry* geometry)
        : m_scrollbar(scrollbar)
        , m_painter(painter)
        , m_geometry(geometry)
    {
    }

    WebKit::WebScrollbar* m_scrollbar;
    WebKit::WebScrollbarThemePainter m_painter;
    WebKit::WebScrollbarThemeGeometry* m_geometry;
};

void ScrollbarLayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (!host || host != layerTreeHost()) {
        m_backTrackUpdater.clear();
        m_backTrack.clear();
        m_thumbUpdater.clear();
        m_thumb.clear();
    }

    LayerChromium::setLayerTreeHost(host);
}

void ScrollbarLayerChromium::createTextureUpdaterIfNeeded()
{
    m_textureFormat = layerTreeHost()->layerRendererCapabilities().bestTextureFormat;

    if (!m_backTrackUpdater)
        m_backTrackUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), m_painter, m_geometry.get(), WebKit::WebScrollbar::BackTrackPart));
    if (!m_backTrack)
        m_backTrack = m_backTrackUpdater->createTexture(layerTreeHost()->contentsTextureManager());

    // Only create two-part track if we think the two parts could be different in appearance.
    if (m_scrollbar->isCustomScrollbar()) {
        if (!m_foreTrackUpdater)
            m_foreTrackUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), m_painter, m_geometry.get(), WebKit::WebScrollbar::ForwardTrackPart));
        if (!m_foreTrack)
            m_foreTrack = m_foreTrackUpdater->createTexture(layerTreeHost()->contentsTextureManager());
    }

    if (!m_thumbUpdater)
        m_thumbUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarThumbPainter::create(m_scrollbar.get(), m_painter, m_geometry.get()));
    if (!m_thumb)
        m_thumb = m_thumbUpdater->createTexture(layerTreeHost()->contentsTextureManager());
}

void ScrollbarLayerChromium::updatePart(LayerTextureUpdater* painter, LayerTextureUpdater::Texture* texture, const IntRect& rect, CCTextureUpdateQueue& queue, CCRenderingStats& stats)
{
    // Skip painting and uploading if there are no invalidations and
    // we already have valid texture data.
    if (texture->texture()->haveBackingTexture()
            && texture->texture()->size() == rect.size()
            && m_updateRect.isEmpty())
        return;

    // We should always have enough memory for UI.
    ASSERT(texture->texture()->canAcquireBackingTexture());
    if (!texture->texture()->canAcquireBackingTexture())
        return;

    // Paint and upload the entire part.
    IntRect paintedOpaqueRect;
    painter->prepareToUpdate(rect, rect.size(), 1, 1, paintedOpaqueRect, stats);
    texture->prepareRect(rect, stats);

    IntRect destRect(IntPoint(), rect.size());
    TextureUploader::Parameters upload = { texture, rect, destRect };
    queue.appendFullUpload(upload);
}


void ScrollbarLayerChromium::setTexturePriorities(const CCPriorityCalculator&)
{
    if (contentBounds().isEmpty())
        return;

    createTextureUpdaterIfNeeded();

    bool drawsToRoot = !renderTarget()->parent();
    if (m_backTrack) {
        m_backTrack->texture()->setDimensions(contentBounds(), m_textureFormat);
        m_backTrack->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(drawsToRoot));
    }
    if (m_foreTrack) {
        m_foreTrack->texture()->setDimensions(contentBounds(), m_textureFormat);
        m_foreTrack->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(drawsToRoot));
    }
    if (m_thumb) {
        WebKit::WebRect thumbRect = m_geometry->thumbRect(m_scrollbar.get());
        m_thumb->texture()->setDimensions(IntSize(thumbRect.width, thumbRect.height), m_textureFormat);
        m_thumb->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(drawsToRoot));
    }
}

void ScrollbarLayerChromium::update(CCTextureUpdateQueue& queue, const CCOcclusionTracker*, CCRenderingStats& stats)
{
    if (contentBounds().isEmpty())
        return;

    createTextureUpdaterIfNeeded();

    IntPoint scrollbarOrigin(m_scrollbar->location().x, m_scrollbar->location().y);
    IntRect contentRect(scrollbarOrigin, contentBounds());
    updatePart(m_backTrackUpdater.get(), m_backTrack.get(), contentRect, queue, stats);
    if (m_foreTrack && m_foreTrackUpdater)
        updatePart(m_foreTrackUpdater.get(), m_foreTrack.get(), contentRect, queue, stats);

    // Consider the thumb to be at the origin when painting.
    WebKit::WebRect thumbRect = m_geometry->thumbRect(m_scrollbar.get());
    IntRect originThumbRect = IntRect(0, 0, thumbRect.width, thumbRect.height);
    if (!originThumbRect.isEmpty())
        updatePart(m_thumbUpdater.get(), m_thumb.get(), originThumbRect, queue, stats);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
