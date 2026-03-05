/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "ContainerNodeInlines.h"
#include "DOMParser.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentSVG.h"
#include "DocumentView.h"
#include "EditorClient.h"
#include "FrameLoader.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "JSDOMWindowBase.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "NativeImage.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "RenderSVGRoot.h"
#include "RenderStyle+GettersInlines.h"
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
#include "TypedElementDescendantIteratorInlines.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

namespace WebCore {

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
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
}

RefPtr<SVGSVGElement> SVGImage::rootElement() const
{
    if (!m_page)
        return nullptr;

    RefPtr localMainFrame = m_page->localMainFrame();
    if (!localMainFrame)
        return nullptr;

    return DocumentSVG::rootElement(*protect(localMainFrame->document()));
}

bool SVGImage::renderingTaintsOrigin() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return false;

    // FIXME: Once foreignObject elements within SVG images are updated to not leak cross-origin data
    // (e.g., visited links, spellcheck) we can remove the SVGForeignObjectElement check here and
    // research if we can remove the Image::renderingTaintsOrigin mechanism entirely.
    for (Ref element : descendantsOfType<SVGElement>(*rootElement)) {
        if (is<SVGForeignObjectElement>(element.get()))
            return true;
        if (RefPtr svgImage = dynamicDowncast<SVGImageElement>(element.get())) {
            if (svgImage->renderingTaintsOrigin())
                return true;
        } else if (RefPtr svgFEImage = dynamicDowncast<SVGFEImageElement>(element.get())) {
            if (svgFEImage->renderingTaintsOrigin())
                return true;
        }
    }

    // Because SVG image rendering disallows external resources and links,
    // these images effectively are restricted to a single security origin.
    return false;
}

void SVGImage::setContainerSize(const FloatSize& size)
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->renderer() || !rootElement->renderer()->isRenderOrLegacyRenderSVGRoot())
        return;

    RefPtr view = frameView();
    view->resize(containerSize());

    if (CheckedPtr renderer = dynamicDowncast<LegacyRenderSVGRoot>(rootElement->renderer())) {
        renderer->setContainerSize(IntSize(size));
        return;
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderSVGRoot>(rootElement->renderer())) {
        renderer->setContainerSize(IntSize(size));
        return;
    }
}

IntSize SVGImage::containerSize() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->renderer() || !rootElement->renderer()->isRenderOrLegacyRenderSVGRoot())
        return { };

    // If a container size is available it has precedence.
    auto computeContainerSize = [&]() -> IntSize {
        if (CheckedPtr renderer = dynamicDowncast<LegacyRenderSVGRoot>(rootElement->renderer()))
            return renderer->containerSize();

        if (CheckedPtr renderer = dynamicDowncast<RenderSVGRoot>(rootElement->renderer()))
            return renderer->containerSize();

        return { };
    };

    auto containerSize = computeContainerSize();
    if (!containerSize.isEmpty())
        return containerSize;

    // Assure that a container size is always given for a non-identity zoom level.
    ASSERT(rootElement->renderer()->style().usedZoom() == 1);

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

ImageDrawResult SVGImage::drawForContainer(GraphicsContext& context, const FloatSize containerSize, float containerZoom, const URL& initialFragmentURL, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    if (!m_page)
        return ImageDrawResult::DidNothing;

    RefPtr observer = imageObserver();

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

    protect(frameView())->scrollToFragment(initialFragmentURL);

    ImageDrawResult result = draw(context, dstRect, scaledSrc, options);

    setImageObserver(WTF::move(observer));
    return result;
}

bool SVGImage::hasHDRContent() const
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (!m_page)
        return false;

    if (RefPtr localTopDocument = m_page->localTopDocument())
        return localTopDocument->hasHDRContent();
#endif
    return false;
}

RefPtr<NativeImage> SVGImage::nativeImage(const DestinationColorSpace& colorSpace)
{
    return nativeImage(size(), colorSpace);
}

RefPtr<NativeImage> SVGImage::nativeImage(const FloatSize& size, const DestinationColorSpace& colorSpace)
{
    if (!m_page)
        return nullptr;

    auto renderingMode = m_page->settings().acceleratedDrawingEnabled() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

    HostWindow* hostWindow = nullptr;
    if (CheckedPtr contentRenderer = embeddedContentBox())
        hostWindow = contentRenderer->hostWindow();

    RefPtr imageBuffer = ImageBuffer::create(size, renderingMode, RenderingPurpose::DOM, 1, colorSpace, PixelFormat::BGRA8, hostWindow);
    if (!imageBuffer)
        return nullptr;

    RefPtr observer = imageObserver();
    setImageObserver(nullptr);
    setContainerSize(size);

    imageBuffer->context().drawImage(*this, FloatPoint(0, 0));

    setImageObserver(WTF::move(observer));
    return ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
}

