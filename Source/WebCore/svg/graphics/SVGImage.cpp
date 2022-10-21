/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "SVGImage.h"

#include "CacheStorageProvider.h"
#include "Chrome.h"
#include "CommonVM.h"
#include "DOMWindow.h"
#include "DocumentLoader.h"
#include "DocumentSVG.h"
#include "EditorClient.h"
#include "ElementIterator.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "JSDOMWindowBase.h"
#include "LegacyRenderSVGRoot.h"
#include "LibWebRTCProvider.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "RenderSVGRoot.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFEImageElement.h"
#include "SVGForeignObjectElement.h"
#include "SVGImageClients.h"
#include "SVGImageElement.h"
#include "SVGSVGElement.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "SocketProvider.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

namespace WebCore {

SVGImage::SVGImage(ImageObserver& observer)
    : Image(&observer)
    , m_startAnimationTimer(*this, &SVGImage::startAnimationTimerFired)
{
}

SVGImage::~SVGImage()
{
    if (m_page) {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        // Clear m_page, so that SVGImageChromeClient knows we're destructed.
        m_page = nullptr;
    }

    // Verify that page teardown destroyed the Chrome
    ASSERT(!m_chromeClient || !m_chromeClient->image());
}

inline RefPtr<SVGSVGElement> SVGImage::rootElement() const
{
    if (!m_page)
        return nullptr;
    return DocumentSVG::rootElement(*m_page->mainFrame().document());
}

bool SVGImage::hasSingleSecurityOrigin() const
{
    auto rootElement = this->rootElement();
    if (!rootElement)
        return true;

    // FIXME: Once foreignObject elements within SVG images are updated to not leak cross-origin data
    // (e.g., visited links, spellcheck) we can remove the SVGForeignObjectElement check here and
    // research if we can remove the Image::hasSingleSecurityOrigin mechanism entirely.
    for (auto& element : descendantsOfType<SVGElement>(*rootElement)) {
        if (is<SVGForeignObjectElement>(element))
            return false;
        if (is<SVGImageElement>(element)) {
            if (!downcast<SVGImageElement>(element).hasSingleSecurityOrigin())
                return false;
        } else if (is<SVGFEImageElement>(element)) {
            if (!downcast<SVGFEImageElement>(element).hasSingleSecurityOrigin())
                return false;
        }
    }

    // Because SVG image rendering disallows external resources and links,
    // these images effectively are restricted to a single security origin.
    return true;
}

void SVGImage::setContainerSize(const FloatSize& size)
{
    if (!usesContainerSize())
        return;

    auto rootElement = this->rootElement();
    if (!rootElement || !rootElement->renderer() || !rootElement->renderer()->isSVGRootOrLegacySVGRoot())
        return;

    RefPtr view = frameView();
    view->resize(containerSize());

    if (auto* renderer = dynamicDowncast<LegacyRenderSVGRoot>(rootElement->renderer())) {
        renderer->setContainerSize(IntSize(size));
        return;
    }

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (auto* renderer = dynamicDowncast<RenderSVGRoot>(rootElement->renderer())) {
        renderer->setContainerSize(IntSize(size));
        return;
    }
#endif
}

IntSize SVGImage::containerSize() const
{
    auto rootElement = this->rootElement();
    if (!rootElement || !rootElement->renderer() || !rootElement->renderer()->isSVGRootOrLegacySVGRoot())
        return { };

    // If a container size is available it has precedence.
    auto computeContainerSize = [&]() -> IntSize {
        if (auto* renderer = dynamicDowncast<LegacyRenderSVGRoot>(rootElement->renderer()))
            return renderer->containerSize();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* renderer = dynamicDowncast<RenderSVGRoot>(rootElement->renderer()))
            return renderer->containerSize();
#endif

        return { };
    };

    auto containerSize = computeContainerSize();
    if (!containerSize.isEmpty())
        return containerSize;

    // Assure that a container size is always given for a non-identity zoom level.
    ASSERT(rootElement->renderer()->style().effectiveZoom() == 1);

