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
#include "GraphicsContext.h"
#include "LayerPainterChromium.h"
#include "PlatformContextSkia.h"
#include "Scrollbar.h"
#include "ScrollbarThemeComposite.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCScrollbarLayerImpl.h"
#include "cc/CCTextureUpdater.h"

namespace WebCore {

PassOwnPtr<CCLayerImpl> ScrollbarLayerChromium::createCCLayerImpl()
{
    return CCScrollbarLayerImpl::create(id());
}

PassRefPtr<ScrollbarLayerChromium> ScrollbarLayerChromium::create(Scrollbar* scrollbar, int scrollLayerId)
{
    return adoptRef(new ScrollbarLayerChromium(scrollbar, scrollLayerId));
}

ScrollbarLayerChromium::ScrollbarLayerChromium(Scrollbar* scrollbar, int scrollLayerId)
    : m_scrollbar(scrollbar)
    , m_scrollLayerId(scrollLayerId)
    , m_textureFormat(GraphicsContext3D::INVALID_ENUM)
    , m_scrollbarOverlayStyle(scrollbar->scrollbarOverlayStyle())
    , m_isScrollableAreaActive(scrollbar->isScrollableAreaActive())
    , m_isScrollViewScrollbar(scrollbar->isScrollViewScrollbar())
    , m_orientation(scrollbar->orientation())
    , m_controlSize(scrollbar->controlSize())
{
}

ScrollbarThemeComposite* ScrollbarLayerChromium::theme() const
{
    // All Chromium scrollbars are ScrollbarThemeComposite-derived.
    return static_cast<ScrollbarThemeComposite*>(m_scrollbar->theme());
}

void ScrollbarLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCScrollbarLayerImpl* scrollbarLayer = static_cast<CCScrollbarLayerImpl*>(layer);

    scrollbarLayer->setScrollbarOverlayStyle(m_scrollbarOverlayStyle);

    if (m_backTrack && m_backTrack->texture()->haveBackingTexture())
        scrollbarLayer->setBackTrackTextureId(m_backTrack->texture()->textureId());
    else
        scrollbarLayer->setBackTrackTextureId(0);

    if (m_foreTrack && m_foreTrack->texture()->haveBackingTexture())
        scrollbarLayer->setForeTrackTextureId(m_foreTrack->texture()->textureId());
    else
        scrollbarLayer->setForeTrackTextureId(0);

    if (m_thumb && m_thumb->texture()->haveBackingTexture())
        scrollbarLayer->setThumbTextureId(m_thumb->texture()->textureId());
    else
        scrollbarLayer->setThumbTextureId(0);

    Vector<IntRect> tickmarks;
    m_scrollbar->getTickmarks(tickmarks);
    scrollbarLayer->setTickmarks(tickmarks);

    scrollbarLayer->setIsScrollableAreaActive(m_isScrollableAreaActive);
    scrollbarLayer->setIsScrollViewScrollbar(m_isScrollViewScrollbar);

    scrollbarLayer->setOrientation(m_orientation);

    scrollbarLayer->setControlSize(m_controlSize);

    scrollbarLayer->setPressedPart(m_scrollbar->pressedPart());
    scrollbarLayer->setHoveredPart(m_scrollbar->hoveredPart());

    scrollbarLayer->setEnabled(m_scrollbar->enabled());
}

class ScrollbarBackgroundPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ScrollbarBackgroundPainter);
public:
    static PassOwnPtr<ScrollbarBackgroundPainter> create(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme, ScrollbarPart trackPart)
    {
        return adoptPtr(new ScrollbarBackgroundPainter(scrollbar, theme, trackPart));
    }

    virtual void paint(SkCanvas* canvas, const IntRect& contentRect, FloatRect&) OVERRIDE
    {
        PlatformContextSkia platformContext(canvas);
        platformContext.setDrawingToImageBuffer(true);
        GraphicsContext context(&platformContext);

        // The following is a simplification of ScrollbarThemeComposite::paint.
        m_theme->paintScrollbarBackground(&context, m_scrollbar);

        if (m_theme->hasButtons(m_scrollbar)) {
            IntRect backButtonStartPaintRect = m_theme->backButtonRect(m_scrollbar, BackButtonStartPart, true);
            m_theme->paintButton(&context, m_scrollbar, backButtonStartPaintRect, BackButtonStartPart);

            IntRect backButtonEndPaintRect = m_theme->backButtonRect(m_scrollbar, BackButtonEndPart, true);
            m_theme->paintButton(&context, m_scrollbar, backButtonEndPaintRect, BackButtonEndPart);

            IntRect forwardButtonStartPaintRect = m_theme->forwardButtonRect(m_scrollbar, ForwardButtonStartPart, true);
            m_theme->paintButton(&context, m_scrollbar, forwardButtonStartPaintRect, ForwardButtonStartPart);

            IntRect forwardButtonEndPaintRect = m_theme->forwardButtonRect(m_scrollbar, ForwardButtonEndPart, true);
            m_theme->paintButton(&context, m_scrollbar, forwardButtonEndPaintRect, ForwardButtonEndPart);
        }

        IntRect trackPaintRect = m_theme->trackRect(m_scrollbar, true);
        m_theme->paintTrackBackground(&context, m_scrollbar, trackPaintRect);

        bool thumbPresent = m_theme->hasThumb(m_scrollbar);
        if (thumbPresent)
            m_theme->paintTrackPiece(&context, m_scrollbar, trackPaintRect, m_trackPart);

        m_theme->paintTickmarks(&context, m_scrollbar, trackPaintRect);
    }
