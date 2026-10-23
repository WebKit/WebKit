/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008-2026 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "ContainerNodeInlines.h"
#include "DOMParser.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "NativeImage.h"
#include "Page.h"
#include "RenderReplaced.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFEImageElement.h"
#include "SVGForeignObjectElement.h"
#include "SVGImageElement.h"
#include "SVGImageInstance.h"
#include "SVGImageInstanceCache.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleImageDrawingExtras.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/ScopedLambda.h>

namespace WebCore {

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
    , m_instances(makeUniqueRef<SVGImageInstanceCache>())
{
}

SVGImage::~SVGImage() = default;

FloatSize SVGImage::size(ImageOrientation) const
{
    RefPtr defaultInstance = m_instances->defaultInstance();
    return defaultInstance ? defaultInstance->intrinsicSize() : FloatSize { };
}

Page* SVGImage::internalPage()
{
    RefPtr defaultInstance = m_instances->defaultInstance();
    return defaultInstance ? defaultInstance->page() : nullptr;
}

void SVGImage::updateRendering()
{
        // Clear m_page, so that SVGImageChromeClient knows we're destructed.
    m_needsRenderingUpdate = false;

    Vector<Ref<SVGImageInstance>> instances;
    m_instances->forEach([&](auto& instance) {
        instances.append(instance);
    });
    for (Ref instance : instances) {
        if (RefPtr page = instance->page())
            page->isolatedUpdateRendering();
    }
}

#if ENABLE(AX_CUSTOM_COLOR_MODE)
void SVGImage::setFallbackInvertContent(bool invertContent)
{
    m_instances->forEach([&](auto& instance) {
        instance.setFallbackInvertContent(invertContent);
    });
}
#endif

RefPtr<SVGSVGElement> SVGImage::rootElement() const
{
    RefPtr defaultInstance = m_instances->defaultInstance();
    return defaultInstance ? defaultInstance->rootElement() : nullptr;
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

bool SVGImage::hasHDRContent() const
{
    return m_instances->containsIf([](auto& instance) {
        return instance.hasHDRContent();
    });
}

RefPtr<NativeImage> SVGImage::nativeImage(ConcreteObjectSize concreteObjectSize, const ColorSpace& colorSpace, const ImageDrawingExtras* extras)
{
    return nativeImage(concreteObjectSize.size(), colorSpace, extras);
}

RefPtr<NativeImage> SVGImage::nativeImage(const FloatSize& size, const ColorSpace& colorSpace, const ImageDrawingExtras* extras, ImageDrawingOptions options)
{
    RefPtr page = internalPage();
    if (!page)
        return nullptr;

    auto renderingMode = page->settings().acceleratedDrawingEnabled() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

    HostWindow* hostWindow = nullptr;
    if (CheckedPtr svgRoot = embeddedSVGRoot())
        hostWindow = svgRoot->hostWindow();

    RefPtr imageBuffer = ImageBuffer::create(size, renderingMode, RenderingPurpose::DOM, 1, colorSpace, PixelFormat::BGRA8, hostWindow);
    if (!imageBuffer)
        return nullptr;

    {
        ImageObserverDisableScope imageObserverDisabler(*this);
        imageBuffer->context().drawImage(*this, ConcreteObjectSize::fixed(size), FloatPoint(), options, extras);
    }
    reportChangesHeldWhileObserverWasDisabled();

    return ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
}

RefPtr<NativeImage> SVGImage::currentNativeImage(ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
{
    return nativeImage(concreteObjectSize, ColorSpace::SRGB(), extras);
}

RefPtr<NativeImage> SVGImage::currentPreTransformedNativeImage(ConcreteObjectSize concreteObjectSize, ImageOrientation, const ImageDrawingExtras* extras)
{
    return nativeImage(concreteObjectSize, ColorSpace::SRGB(), extras);
}

void SVGImage::drawPattern(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect,
    const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, const ImageDrawingExtras* extras)
{
    FloatRect zoomedContainerRect = FloatRect(FloatPoint(), concreteObjectSize.size());
    zoomedContainerRect.scale(concreteObjectSize.zoom());

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

    ImageDrawingOptions bufferOptions {
        options.ignoreRootPreserveAspectRatio(),
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        options.invertContent(),
#endif
    };
    draw(buffer->context(), concreteObjectSize, imageBufferSize, zoomedContainerRect, bufferOptions, extras);
    if (options.drawLuminanceMask() == DrawLuminanceMask::Yes)
        buffer->convertToLuminanceMask();

    // Adjust the source rect and transform due to the image buffer's scaling.
    FloatRect scaledSrcRect = srcRect;
    scaledSrcRect.scale(imageBufferScale.width(), imageBufferScale.height());
    AffineTransform unscaledPatternTransform(patternTransform);
    unscaledPatternTransform.scale(1 / imageBufferScale.width(), 1 / imageBufferScale.height());

    context.drawPattern(*buffer, dstRect, scaledSrcRect, unscaledPatternTransform, phase, spacing, options);
}

ImageDrawResult SVGImage::draw(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, ImageDrawingOptions options, const ImageDrawingExtras* extras)
{
    if (m_instances->isEmpty() || concreteObjectSize.size().isEmpty())
        return ImageDrawResult::DidNothing;

    auto* styleExtras = dynamicDowncast<Style::ImageDrawingExtras>(extras);
    if (auto useDarkAppearance = styleExtras ? styleExtras->useDarkAppearance() : std::nullopt) {
        if (*useDarkAppearance)
            m_hasDrawnDark = true;
        else
            m_hasDrawnLight = true;
    }

    auto state = documentStateForDraw(options, extras);

    auto result = [&] {
        // Temporarily reset image observer, we don't want to receive any changeInRect() calls due to this relayout.
        ImageObserverDisableScope imageObserverDisabler(*this);

        Ref instance = instanceForDraw(environmentSettingsForDraw(extras), state);
        return instance->draw(context, concreteObjectSize, dstRect, srcRect, options, state);
    }();

    if (result == ImageDrawResult::DidDraw) {
        if (RefPtr observer = imageObserver())
            observer->didDraw(*this);
    }
    reportChangesHeldWhileObserverWasDisabled();

    return result;
}

SVGImageDocumentState SVGImage::documentStateForDraw(ImageDrawingOptions options, const ImageDrawingExtras* extras) const
{
    bool fallbackUseDarkAppearance = m_hasDrawnDark && !m_hasDrawnLight;

    return protect(m_instances->defaultInstance())->documentStateForDraw(options, extras, fallbackUseDarkAppearance);
}

const ImageEnvironmentSettings* SVGImage::environmentSettingsForDraw(const ImageDrawingExtras* extras) const
{
    auto* styleExtras = dynamicDowncast<Style::ImageDrawingExtras>(extras);
    if (auto* environmentSettings = styleExtras ? styleExtras->environmentSettings() : nullptr)
        return environmentSettings;
    return m_environmentSettings.get();
}

Ref<SVGImageInstance> SVGImage::instanceForDraw(const ImageEnvironmentSettings* environmentSettings, const SVGImageDocumentState& state)
{
    auto lookup = m_instances->instanceForDraw(*this, environmentSettings, state);
    if (lookup.tookPlayingInstance)
        m_tookPlayingInstance = true;
    return WTF::move(lookup.instance);
}

Seconds SVGImage::animationTimeForTesting(const ImageDrawingExtras* extras) const
{
    if (m_instances->isEmpty())
        return { };

    RefPtr instance = m_instances->matchingInstance(environmentSettingsForDraw(extras), documentStateForDraw({ }, extras));
    return instance ? instance->animationTime() : Seconds { };
}

void SVGImage::scheduleRenderingUpdate()
{
    m_needsRenderingUpdate = true;
    if (RefPtr observer = imageObserver())
        observer->scheduleRenderingUpdate(*this);
}

void SVGImage::reportChangesHeldWhileObserverWasDisabled()
{
    RefPtr observer = imageObserver();
    if (!observer)
        return;

    reportDecodedSizeChange();
    if (m_needsRenderingUpdate)
        observer->scheduleRenderingUpdate(*this);
    if (std::exchange(m_tookPlayingInstance, false))
        observer->changedInRect(*this);
}

void SVGImage::reportDecodedSizeChange()
{
    RefPtr observer = imageObserver();
    if (!observer)
        return;

    if (auto change = m_instances->takeDecodedSizeChange())
        observer->decodedSizeChanged(*this, change);
}

void SVGImage::destroyDecodedData(bool destroyAll)
{
    m_instances->removeInstances(destroyAll ? 0 : 1);
    reportDecodedSizeChange();
}

bool SVGImage::currentFrameIsComplete() const
{
    return m_instances->defaultInstance();
}

bool SVGImage::hasSomethingToDraw() const
{
    return embeddedSVGRoot();
}

RenderReplaced* SVGImage::embeddedSVGRoot() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return nullptr;
    return downcast<RenderReplaced>(rootElement->renderer());
}

LocalFrameView* SVGImage::frameView() const
{
    RefPtr defaultInstance = m_instances->defaultInstance();
    return defaultInstance ? defaultInstance->frameView() : nullptr;
}

NaturalDimensions SVGImage::unorientedNaturalDimensions() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return NaturalDimensions::none();
    return rootElement->naturalDimensions();
}

void SVGImage::startAnimation()
{
    m_instances->removeInstancesHeldByGoneTimelines();
    reportDecodedSizeChange();
    m_instances->forEach([](auto& instance) {
        if (!instance.animationIsHeld())
            instance.startAnimation();
    });

    if (m_needsRenderingUpdate) {
        if (RefPtr observer = imageObserver())
            observer->scheduleRenderingUpdate(*this);
    }
}

void SVGImage::stopAnimation(const ImageDrawingExtras* changedFor)
{
    auto* styleChangedFor = dynamicDowncast<Style::ImageDrawingExtras>(changedFor);
    m_instances->forEach([&](auto& instance) {
        if (!styleChangedFor) {
            instance.stopAnimation();
            return;
        }
        if (auto extras = instance.drawingExtrasForChanges(); extras && extras->hasSameEnvironment(*styleChangedFor))
            instance.stopAnimation();
    });
}

void SVGImage::resetAnimation()
{
    m_instances->forEach([](auto& instance) {
        if (!instance.animationIsHeld())
            instance.resetAnimation();
    });
}

bool SVGImage::isAnimating() const
{
    return m_instances->containsIf([](auto& instance) {
        return instance.isAnimating();
    });
}

void SVGImage::reportApproximateMemoryCost() const
{
    JSC::VM& vm = commonVM();
    JSC::JSLockHolder lock(vm);
    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    RefPtr defaultInstance = m_instances->defaultInstance();
    vm.heap.deprecatedReportExtraMemory((defaultInstance ? defaultInstance->approximateMemoryCost() : 0) + data()->size());
}

EncodedDataStatus SVGImage::dataChanged(bool allDataReceived, const ImageDrawingExtras* extras)
{
    // Don't do anything; it is an empty image.
    if (!data()->size())
        return EncodedDataStatus::Complete;

    if (allDataReceived) {
        auto* styleExtras = dynamicDowncast<Style::ImageDrawingExtras>(extras);
        m_environmentSettings = styleExtras ? styleExtras->environmentSettings() : nullptr;

        m_instances->reset(SVGImageInstance::create(*this, m_environmentSettings.get()));
        reportDecodedSizeChange();
        reportApproximateMemoryCost();
    }

    return m_instances->defaultInstance() ? EncodedDataStatus::Complete : EncodedDataStatus::Unknown;
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
    protect(protect(internalPage())->localTopDocument())->whenWindowLoadEventOrDestroyed([embedderDocument = WeakPtr { embedderDocument }, completionHandler = WTF::move(completionHandler)]() mutable {
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
