/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
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

#include "CachedPage.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "NotImplemented.h"
#include "Page.h"
#include "ResourceError.h"
#include "SVGDocument.h"
#include "SVGImage.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "Settings.h"

#include "SVGImageEmptyClients.h"

namespace WebCore {

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
    , m_document(0)
    , m_page(0)
    , m_frame(0)
    , m_frameView(0)
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

    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return;

    rootElement->setContainerSize(containerSize);
}

bool SVGImage::usesContainerSize() const
{
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
        svgSize.setWidth(static_cast<int>(width.value()));

    if (height.unitType() == LengthTypePercentage) 
        svgSize.setHeight(rootElement->relativeHeightValue());
    else
        svgSize.setHeight(static_cast<int>(height.value()));

    return svgSize;
}

bool SVGImage::hasRelativeWidth() const
{
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_frame->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->width().unitType() == LengthTypePercentage;
}

bool SVGImage::hasRelativeHeight() const
{
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
    context->clip(enclosingIntRect(dstRect));
    context->translate(dstRect.location().x(), dstRect.location().y());
    context->scale(FloatSize(dstRect.width()/srcRect.width(), dstRect.height()/srcRect.height()));
    m_frame->paint(context, enclosingIntRect(srcRect));
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
        renderSubtreeToImage(m_frameCache.get(), m_frame->renderer());
    }
#if PLATFORM(CG)
    return m_frameCache->cgImage();
#elif PLATFORM(QT)
    return m_frameCache->pixmap();
#elif PLATFORM(CAIRO)
    return m_frameCache->surface();
#else
    notImplemented();
    return 0;
#endif
}

bool SVGImage::dataChanged(bool allDataReceived)
{
    int length = m_data->size();
    if (!length) // if this was an empty image
        return true;
    
    if (allDataReceived) {
        static ChromeClient* dummyChromeClient = new SVGEmptyChromeClient;
        static FrameLoaderClient* dummyFrameLoaderClient =  new SVGEmptyFrameLoaderClient;
        static EditorClient* dummyEditorClient = new SVGEmptyEditorClient;
        static ContextMenuClient* dummyContextMenuClient = new SVGEmptyContextMenuClient;
        static DragClient* dummyDragClient = new SVGEmptyDragClient;
        static InspectorClient* dummyInspectorClient = new SVGEmptyInspectorClient;

        // FIXME: If this SVG ends up loading itself, we'll leak this Frame (and associated DOM & render trees).
        // The Cache code does not know about CachedImages holding Frames and won't know to break the cycle.
        m_page.set(new Page(dummyChromeClient, dummyContextMenuClient, dummyEditorClient, dummyDragClient, dummyInspectorClient));
        m_page->settings()->setJavaScriptEnabled(false);

        m_frame = new Frame(m_page.get(), 0, dummyFrameLoaderClient);
        m_frame->init();
        m_frameView = new FrameView(m_frame.get());
        m_frameView->deref(); // FIXME: FrameView starts with a refcount of 1
        m_frame->setView(m_frameView.get());
        ResourceRequest fakeRequest(KURL(""));
        m_frame->loader()->load(fakeRequest); // Make sure the DocumentLoader is created
        m_frame->loader()->cancelContentPolicyCheck(); // cancel any policy checks
        m_frame->loader()->commitProvisionalLoad(0);
        m_frame->loader()->setResponseMIMEType("image/svg+xml");
        m_frame->loader()->begin("placeholder.svg"); // create the empty document
        m_frame->loader()->write(m_data->data(), m_data->size());
        m_frame->loader()->end();
    }
    return m_frameView;
}

}

#endif // ENABLE(SVG)
