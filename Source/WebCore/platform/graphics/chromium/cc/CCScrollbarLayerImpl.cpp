/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCScrollbarLayerImpl.h"

#include "CCTileDrawQuad.h"
#include "LayerRendererChromium.h"
#include "ManagedTexture.h"
#include "PlatformCanvas.h"
#include "ScrollbarTheme.h"
#include "cc/CCQuadCuller.h"

namespace WebCore {

PassOwnPtr<CCScrollbarLayerImpl> CCScrollbarLayerImpl::create(int id)
{
    return adoptPtr(new CCScrollbarLayerImpl(id));
}

CCScrollbarLayerImpl::CCScrollbarLayerImpl(int id)
    : CCLayerImpl(id)
    , m_scrollLayer(0)
    , m_scrollbar(this)
{
}

void CCScrollbarLayerImpl::willDraw(LayerRendererChromium* layerRenderer)
{
    CCLayerImpl::willDraw(layerRenderer);

    if (bounds().isEmpty() || contentBounds().isEmpty())
        return;

    if (!m_texture)
        m_texture = ManagedTexture::create(layerRenderer->renderSurfaceTextureManager());

    // The context could have been lost since the last frame and the old texture
    // manager may no longer be valid.
    m_texture->setTextureManager(layerRenderer->renderSurfaceTextureManager());

    IntSize textureSize = contentBounds();
    if (!m_texture->reserve(textureSize, GraphicsContext3D::RGBA))
        return;

    PlatformCanvas canvas;
    canvas.resize(textureSize);
    {
        PlatformCanvas::Painter painter(&canvas, PlatformCanvas::Painter::GrayscaleText);
        paint(painter.context());
    }

    {
        PlatformCanvas::AutoLocker locker(&canvas);
        GraphicsContext3D* context = layerRenderer->context();
        m_texture->bindTexture(context, layerRenderer->renderSurfaceTextureAllocator());

        // FIXME: Skia uses BGRA actually, we correct that with the swizzle pixel shader.
        GLC(context, context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, m_texture->format(), canvas.size().width(), canvas.size().height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, locker.pixels()));
    }
}

void CCScrollbarLayerImpl::appendQuads(CCQuadCuller& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    if (!m_texture->isReserved())
        return;

    IntRect quadRect(IntPoint(), bounds());
    quadList.append(CCTileDrawQuad::create(sharedQuadState, quadRect, quadRect, m_texture->textureId(), IntPoint(), m_texture->size(), GraphicsContext3D::NEAREST, true, true, true, true, true));
}

void CCScrollbarLayerImpl::didDraw()
{
    CCLayerImpl::didDraw();

    m_texture->unreserve();
}

void CCScrollbarLayerImpl::paint(GraphicsContext* context)
{
    ScrollbarTheme* theme = ScrollbarTheme::theme(); // FIXME: should make impl-side clone if needed

    context->clearRect(IntRect(IntPoint(), contentBounds()));
    theme->paint(&m_scrollbar, context, IntRect(IntPoint(), contentBounds()));
}


int CCScrollbarLayerImpl::CCScrollbar::x() const
{
    return frameRect().x();
}

int CCScrollbarLayerImpl::CCScrollbar::y() const
{
    return frameRect().y();
}

int CCScrollbarLayerImpl::CCScrollbar::width() const
{
    return frameRect().width();
}

int CCScrollbarLayerImpl::CCScrollbar::height() const
{
    return frameRect().height();
}

IntSize CCScrollbarLayerImpl::CCScrollbar::size() const
{
    return frameRect().size();
}

IntPoint CCScrollbarLayerImpl::CCScrollbar::location() const
{
    return frameRect().location();
}

ScrollView* CCScrollbarLayerImpl::CCScrollbar::parent() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

ScrollView* CCScrollbarLayerImpl::CCScrollbar::root() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void CCScrollbarLayerImpl::CCScrollbar::setFrameRect(const IntRect&)
{
    ASSERT_NOT_REACHED();
}

