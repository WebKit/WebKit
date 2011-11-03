/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "EmptyClients.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFormElement.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "Length.h"
#include "Page.h"
#include "RenderSVGRoot.h"
#include "RenderView.h"
#include "ResourceError.h"
#include "SVGDocument.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "Settings.h"

namespace WebCore {

class SVGImageChromeClient : public EmptyChromeClient {
    WTF_MAKE_NONCOPYABLE(SVGImageChromeClient); WTF_MAKE_FAST_ALLOCATED;
public:
    SVGImageChromeClient(SVGImage* image)
        : m_image(image)
    {
    }

    virtual bool isSVGImageChromeClient() const { return true; }
    SVGImage* image() const { return m_image; }

private:
    virtual void chromeDestroyed()
    {
        m_image = 0;
    }

    virtual void invalidateContentsAndWindow(const LayoutRect& r, bool)
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

PassRefPtr<SVGImage> SVGImage::createWithDataAndSize(ImageObserver* observer, SharedBuffer* data, const IntSize& size, float zoom)
{
    ASSERT(!size.isEmpty());

    RefPtr<SVGImage> image = adoptRef(new SVGImage(0));
    image->setData(data, true);
    image->setContainerSize(size);
    image->setContainerZoom(zoom);
    image->setImageObserver(observer);
    return image.release();
}

void SVGImage::setContainerZoom(float containerZoom)
{
    if (!m_page)
        return;
    Frame* frame = m_page->mainFrame();
    frame->setPageZoomFactor(containerZoom);
}

void SVGImage::setContainerSize(const IntSize& containerSize)
{
    ASSERT(!containerSize.isEmpty());
    ASSERT(!imageObserver());
    
    if (!m_page)
        return;
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return;

    RenderSVGRoot* renderer = toRenderSVGRoot(rootElement->renderer());
    if (!renderer)
        return;
    renderer->setContainerSize(containerSize);
    frame->view()->resize(size());
}

bool SVGImage::usesContainerSize() const
{
    if (!m_page)
        return false;
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return false;
    if (RenderSVGRoot* renderer = toRenderSVGRoot(rootElement->renderer()))
        return !renderer->containerSize().isEmpty();
    return false;
}

IntSize SVGImage::size() const
{
    if (!m_page)
        return IntSize();
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return IntSize();

    RenderSVGRoot* renderer = toRenderSVGRoot(rootElement->renderer());
    if (!renderer)
        return IntSize();

    // If a container size is available it has precedence.
    IntSize containerSize = renderer->containerSize();
    if (!containerSize.isEmpty())
        return containerSize;

    // Assure that a container size is always given for a non-identity zoom level.
    ASSERT(renderer->style()->effectiveZoom() == 1);
    IntSize size = enclosingIntRect(rootElement->currentViewBoxRect()).size();
    if (!size.isEmpty())
        return size;

    // As last resort, use CSS default intrinsic size.
    return IntSize(300, 150);
}

void SVGImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator compositeOp)
{
    if (!m_page)
        return;

    FrameView* view = m_page->mainFrame()->view();

    GraphicsContextStateSaver stateSaver(*context);
    context->setCompositeOperation(compositeOp);
    context->clip(enclosingIntRect(dstRect));
    if (compositeOp != CompositeSourceOver)
        context->beginTransparencyLayer(1);

    FloatSize scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
    
    // We can only draw the entire frame, clipped to the rect we want. So compute where the top left
    // of the image would be if we were drawing without clipping, and translate accordingly.
    FloatSize topLeftOffset(srcRect.location().x() * scale.width(), srcRect.location().y() * scale.height());
    FloatPoint destOffset = dstRect.location() - topLeftOffset;

    context->translate(destOffset.x(), destOffset.y());
    context->scale(scale);

    view->resize(size());

    if (view->needsLayout())
        view->layout();

    view->paint(context, IntRect(0, 0, view->width(), view->height()));

    if (compositeOp != CompositeSourceOver)
        context->endTransparencyLayer();

    stateSaver.restore();

    if (imageObserver())
        imageObserver()->didDraw(this);
}

RenderBox* SVGImage::embeddedContentBox() const
{
    if (!m_page)
        return 0;
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return 0;
    return toRenderBox(rootElement->renderer());
}

void SVGImage::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (!m_page)
        return;
    Frame* frame = m_page->mainFrame();
    SVGSVGElement* rootElement = static_cast<SVGDocument*>(frame->document())->rootElement();
    if (!rootElement)
        return;
    RenderBox* renderer = toRenderBox(rootElement->renderer());
    if (!renderer)
        return;

