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

    if (m_background && m_background->texture()->isReserved())
        scrollbarLayer->setBackgroundTextureId(m_background->texture()->textureId());
    else
        scrollbarLayer->setBackgroundTextureId(0);

    if (m_thumb && m_thumb->texture()->isReserved())
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
    static PassOwnPtr<ScrollbarBackgroundPainter> create(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme)
    {
        return adoptPtr(new ScrollbarBackgroundPainter(scrollbar, theme));
    }

    virtual void paint(SkCanvas* canvas, const IntRect& contentRect, IntRect&) OVERRIDE
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
        if (thumbPresent) {
            // FIXME: There's no "paint the whole track" part. Drawing both the
            // BackTrackPart and the ForwardTrackPart in their splitTrack rects
            // ends up leaving a distinctive line. Painting one part as the
            // entire track appears to be identical to painting both and
            // covering up the split between them with the thumb.
            m_theme->paintTrackPiece(&context, m_scrollbar, trackPaintRect, BackTrackPart);
        }

        m_theme->paintTickmarks(&context, m_scrollbar, trackPaintRect);
    }
private:
    ScrollbarBackgroundPainter(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme)
        : m_scrollbar(scrollbar)
        , m_theme(theme)
    {
    }

    ScrollbarThemeClient* m_scrollbar;
    ScrollbarThemeComposite* m_theme;
};

class ScrollbarThumbPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ScrollbarThumbPainter);
public:
    static PassOwnPtr<ScrollbarThumbPainter> create(ScrollbarThemeClient* scrollbar, ScrollbarThemeComposite* theme)
    {
        return adoptPtr(new ScrollbarThumbPainter(scrollbar, theme));
    }

    virtual void paint(SkCanvas* canvas, const IntRect& contentRect, IntRect& opaque) OVERRIDE
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
        m_backgroundUpdater.clear();
        m_background.clear();
        m_thumbUpdater.clear();
        m_thumb.clear();
    }

    LayerChromium::setLayerTreeHost(host);
}

void ScrollbarLayerChromium::createTextureUpdaterIfNeeded()
{
    bool useMapSubImage = layerTreeHost()->layerRendererCapabilities().usingMapSub;
    m_textureFormat = layerTreeHost()->layerRendererCapabilities().bestTextureFormat;

    if (!m_backgroundUpdater)
        m_backgroundUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarBackgroundPainter::create(m_scrollbar.get(), theme()), useMapSubImage);
    if (!m_background)
        m_background = m_backgroundUpdater->createTexture(layerTreeHost()->contentsTextureManager());

    if (!m_thumbUpdater)
        m_thumbUpdater = BitmapCanvasLayerTextureUpdater::create(ScrollbarThumbPainter::create(m_scrollbar.get(), theme()), useMapSubImage);
    if (!m_thumb)
        m_thumb = m_thumbUpdater->createTexture(layerTreeHost()->contentsTextureManager());
}

void ScrollbarLayerChromium::updatePart(LayerTextureUpdater* painter, LayerTextureUpdater::Texture* texture, const IntRect& rect, CCTextureUpdater& updater)
{
    bool textureValid = texture->texture()->isValid(rect.size(), m_textureFormat);
    // Skip painting and uploading if there are no invalidations.
    if (textureValid && m_updateRect.isEmpty()) {
        texture->texture()->reserve(rect.size(), m_textureFormat);
        return;
    }

    // ScrollbarLayerChromium doesn't support partial uploads, so any
    // invalidation is treated a full layer invalidation.
    if (layerTreeHost()->bufferedUpdates() && textureValid)
        layerTreeHost()->deleteTextureAfterCommit(texture->texture()->steal());

    if (!texture->texture()->reserve(rect.size(), m_textureFormat))
        return;

    // Paint and upload the entire part.
    IntRect paintedOpaqueRect;
    painter->prepareToUpdate(rect, rect.size(), 1, paintedOpaqueRect);
    texture->prepareRect(rect);

    IntRect destRect(IntPoint(), rect.size());
    updater.appendUpdate(texture, rect, destRect);
}

void ScrollbarLayerChromium::update(CCTextureUpdater& updater, const CCOcclusionTracker*)
{
    if (contentBounds().isEmpty())
        return;

    createTextureUpdaterIfNeeded();

    IntPoint scrollbarOrigin(m_scrollbar->x(), m_scrollbar->y());
    IntRect contentRect(scrollbarOrigin, contentBounds());
    updatePart(m_backgroundUpdater.get(), m_background.get(), contentRect, updater);

    // Consider the thumb to be at the origin when painting.
    IntRect thumbRect = IntRect(IntPoint(), theme()->thumbRect(m_scrollbar.get()).size());
    if (!thumbRect.isEmpty())
        updatePart(m_thumbUpdater.get(), m_thumb.get(), thumbRect, updater);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