IntRect CCScrollbarLayerImpl::CCScrollbar::frameRect() const
{
    return IntRect(IntPoint(), m_owner->contentBounds());
}

void CCScrollbarLayerImpl::CCScrollbar::invalidate()
{
    invalidateRect(frameRect());
}

void CCScrollbarLayerImpl::CCScrollbar::invalidateRect(const IntRect&)
{
    ASSERT_NOT_REACHED();
}

ScrollbarOverlayStyle CCScrollbarLayerImpl::CCScrollbar::scrollbarOverlayStyle() const
{
    return m_owner->m_scrollbarOverlayStyle;
}

void CCScrollbarLayerImpl::CCScrollbar::getTickmarks(Vector<IntRect>& tickmarks) const
{
    tickmarks = m_owner->m_tickmarks;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollableAreaActive() const
{
    return m_owner->m_isScrollableAreaActive;
}

bool CCScrollbarLayerImpl::CCScrollbar::isScrollViewScrollbar() const
{
    return m_owner->m_isScrollViewScrollbar;
}

IntPoint CCScrollbarLayerImpl::CCScrollbar::convertFromContainingWindow(const IntPoint& windowPoint)
{
    ASSERT_NOT_REACHED();
    return windowPoint;
}

bool CCScrollbarLayerImpl::CCScrollbar::isCustomScrollbar() const
{
    return false;
}

ScrollbarOrientation CCScrollbarLayerImpl::CCScrollbar::orientation() const
{
    return m_owner->m_orientation;
}

int CCScrollbarLayerImpl::CCScrollbar::value() const
{
    if (!m_owner->m_scrollLayer)
        return 0;
    if (orientation() == HorizontalScrollbar)
        return m_owner->m_scrollLayer->scrollPosition().x() + m_owner->m_scrollLayer->scrollDelta().width();
    return m_owner->m_scrollLayer->scrollPosition().y() + m_owner->m_scrollLayer->scrollDelta().height();
}

float CCScrollbarLayerImpl::CCScrollbar::currentPos() const
{
    return value();
}

int CCScrollbarLayerImpl::CCScrollbar::visibleSize() const
{
    return totalSize() - maximum();
}

int CCScrollbarLayerImpl::CCScrollbar::totalSize() const
{
    if (!m_owner->m_scrollLayer || !m_owner->m_scrollLayer->children().size())
        return 0;
    // Copy & paste from CCLayerTreeHostImpl...
    // FIXME: Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    if (orientation() == HorizontalScrollbar)
        return m_owner->m_scrollLayer->children()[0]->contentBounds().width();
    return m_owner->m_scrollLayer->children()[0]->contentBounds().height();
}

int CCScrollbarLayerImpl::CCScrollbar::maximum() const
{
    if (!m_owner->m_scrollLayer)
        return 0;
    if (orientation() == HorizontalScrollbar)
        return m_owner->m_scrollLayer->maxScrollPosition().width();
    return m_owner->m_scrollLayer->maxScrollPosition().height();
}

ScrollbarControlSize CCScrollbarLayerImpl::CCScrollbar::controlSize() const
{
    return m_owner->m_controlSize;
}

int CCScrollbarLayerImpl::CCScrollbar::lineStep() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

int CCScrollbarLayerImpl::CCScrollbar::pageStep() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::pressedPart() const
{
    return m_owner->m_pressedPart;
}

ScrollbarPart CCScrollbarLayerImpl::CCScrollbar::hoveredPart() const
{
    return m_owner->m_hoveredPart;
}

void CCScrollbarLayerImpl::CCScrollbar::styleChanged()
{
}

bool CCScrollbarLayerImpl::CCScrollbar::enabled() const
{
    return m_owner->m_enabled;
}

void CCScrollbarLayerImpl::CCScrollbar::setEnabled(bool)
{
    ASSERT_NOT_REACHED();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