    FloatSize currentSize;
    if (rootElement->hasIntrinsicWidth() && rootElement->hasIntrinsicHeight())
        currentSize = rootElement->currentViewportSizeExcludingZoom();
    else
        currentSize = rootElement->currentViewBoxRect().size();

    // Use the default CSS intrinsic size if the above failed.
    if (currentSize.isEmpty())
        return IntSize(300, 150);

    return IntSize(currentSize);
}

ImageDrawResult SVGImage::drawForContainer(GraphicsContext& context, const FloatSize containerSize, float containerZoom, const URL& initialFragmentURL, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    if (!m_page)
        return ImageDrawResult::DidNothing;

    ImageObserver* observer = imageObserver();
    ASSERT(observer);

    // Temporarily reset image observer, we don't want to receive any changeInRect() calls due to this relayout.
    setImageObserver(nullptr);

    IntSize roundedContainerSize = roundedIntSize(containerSize);
    setContainerSize(roundedContainerSize);

    FloatRect scaledSrc = srcRect;
    scaledSrc.scale(1 / containerZoom);

    // Compensate for the container size rounding by adjusting the source rect.
    FloatSize adjustedSrcSize = scaledSrc.size();
    adjustedSrcSize.scale(roundedContainerSize.width() / containerSize.width(), roundedContainerSize.height() / containerSize.height());
    scaledSrc.setSize(adjustedSrcSize);

    frameView()->scrollToFragment(initialFragmentURL);

    ImageDrawResult result = draw(context, dstRect, scaledSrc, options);

    setImageObserver(observer);
    return result;
}

RefPtr<NativeImage> SVGImage::nativeImage(const DestinationColorSpace& colorSpace)
{
    if (!m_page)
        return nullptr;

    OptionSet<ImageBufferOptions> bufferOptions;
    if (m_page->settings().acceleratedDrawingEnabled())
        bufferOptions.add(ImageBufferOptions::Accelerated);

    HostWindow* hostWindow = nullptr;
    if (auto contentRenderer = embeddedContentBox())
        hostWindow = contentRenderer->hostWindow();

    auto imageBuffer = ImageBuffer::create(size(), RenderingPurpose::DOM, 1, colorSpace, PixelFormat::BGRA8, bufferOptions, { hostWindow });
    if (!imageBuffer)
        return nullptr;

    ImageObserver* observer = imageObserver();
    setImageObserver(nullptr);
    setContainerSize(size());

    imageBuffer->context().drawImage(*this, FloatPoint(0, 0));

    setImageObserver(observer);
    return ImageBuffer::sinkIntoNativeImage(WTFMove(imageBuffer));
}

void SVGImage::drawPatternForContainer(GraphicsContext& context, const FloatSize& containerSize, float containerZoom, const URL& initialFragmentURL, const FloatRect& srcRect,
    const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const FloatRect& dstRect, const ImagePaintingOptions& options)
{
    FloatRect zoomedContainerRect = FloatRect(FloatPoint(), containerSize);
    zoomedContainerRect.scale(containerZoom);

    // The ImageBuffer size needs to be scaled to match the final resolution.
    AffineTransform transform = context.getCTM();
    FloatSize imageBufferScale = FloatSize(transform.xScale(), transform.yScale());
    ASSERT(imageBufferScale.width());
    ASSERT(imageBufferScale.height());

    FloatRect imageBufferSize = zoomedContainerRect;
    imageBufferSize.scale(imageBufferScale.width(), imageBufferScale.height());

    auto buffer = context.createImageBuffer(expandedIntSize(imageBufferSize.size()));
    if (!buffer) // Failed to allocate buffer.
        return;
    drawForContainer(buffer->context(), containerSize, containerZoom, initialFragmentURL, imageBufferSize, zoomedContainerRect);
    if (context.drawLuminanceMask())
        buffer->convertToLuminanceMask();

    RefPtr<Image> image = ImageBuffer::sinkIntoImage(WTFMove(buffer), PreserveResolution::Yes);
    if (!image)
        return;

    // Adjust the source rect and transform due to the image buffer's scaling.
    FloatRect scaledSrcRect = srcRect;
    scaledSrcRect.scale(imageBufferScale.width(), imageBufferScale.height());
    AffineTransform unscaledPatternTransform(patternTransform);
    unscaledPatternTransform.scale(1 / imageBufferScale.width(), 1 / imageBufferScale.height());

    context.setDrawLuminanceMask(false);
    image->drawPattern(context, dstRect, scaledSrcRect, unscaledPatternTransform, phase, spacing, options);
}