void SVGImage::drawPatternForContainer(GraphicsContext& context, const FloatSize& containerSize, float containerZoom, const URL& initialFragmentURL, const FloatRect& srcRect,
    const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const FloatRect& dstRect, ImagePaintingOptions options)
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

    RefPtr buffer = context.createImageBuffer(expandedIntSize(imageBufferSize.size()));
    if (!buffer)
        return;

    drawForContainer(buffer->context(), containerSize, containerZoom, initialFragmentURL, imageBufferSize, zoomedContainerRect);
    if (context.drawLuminanceMask())
        buffer->convertToLuminanceMask();

    // Adjust the source rect and transform due to the image buffer's scaling.
    FloatRect scaledSrcRect = srcRect;
    scaledSrcRect.scale(imageBufferScale.width(), imageBufferScale.height());
    AffineTransform unscaledPatternTransform(patternTransform);
    unscaledPatternTransform.scale(1 / imageBufferScale.width(), 1 / imageBufferScale.height());

    context.setDrawLuminanceMask(false);
    context.drawPattern(*buffer, dstRect, scaledSrcRect, unscaledPatternTransform, phase, spacing, options);
}

ImageDrawResult SVGImage::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions options)
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

    auto orientation = options.orientation();
    // SVG images don't have intrinsic orientation metadata like EXIF, so FromImage defaults to None.
    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = ImageOrientation::Orientation::None;

    FloatSize scale(dstRect.size() / srcRect.size());
    
    // We can only draw the entire frame, clipped to the rect we want. So compute where the top left
    // of the image would be if we were drawing without clipping, and translate accordingly.
    FloatSize topLeftOffset(srcRect.location().x() * scale.width(), srcRect.location().y() * scale.height());
    FloatPoint destOffset = dstRect.location() - topLeftOffset;

    context.translate(destOffset);
    context.scale(scale);

    // Apply orientation transformation if needed.
    if (orientation != ImageOrientation::Orientation::None) {
        auto containerSizeForTransform = containerSize();
        auto orientationTransform = ImageOrientation(orientation).transformFromDefault(FloatSize(containerSizeForTransform));
        context.concatCTM(orientationTransform);
    }

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

    if (RefPtr observer = imageObserver())
        observer->didDraw(*this);

    return ImageDrawResult::DidDraw;
}

RenderBox* SVGImage::embeddedContentBox() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return nullptr;
    return downcast<RenderBox>(rootElement->renderer());
}

LocalFrameView* SVGImage::frameView() const
{
    if (!m_page)
        return nullptr;

    RefPtr localMainFrame = m_page->localMainFrame();
    if (!localMainFrame)
        return nullptr;

    return localMainFrame->view();
}

bool SVGImage::hasRelativeWidth() const
{
    // FIXME: This seems wrong.
    return false;
}

bool SVGImage::hasRelativeHeight() const
{
    // FIXME: This seems wrong.
    return false;
}

void SVGImage::computeIntrinsicDimensions(float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio)
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return;

    intrinsicWidth = rootElement->intrinsicWidth();
    intrinsicHeight = rootElement->intrinsicHeight();

    if (rootElement->preserveAspectRatio().align() == SVGPreserveAspectRatioValue::SVG_PRESERVEASPECTRATIO_NONE)
        return;

    intrinsicRatio = rootElement->viewBox().size();
    if (intrinsicRatio.isEmpty())
        intrinsicRatio = FloatSize { intrinsicWidth, intrinsicHeight };
}

void SVGImage::startAnimationTimerFired()
{
    startAnimation();
}

void SVGImage::scheduleStartAnimation()
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    m_startAnimationTimer.startOneShot(0_s);
}

void SVGImage::startAnimation()
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    rootElement->unpauseAnimations();
    rootElement->setCurrentTime(0);
}

void SVGImage::resumeAnimation()
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    rootElement->unpauseAnimations();
}

void SVGImage::stopAnimation()
{
    m_startAnimationTimer.stop();
    RefPtr rootElement = this->rootElement();
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
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return false;
    return rootElement->hasActiveAnimation();
}