private:
    ScrollbarBackgroundPainter(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme, ScrollbarPart trackPart)
        : m_scrollbar(scrollbar)
        , m_theme(theme)
        , m_trackPart(trackPart)
    {
    }

    ScrollbarThemeClient* m_scrollbar;
    ScrollbarThemeComposite* m_theme;
    ScrollbarPart m_trackPart;
};

class ScrollbarThumbPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ScrollbarThumbPainter);
public:
    static PassOwnPtr<ScrollbarThumbPainter> create(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme)
    {
        return adoptPtr(new ScrollbarThumbPainter(scrollbar, theme));
    }

    virtual void paint(SkCanvas* canvas, const IntRect& contentRect, FloatRect& opaque) OVERRIDE
    {
        PlatformContextSkia platformContext(canvas);
        platformContext.setDrawingToImageBuffer(true);
        GraphicsContext context(&platformContext);

        // Consider the thumb to be at the origin when painting.
        IntRect thumbRect = IntRect(IntPoint(), m_theme->thumbRect(m_scrollbar).size());
        m_theme->paintThumb(&context, m_scrollbar, thumbRect);
    }

private:
    ScrollbarThumbPainter(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme)
        : m_scrollbar(scrollbar)
        , m_theme(theme)
    {
    }

    ScrollbarThemeClient* m_scrollbar;
    ScrollbarThemeComposite* m_theme;
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
    bool useMapSubImage = layerTreeHost()->layerRendererCapabilities().usingMapSub;
    m_textureFormat = layerTreeHost()->layerRendererCapabilities().bestTextureFormat;

    if (!m_backTrackUpdater)
        m_backTrackUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), theme(), BackTrackPart), useMapSubImage);
    if (!m_backTrack)
        m_backTrack = m_backTrackUpdater->createTexture(layerTreeHost()->contentsTextureManager());

    // Only create two-part track if we think the two parts could be different in appearance.
    if (m_scrollbar->isCustomScrollbar()) {
        if (!m_foreTrackUpdater)
            m_foreTrackUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), theme(), ForwardTrackPart), useMapSubImage);
        if (!m_foreTrack)
            m_foreTrack = m_foreTrackUpdater->createTexture(layerTreeHost()->contentsTextureManager());
    }

    if (!m_thumbUpdater)
        m_thumbUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarThumbPainter::create(m_scrollbar.get(), theme()), useMapSubImage);
    if (!m_thumb)
        m_thumb = m_thumbUpdater->createTexture(layerTreeHost()->contentsTextureManager());
}

void ScrollbarLayerChromium::updatePart(LayerTextureUpdater* painter, LayerTextureUpdater::Texture* texture, const IntRect& rect, CCTextureUpdater& updater)
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
    painter->prepareToUpdate(rect, rect.size(), 1, 1, paintedOpaqueRect);
    texture->prepareRect(rect);

    IntRect destRect(IntPoint(), rect.size());
    updater.appendUpdate(texture, rect, destRect);
}


void ScrollbarLayerChromium::setTexturePriorities(const CCPriorityCalculator&)
{
    if (contentBounds().isEmpty())
        return;

    createTextureUpdaterIfNeeded();

    bool drawsToRootSurface = !targetRenderSurface()->targetRenderSurface();
    if (m_backTrack) {
        m_backTrack->texture()->setDimensions(contentBounds(), m_textureFormat);
        m_backTrack->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(drawsToRootSurface));
    }
    if (m_foreTrack) {
        m_foreTrack->texture()->setDimensions(contentBounds(), m_textureFormat);
        m_foreTrack->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(drawsToRootSurface));
    }
    if (m_thumb) {
        IntSize thumbSize = theme()->thumbRect(m_scrollbar.get()).size();
        m_thumb->texture()->setDimensions(thumbSize, m_textureFormat);
        m_thumb->texture()->setRequestPriority(CCPriorityCalculator::uiPriority(drawsToRootSurface));
    }
}

void ScrollbarLayerChromium::update(CCTextureUpdater& updater, const CCOcclusionTracker*)
{
    if (contentBounds().isEmpty())
        return;

    createTextureUpdaterIfNeeded();

    IntPoint scrollbarOrigin(m_scrollbar->x(), m_scrollbar->y());
    IntRect contentRect(scrollbarOrigin, contentBounds());
    updatePart(m_backTrackUpdater.get(), m_backTrack.get(), contentRect, updater);
    if (m_foreTrack && m_foreTrackUpdater)
        updatePart(m_foreTrackUpdater.get(), m_foreTrack.get(), contentRect, updater);

    // Consider the thumb to be at the origin when painting.
    IntRect thumbRect = IntRect(IntPoint(), theme()->thumbRect(m_scrollbar.get()).size());
    if (!thumbRect.isEmpty())
        updatePart(m_thumbUpdater.get(), m_thumb.get(), thumbRect, updater);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