ImageDrawResult SVGImage::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    if (!m_page)
        return ImageDrawResult::DidNothing;

    RefPtr view = frameView();
    ASSERT(view);

    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    context.clip(enclosingIntRect(dstRect));

    float alpha = context.alpha();
    bool compositingRequiresTransparencyLayer = options.compositeOperator() != CompositeOperator::SourceOver || options.blendMode() != BlendMode::Normal || alpha < 1;
    if (compositingRequiresTransparencyLayer) {
        context.beginTransparencyLayer(alpha);
        context.setCompositeOperation(CompositeOperator::SourceOver, BlendMode::Normal);
    }

    // FIXME: We should honor options.orientation(), since ImageBitmap's flipY handling relies on it. https://bugs.webkit.org/show_bug.cgi?id=231001
    FloatSize scale(dstRect.size() / srcRect.size());
    
    // We can only draw the entire frame, clipped to the rect we want. So compute where the top left
    // of the image would be if we were drawing without clipping, and translate accordingly.
    FloatSize topLeftOffset(srcRect.location().x() * scale.width(), srcRect.location().y() * scale.height());
    FloatPoint destOffset = dstRect.location() - topLeftOffset;

    context.translate(destOffset);
    context.scale(scale);

    view->resize(containerSize());

    {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        if (view->needsLayout())
            view->layoutContext().layout();
    }

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(view->useDarkAppearance());
#endif

    view->paint(context, intersection(context.clipBounds(), enclosingIntRect(srcRect)));

    if (compositingRequiresTransparencyLayer)
        context.endTransparencyLayer();

    stateSaver.restore();

    if (imageObserver())
        imageObserver()->didDraw(*this);

    return ImageDrawResult::DidDraw;
}

RenderBox* SVGImage::embeddedContentBox() const
{
    auto rootElement = this->rootElement();
    if (!rootElement)
        return nullptr;
    return downcast<RenderBox>(rootElement->renderer());
}

FrameView* SVGImage::frameView() const
{
    if (!m_page)
        return nullptr;
    return m_page->mainFrame().view();
}

bool SVGImage::hasRelativeWidth() const
{
    auto rootElement = this->rootElement();
    if (!rootElement)
        return false;
    return rootElement->intrinsicWidth().isPercentOrCalculated();
}

bool SVGImage::hasRelativeHeight() const
{
    auto rootElement = this->rootElement();
    if (!rootElement)
        return false;
    return rootElement->intrinsicHeight().isPercentOrCalculated();
}

void SVGImage::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    auto rootElement = this->rootElement();
    if (!rootElement)
        return;

    intrinsicWidth = rootElement->intrinsicWidth();
    intrinsicHeight = rootElement->intrinsicHeight();
    if (rootElement->preserveAspectRatio().align() == SVGPreserveAspectRatioValue::SVG_PRESERVEASPECTRATIO_NONE)
        return;

    intrinsicRatio = rootElement->viewBox().size();
    if (intrinsicRatio.isEmpty() && intrinsicWidth.isFixed() && intrinsicHeight.isFixed())
        intrinsicRatio = FloatSize(floatValueForLength(intrinsicWidth, 0), floatValueForLength(intrinsicHeight, 0));
}

void SVGImage::startAnimationTimerFired()
{
    startAnimation();
}

