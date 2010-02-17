/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "FileChooser.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFormElement.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "Page.h"
#include "RenderView.h"
#include "ResourceError.h"
#include "SVGDocument.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "Settings.h"

// Moving this #include above FrameLoader.h causes the Windows build to fail due to warnings about
// alignment in Timer<FrameLoader>. It seems that the definition of EmptyFrameLoaderClient is what
// causes this (removing that definition fixes the warnings), but it isn't clear why.
#include "EmptyClients.h"

namespace WebCore {

class SVGImageChromeClient : public EmptyChromeClient, public Noncopyable {
public:
    SVGImageChromeClient(SVGImage* image)
        : m_image(image)
    {
    }

    SVGImage* image() const { return m_image; }
    
private:
    virtual void chromeDestroyed()
    {
        m_image = 0;
    }

    virtual void repaint(const IntRect& r, bool, bool, bool)
    {
        if (m_image && m_image->imageObserver())
            m_image->imageObserver()->changedInRect(m_image, r);
    }

    SVGImage* m_image;
};

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
{
}

SVGImage::~SVGImage()
{
    if (m_page) {
        m_page->mainFrame()->loader()->frameDetached(); // Break both the loader and view references to the frame

        // Clear explicitly because we want to delete the page before the ChromeClient.
        // FIXME: I believe that's already guaranteed by C++ object destruction rules,
        // so this may matter only for the assertion below.
        m_page.clear();
    }

    // Verify that page teardown destroyed the Chrome
    ASSERT(!m_chromeClient || !m_chromeClient->image());
}

void SVGImage::setContainerSize(const IntSize& containerSize)
{
    if (containerSize.isEmpty())
        return;

    if (!m_page)
        return;
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return;

    rootElement->setContainerSize(containerSize);
}

bool SVGImage::usesContainerSize() const
{
    if (!m_page)
        return false;
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->hasSetContainerSize();
}

IntSize SVGImage::size() const
{
    if (!m_page)
        return IntSize();
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
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
    if (!m_page)
        return false;
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_page->mainFrame()->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->width().unitType() == LengthTypePercentage;
}

bool SVGImage::hasRelativeHeight() const
{
    if (!m_page)
        return false;
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(m_page->mainFrame()->document())->rootElement();
    if (!rootElement)
        return false;

    return rootElement->height().unitType() == LengthTypePercentage;
}

void SVGImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator compositeOp)
{
    if (!m_page)
        return;

    FrameView* view = m_page->mainFrame()->view();

    context->save();
    context->setCompositeOperation(compositeOp);
    context->clip(enclosingIntRect(dstRect));
    if (compositeOp != CompositeSourceOver)
        context->beginTransparencyLayer(1);
    context->translate(dstRect.location().x(), dstRect.location().y());
    context->scale(FloatSize(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height()));

    view->resize(size());

    if (view->needsLayout())
        view->layout();
    view->paint(context, IntRect(0, 0, view->width(), view->height()));

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
        if (!m_page)
            return 0;
        m_frameCache = ImageBuffer::create(size());
        if (!m_frameCache) // failed to allocate image
            return 0;
        draw(m_frameCache->context(), rect(), rect(), DeviceColorSpace, CompositeSourceOver);
    }
    return m_frameCache->image()->nativeImageForCurrentFrame();
}

bool SVGImage::dataChanged(bool allDataReceived)
{
    // Don't do anything if is an empty image.
    if (!data()->size())
        return true;

    if (allDataReceived) {
        static FrameLoaderClient* dummyFrameLoaderClient =  new EmptyFrameLoaderClient;
        static EditorClient* dummyEditorClient = new EmptyEditorClient;
#if ENABLE(CONTEXT_MENUS)
        static ContextMenuClient* dummyContextMenuClient = new EmptyContextMenuClient;
#else
        static ContextMenuClient* dummyContextMenuClient = 0;
#endif
#if ENABLE(DRAG_SUPPORT)
        static DragClient* dummyDragClient = new EmptyDragClient;
#else
        static DragClient* dummyDragClient = 0;
#endif
        static InspectorClient* dummyInspectorClient = new EmptyInspectorClient;

        m_chromeClient.set(new SVGImageChromeClient(this));
        
        // FIXME: If this SVG ends up loading itself, we might leak the world.
        // The comment said that the Cache code does not know about CachedImages
        // holding Frames and won't know to break the cycle. But 
        m_page.set(new Page(m_chromeClient.get(), dummyContextMenuClient, dummyEditorClient, dummyDragClient, dummyInspectorClient, 0, 0));
        m_page->settings()->setJavaScriptEnabled(false);
        m_page->settings()->setPluginsEnabled(false);

        RefPtr<Frame> frame = Frame::create(m_page.get(), 0, dummyFrameLoaderClient);
        frame->setView(FrameView::create(frame.get()));
        frame->init();
        ResourceRequest fakeRequest(KURL(ParsedURLString, ""));
        FrameLoader* loader = frame->loader();
        loader->load(fakeRequest, false); // Make sure the DocumentLoader is created
        loader->policyChecker()->cancelCheck(); // cancel any policy checks
        loader->commitProvisionalLoad(0);
        loader->setResponseMIMEType("image/svg+xml");
        loader->begin(KURL()); // create the empty document
        loader->write(data()->data(), data()->size());
        loader->end();
        frame->view()->setTransparent(true); // SVG Images are transparent.
    }

    return m_page;
}

String SVGImage::filenameExtension() const
{
    return "svg";
}

}

#endif // ENABLE(SVG)
