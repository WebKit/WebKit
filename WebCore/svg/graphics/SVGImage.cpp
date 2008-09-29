/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#if ENABLE(SVG)
#include "SVGImage.h"

#include "CachedPage.h"
#include "DocumentLoader.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderView.h"
#include "ResourceError.h"
#include "SVGDocument.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "Settings.h"

#include "EmptyClients.h"

namespace WebCore {

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
    , m_document(0)
    , m_frame(0)
{
}

SVGImage::~SVGImage()
{
    if (m_frame)
        m_frame->loader()->frameDetached(); // Break both the loader and view references to the frame
}

void SVGImage::setContainerSize(const IntSize& containerSize)
{
    if (containerSize.width() <= 0 || containerSize.height() <= 0)
        return;

    if (!m_frame || !m_frame->document())
        return;
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return;

    rootElement->setContainerSize(containerSize);
}

bool SVGImage::usesContainerSize() const
{
    if (!m_frame || !m_frame->document())
        return false;
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->hasSetContainerSize();
}

IntSize SVGImage::size() const
{
    if (!m_frame || !m_frame->document())
        return IntSize();
    
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return IntSize();
    
    SVGLength width = rootElement->width();
    SVGLength height = rootElement->height();
    
    IntSize svgSize;
    if (width.unitType() == LengthTypePercentage) 
        svgSize.setWidth(rootElement->relativeWidthValue());
    else
        svgSize.setWidth(static_cast<int>(width.value(rootElement)));

    if (height.unitType() == LengthTypePercentage) 
        svgSize.setHeight(rootElement->relativeHeightValue());
    else
        svgSize.setHeight(static_cast<int>(height.value(rootElement)));

    return svgSize;
}

bool SVGImage::hasRelativeWidth() const
{
    if (!m_frame || !m_frame->document())
        return false;
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->width().unitType() == LengthTypePercentage;
}

bool SVGImage::hasRelativeHeight() const
{
    if (!m_frame || !m_frame->document())
        return false;
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->height().unitType() == LengthTypePercentage;
}

void SVGImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator compositeOp)
{
    if (!m_frame)
        return;
    
    context->save();
    context->setCompositeOperation(compositeOp);
    context->clip(enclosingIntRect(dstRect));
    if (compositeOp != CompositeSourceOver)
        context->beginTransparencyLayer(1.0f);
    context->translate(dstRect.location().x(), dstRect.location().y());
    context->scale(FloatSize(dstRect.width()/srcRect.width(), dstRect.height()/srcRect.height()));
    
    if (m_frame->view()->needsLayout())
        m_frame->view()->layout();
    m_frame->paint(context, enclosingIntRect(srcRect));

    if (compositeOp != CompositeSourceOver)
        context->endTransparencyLayer();

    context->restore();

    if (imageObserver())
        imageObserver()->didDraw(this);
}

NativeImagePtr SVGImage::nativeImageForCurrentFrame()
{
    // FIXME: In order to support dynamic SVGs we need to have a way to invalidate this
    // frame cache, or better yet, not use a cache for tiled drawing at all, instead
    // having a tiled drawing callback (hopefully non-virtual).
    if (!m_frameCache) {
        m_frameCache.set(ImageBuffer::create(size(), false).release());
        if (!m_frameCache) // failed to allocate image
            return 0;
        renderSubtreeToImage(m_frameCache.get(), m_frame->contentRenderer());
    }
    return m_frameCache->image()->nativeImageForCurrentFrame();
}

bool SVGImage::dataChanged(bool allDataReceived)
{
    int length = m_data->size();
    if (!length) // if this was an empty image
        return true;
    
    if (allDataReceived) {
        m_frame = FrameLoader::createDummyFrame();
        m_frame->loader()->fakeLoad(m_data.get(), "image/svg+xml");
        m_frame->view()->setTransparent(true); // SVG Images are transparent.
    }
    return m_frame ? m_frame->view() : 0;
}

}

#endif // ENABLE(SVG)