void SVGImage::reportApproximateMemoryCost() const
{
    RefPtr localTopDocument = m_page->localTopDocument();
    if (!localTopDocument)
        return;

    size_t decodedImageMemoryCost = 0;

    for (RefPtr<Node> node = localTopDocument; node; node = NodeTraversal::next(*node))
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
        auto pageConfiguration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
        pageConfiguration.chromeClient = makeUniqueRef<SVGImageChromeClient>(this);

        // FIXME: If this SVG ends up loading itself, we might leak the world.
        // The Cache code does not know about CachedImages holding Frames and
        // won't know to break the cycle.
        // This will become an issue when SVGImage will be able to load other
        // SVGImage objects, but we're safe now, because SVGImage can only be
        // loaded by a top-level document.
        m_page = Page::create(WTF::move(pageConfiguration));
#if ENABLE(VIDEO)
        m_page->settings().setMediaEnabled(false);
#endif
        m_page->settings().setScriptEnabled(false);
        m_page->settings().setAcceleratedCompositingEnabled(false);
        m_page->settings().setShouldAllowUserInstalledFonts(false);

        if (RefPtr observer = imageObserver()) {
            if (RefPtr parentSettings = observer->settings()) {
                m_page->settings().setLayerBasedSVGEngineEnabled(parentSettings->layerBasedSVGEngineEnabled());
                m_page->settings().fontGenericFamilies() = parentSettings->fontGenericFamilies();
                m_page->settings().setCSSDPropertyEnabled(parentSettings->cssDPropertyEnabled());
            }
            m_page->setUseColorAppearance(observer->useSystemDarkAppearance(), false);
        }

        RefPtr localMainFrame = m_page->localMainFrame();
        if (!localMainFrame)
            return EncodedDataStatus::Unknown;

        localMainFrame->setView(LocalFrameView::create(*localMainFrame));
        localMainFrame->init();
        Ref loader = localMainFrame->loader();
        ASSERT(localMainFrame->effectiveSandboxFlags() == SandboxFlags::all());

        RefPtr frameView = localMainFrame->view();
        frameView->setCanHaveScrollbars(false); // SVG Images will always synthesize a viewBox, if it's not available, and thus never see scrollbars.
        frameView->setTransparent(true); // SVG Images are transparent.

        RefPtr activeDocumentLoader = loader->activeDocumentLoader();
        ASSERT(activeDocumentLoader); // DocumentLoader should have been created by frame->init().
        activeDocumentLoader->writer().setMIMEType("image/svg+xml"_s);
        activeDocumentLoader->writer().begin(URL()); // create the empty document
        data()->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
            activeDocumentLoader->writer().addData(buffer);
        });
        activeDocumentLoader->writer().end();

        protect(localMainFrame->document())->updateLayoutIgnorePendingStylesheets();

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

    RefPtr page = element->document().page();
    if (!page)
        return false;

    return page->chrome().client().isSVGImageChromeClient();
}

void SVGImage::subresourcesAreFinished(Document* embedderDocument, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(rootElement());
    if (embedderDocument)
        embedderDocument->incrementLoadEventDelayCount();
    internalPage()->localTopDocument()->whenWindowLoadEventOrDestroyed([embedderDocument = WeakPtr { embedderDocument }, completionHandler = WTF::move(completionHandler)]() mutable {
        if (RefPtr document = embedderDocument.get())
            document->decrementLoadEventDelayCount();
        completionHandler();
    });
}

void SVGImage::tryCreateFromData(std::span<const uint8_t> data, CompletionHandler<void(RefPtr<SVGImage>&&)>&& completionHandler)
{
    Ref svgImage = SVGImage::create(nullptr);
    Ref buffer = SharedBuffer::create(data);
    svgImage->setData(buffer.ptr(), true);
    if (!svgImage->rootElement()) {
        completionHandler(nullptr);
        return;
    }
    svgImage->subresourcesAreFinished(nullptr, [svgImage, completionHandler = WTF::move(completionHandler)]() mutable {
        completionHandler(WTF::move(svgImage));
    });
}

bool SVGImage::isDataDecodable(const Settings& settings, std::span<const uint8_t> data)
{
    Ref document = Document::create(settings, aboutBlankURL());
    Ref domParser = DOMParser::create(document.get());
    auto parseResult = domParser->parseFromString(String::fromUTF8(data), imageSVGContentTypeAtom());
    if (parseResult.hasException())
        return false;

    Ref result = parseResult.returnValue();
    return result->hasSVGRootNode();
}

}