void SVGImage::scheduleStartAnimation()
{
    auto rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    m_startAnimationTimer.startOneShot(0_s);
}

void SVGImage::startAnimation()
{
    auto rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    rootElement->unpauseAnimations();
    rootElement->setCurrentTime(0);
}

void SVGImage::stopAnimation()
{
    m_startAnimationTimer.stop();
    auto rootElement = this->rootElement();
    if (!rootElement)
        return;
    rootElement->pauseAnimations();
}

void SVGImage::resetAnimation()
{
    stopAnimation();
}

bool SVGImage::isAnimating() const
{
    auto rootElement = this->rootElement();
    if (!rootElement)
        return false;
    return rootElement->hasActiveAnimation();
}

void SVGImage::reportApproximateMemoryCost() const
{
    RefPtr document = m_page->mainFrame().document();
    size_t decodedImageMemoryCost = 0;

    for (RefPtr<Node> node = document; node; node = NodeTraversal::next(*node))
        decodedImageMemoryCost += node->approximateMemoryCost();

    JSC::VM& vm = commonVM();
    JSC::JSLockHolder lock(vm);
    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    vm.heap.deprecatedReportExtraMemory(decodedImageMemoryCost + data()->size());
}

EncodedDataStatus SVGImage::dataChanged(bool allDataReceived)
{
    // Don't do anything; it is an empty image.
    if (!data()->size())
        return EncodedDataStatus::Complete;

    if (allDataReceived) {
        auto pageConfiguration = pageConfigurationWithEmptyClients(PAL::SessionID::defaultSessionID());
        m_chromeClient = makeUnique<SVGImageChromeClient>(this);
        pageConfiguration.chromeClient = m_chromeClient.get();

        // FIXME: If this SVG ends up loading itself, we might leak the world.
        // The Cache code does not know about CachedImages holding Frames and
        // won't know to break the cycle.
        // This will become an issue when SVGImage will be able to load other
        // SVGImage objects, but we're safe now, because SVGImage can only be
        // loaded by a top-level document.
        m_page = makeUnique<Page>(WTFMove(pageConfiguration));
#if ENABLE(VIDEO)
        m_page->settings().setMediaEnabled(false);
#endif
        m_page->settings().setScriptEnabled(false);
        m_page->settings().setPluginsEnabled(false);
        m_page->settings().setAcceleratedCompositingEnabled(false);
        m_page->settings().setShouldAllowUserInstalledFonts(false);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* observer = imageObserver())
            m_page->settings().setLayerBasedSVGEngineEnabled(observer->layerBasedSVGEngineEnabled());
#endif

        Frame& frame = m_page->mainFrame();
        frame.setView(FrameView::create(frame));
        frame.init();
        FrameLoader& loader = frame.loader();
        loader.forceSandboxFlags(SandboxAll);

        frame.view()->setCanHaveScrollbars(false); // SVG Images will always synthesize a viewBox, if it's not available, and thus never see scrollbars.
        frame.view()->setTransparent(true); // SVG Images are transparent.

        ASSERT(loader.activeDocumentLoader()); // DocumentLoader should have been created by frame->init().
        loader.activeDocumentLoader()->writer().setMIMEType("image/svg+xml"_s);
        loader.activeDocumentLoader()->writer().begin(URL()); // create the empty document
        data()->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
            loader.activeDocumentLoader()->writer().addData(buffer);
        });
        loader.activeDocumentLoader()->writer().end();

        frame.document()->updateLayoutIgnorePendingStylesheets();

        // Set the intrinsic size before a container size is available.
        m_intrinsicSize = containerSize();
        reportApproximateMemoryCost();
    }

    return m_page ? EncodedDataStatus::Complete : EncodedDataStatus::Unknown;
}

String SVGImage::filenameExtension() const
{
    return "svg"_s;
}

bool isInSVGImage(const Element* element)
{
    ASSERT(element);

    Page* page = element->document().page();
    if (!page)
        return false;

    return page->chrome().client().isSVGImageChromeClient();
}

}