    intrinsicWidth = renderer->style()->width();
    intrinsicHeight = renderer->style()->height();
    if (rootElement->preserveAspectRatio().align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
        intrinsicRatio = rootElement->currentViewBoxRect().size();
}

NativeImagePtr SVGImage::nativeImageForCurrentFrame()
{
    // FIXME: In order to support dynamic SVGs we need to have a way to invalidate this
    // frame cache, or better yet, not use a cache for tiled drawing at all, instead
    // having a tiled drawing callback (hopefully non-virtual).
    if (!m_frameCache) {
        if (!m_page)
            return 0;
        OwnPtr<ImageBuffer> buffer = ImageBuffer::create(size());
        if (!buffer) // failed to allocate image
            return 0;
        draw(buffer->context(), rect(), rect(), ColorSpaceDeviceRGB, CompositeSourceOver);
        m_frameCache = buffer->copyImage(CopyBackingStore);
    }
    return m_frameCache->nativeImageForCurrentFrame();
}

bool SVGImage::dataChanged(bool allDataReceived)
{
    // Don't do anything if is an empty image.
    if (!data()->size())
        return true;

    if (allDataReceived) {
        static FrameLoaderClient* dummyFrameLoaderClient =  new EmptyFrameLoaderClient;

        Page::PageClients pageClients;
        m_chromeClient = adoptPtr(new SVGImageChromeClient(this));
        pageClients.chromeClient = m_chromeClient.get();
#if ENABLE(CONTEXT_MENUS)
        static ContextMenuClient* dummyContextMenuClient = new EmptyContextMenuClient;
        pageClients.contextMenuClient = dummyContextMenuClient;
#endif
        static EditorClient* dummyEditorClient = new EmptyEditorClient;
        pageClients.editorClient = dummyEditorClient;
#if ENABLE(DRAG_SUPPORT)
        static DragClient* dummyDragClient = new EmptyDragClient;
        pageClients.dragClient = dummyDragClient;
#endif
        static InspectorClient* dummyInspectorClient = new EmptyInspectorClient;
        pageClients.inspectorClient = dummyInspectorClient;
#if ENABLE(DEVICE_ORIENTATION)
        static DeviceMotionClient* dummyDeviceMotionClient = new EmptyDeviceMotionClient;
        pageClients.deviceMotionClient = dummyDeviceMotionClient;
        static DeviceOrientationClient* dummyDeviceOrientationClient = new EmptyDeviceOrientationClient;
        pageClients.deviceOrientationClient = dummyDeviceOrientationClient;
#endif

        // FIXME: If this SVG ends up loading itself, we might leak the world.
        // The Cache code does not know about CachedImages holding Frames and
        // won't know to break the cycle.
        // This will become an issue when SVGImage will be able to load other
        // SVGImage objects, but we're safe now, because SVGImage can only be
        // loaded by a top-level document.
        m_page = adoptPtr(new Page(pageClients));
        m_page->settings()->setMediaEnabled(false);
        m_page->settings()->setScriptEnabled(false);
        m_page->settings()->setPluginsEnabled(false);

        RefPtr<Frame> frame = Frame::create(m_page.get(), 0, dummyFrameLoaderClient);
        frame->setView(FrameView::create(frame.get()));
        frame->init();
        FrameLoader* loader = frame->loader();
        loader->forceSandboxFlags(SandboxAll);

        frame->view()->setCanHaveScrollbars(false); // SVG Images will always synthesize a viewBox, if it's not available, and thus never see scrollbars.
        frame->view()->setTransparent(true); // SVG Images are transparent.

        ASSERT(loader->activeDocumentLoader()); // DocumentLoader should have been created by frame->init().
        loader->activeDocumentLoader()->writer()->setMIMEType("image/svg+xml");
        loader->activeDocumentLoader()->writer()->begin(KURL()); // create the empty document
        loader->activeDocumentLoader()->writer()->addData(data()->data(), data()->size());
        loader->activeDocumentLoader()->writer()->end();
    }

    return m_page;
}

String SVGImage::filenameExtension() const
{
    return "svg";
}

}

#endif // ENABLE(SVG)
