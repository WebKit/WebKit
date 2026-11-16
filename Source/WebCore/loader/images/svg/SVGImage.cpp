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
#include "CommonVM.h"
#include "ContainerNodeInlines.h"
#include "DOMParser.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "LocalFrameView.h"
#include "NativeImage.h"
#include "Page.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFEImageElement.h"
#include "SVGForeignObjectElement.h"
#include "SVGImageClients.h"
#include "SVGImageElement.h"
#include "SVGImageInstance.h"
#include "SVGImageInstanceCache.h"
#include "SVGSVGElement.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>

namespace WebCore {

class SVGImage::DrawScope {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(DrawScope);
public:
    explicit DrawScope(SVGImage& image)
        : m_image(image)
        , m_observer(image.imageObserver())
    {
        image.setImageObserver(nullptr);
    }

    ~DrawScope()
    {
        m_image->setImageObserver(m_observer.copyRef());
        m_image->scheduleRenderingUpdateIfNeeded();
    }

private:
    const Ref<SVGImage> m_image;
    const RefPtr<ImageObserver> m_observer;
};

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
    , m_instances(makeUniqueRef<SVGImageInstanceCache>())
    , m_startAnimationTimer(*this, &SVGImage::startAnimationTimerFired)
{
}

SVGImage::~SVGImage() = default;

Page* SVGImage::defaultInstancePage() const
{
    RefPtr instance = m_instances->defaultInstance();
    return instance ? instance->page() : nullptr;
}

RefPtr<SVGSVGElement> SVGImage::defaultInstanceRootElement() const
{
    RefPtr instance = m_instances->defaultInstance();
    return instance ? instance->rootElement() : nullptr;
}

RefPtr<SVGSVGElement> SVGImage::rootElement(const ImageDrawDescriptor& descriptor)
{
    DrawScope drawScope(*this);
    RefPtr instance = instanceForDraw(descriptor);
    return instance ? instance->rootElement() : nullptr;
}

void SVGImage::updateRendering()
{
    m_needsRenderingUpdate = false;
    m_instances->forEach([](auto& instance) {
        instance.updateRendering();
    });
}

void SVGImage::scheduleRenderingUpdate()
{
    m_needsRenderingUpdate = true;
    scheduleRenderingUpdateIfNeeded();
}

void SVGImage::scheduleRenderingUpdateIfNeeded()
{
    if (!m_needsRenderingUpdate)
        return;
    if (RefPtr observer = imageObserver())
        observer->scheduleRenderingUpdate(*this);
}

bool SVGImage::renderingTaintsOrigin() const
{
    RefPtr rootElement = defaultInstanceRootElement();
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
    return m_instances->anyOf([](auto& instance) {
        return instance.hasHDRContent();
    });
}

#if ENABLE(AX_CUSTOM_COLOR_MODE)
void SVGImage::setFallbackInvertContent(bool invertContent)
{
    m_instances->forEach([&](auto& instance) {
        instance.setFallbackInvertContent(invertContent);
    });
}
#endif

RefPtr<NativeImage> SVGImage::nativeImage(const ImageDrawDescriptor& descriptor, const ColorSpace& colorSpace)
{
    if (!defaultInstancePage())
        return nullptr;

    auto size = descriptor.concreteObjectSize.size();
    RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::DOM, 1, colorSpace, PixelFormat::BGRA8);
    if (!imageBuffer)
        return nullptr;

    DrawScope drawScope(*this);

    draw(imageBuffer->context(), ImageDrawDescriptor { ConcreteObjectSize::fixed(size), descriptor.rendition });

    return ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
}

RefPtr<NativeImage> SVGImage::currentNativeImage(const ImageDrawDescriptor& descriptor)
{
    return nativeImage(descriptor);
}

RefPtr<NativeImage> SVGImage::currentPreTransformedNativeImage(const ImageDrawDescriptor& descriptor)
{
    return nativeImage(descriptor);
}

std::optional<Image::PatternTile> SVGImage::currentPatternTile(GraphicsContext& context, const ImageDrawDescriptor& descriptor)
{
    auto concreteObjectSize = descriptor.concreteObjectSize;
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
        return std::nullopt;

    draw(buffer->context(), ImageDrawDescriptor { concreteObjectSize, descriptor.rendition }, imageBufferSize, zoomedContainerRect);

    return PatternTile { SourceImage { buffer.releaseNonNull() }, imageBufferScale };
}

ImageDrawResult SVGImage::drawImage(GraphicsContext& context, const ImageDrawDescriptor& descriptor, const FloatRect& dstRect, const FloatRect& srcRect)
{
    auto concreteObjectSize = descriptor.concreteObjectSize;
    auto concreteSize = concreteObjectSize.size();
    if (concreteSize.isEmpty())
        return ImageDrawResult::DidNothing;

    DrawScope drawScope(*this);

    RefPtr instance = instanceForDraw(descriptor);
    if (!instance || !instance->frameView())
        return ImageDrawResult::DidNothing;

    instance->applyDocumentState(instance->documentStateForDraw(descriptor));
    auto containerSize = instance->appliedDocumentState().containerSize;

    auto adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(1 / concreteObjectSize.zoom());

    // Compensate for the container size rounding by adjusting the source rect.
    auto adjustedSrcSize = adjustedSrcRect.size();
    adjustedSrcSize.scale(containerSize.width() / concreteSize.width(), containerSize.height() / concreteSize.height());
    adjustedSrcRect.setSize(adjustedSrcSize);

    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(descriptor.compositeOperator, descriptor.blendMode);
    if (descriptor.rendition.interpolationQuality != InterpolationQuality::Default)
        context.setImageInterpolationQuality(descriptor.rendition.interpolationQuality);
    context.clip(enclosingIntRect(dstRect));

    float alpha = context.alpha();
    bool compositingRequiresTransparencyLayer = descriptor.compositeOperator != CompositeOperator::SourceOver || descriptor.blendMode != BlendMode::Normal || alpha < 1;
    if (compositingRequiresTransparencyLayer) {
        context.beginTransparencyLayer(alpha);
        context.setCompositeOperation(CompositeOperator::SourceOver, BlendMode::Normal);
    }

    auto orientation = descriptor.orientation;
    // SVG images don't have intrinsic orientation metadata like EXIF, so FromImage defaults to None.
    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = ImageOrientation::Orientation::None;

    FloatSize scale(dstRect.size() / adjustedSrcRect.size());

    // We can only draw the entire frame, clipped to the rect we want. So compute where the top left
    // of the image would be if we were drawing without clipping, and translate accordingly.
    FloatSize topLeftOffset(adjustedSrcRect.location().x() * scale.width(), adjustedSrcRect.location().y() * scale.height());
    FloatPoint destOffset = dstRect.location() - topLeftOffset;

    context.translate(destOffset);
    context.scale(scale);

    // Apply orientation transformation if needed.
    if (orientation != ImageOrientation::Orientation::None) {
        auto orientationTransform = ImageOrientation(orientation).transformFromDefault(FloatSize(containerSize));
        context.concatCTM(orientationTransform);
    }

    instance->paint(context, adjustedSrcRect);

    if (compositingRequiresTransparencyLayer)
        context.endTransparencyLayer();

    stateSaver.restore();

    if (RefPtr observer = imageObserver())
        observer->didDraw(*this);

    return ImageDrawResult::DidDraw;
}

bool SVGImage::hasSomethingToDraw() const
{
    return defaultInstanceEmbeddedSVGRoot();
}

RenderReplaced* SVGImage::defaultInstanceEmbeddedSVGRoot() const
{
    RefPtr rootElement = defaultInstanceRootElement();
    return rootElement ? downcast<RenderReplaced>(rootElement->renderer()) : nullptr;
}

LocalFrameView* SVGImage::defaultInstanceFrameView() const
{
    RefPtr instance = m_instances->defaultInstance();
    return instance ? instance->frameView() : nullptr;
}

NaturalDimensions SVGImage::unorientedNaturalDimensions() const
{
    RefPtr rootElement = defaultInstanceRootElement();
    return rootElement ? rootElement->naturalDimensions() : NaturalDimensions::none();
}

void SVGImage::startAnimation()
{
    bool animationsArePaused = m_instances->anyOf([](auto& instance) {
        return instance.isAnimationPaused();
    });
    if (animationsArePaused)
        m_startAnimationTimer.startOneShot(0_s);

    scheduleRenderingUpdateIfNeeded();
}

void SVGImage::startAnimationTimerFired()
{
    m_instances->forEach([](auto& instance) {
        instance.startAnimation();
    });
}

void SVGImage::stopAnimation()
{
    m_startAnimationTimer.stop();
    m_instances->forEach([](auto& instance) {
        instance.stopAnimation();
    });
}

void SVGImage::resetAnimation()
{
    stopAnimation();
    m_instances->forEach([](auto& instance) {
        instance.resetAnimation();
    });
}

bool SVGImage::isAnimating() const
{
    return m_instances->anyOf([](auto& instance) {
        return instance.isAnimating();
    });
}

RefPtr<SVGImageInstance> SVGImage::instanceForDraw(const ImageDrawDescriptor& descriptor)
{
    RefPtr defaultInstance = m_instances->defaultInstance();
    if (!defaultInstance)
        return nullptr;

    auto& contentSettings = descriptor.rendition.contentSettings;
    if (!contentSettings)
        return defaultInstance;

    if (RefPtr instance = m_instances->find(SVGImageInstance::key(*contentSettings)))
        return instance;

    // The draw that parses a new instance's document may be in the middle of painting.
    ScriptDisallowedScope::DisableAssertionsInScope disabledScope;

    Ref instance = SVGImageInstance::create(*this, contentSettings);
    m_instances->add(instance.copyRef());
    didCreateInstance(instance);

    return instance;
}

void SVGImage::didCreateInstance(SVGImageInstance& instance)
{
    // Animate the new instance together with the others.
    if (RefPtr defaultInstance = m_instances->defaultInstance()) {
        if (RefPtr defaultRootElement = defaultInstance->rootElement(); defaultRootElement && defaultRootElement->animationsPaused()) {
            if (RefPtr rootElement = instance.rootElement())
                rootElement->pauseAnimations();
        }
        instance.copyAnimationTimes(*defaultInstance);
    }

    reportApproximateMemoryCost(instance);
}

void SVGImage::reportApproximateMemoryCost(const SVGImageInstance& instance) const
{
    if (!instance.page())
        return;

    // The data is shared by every instance, so it is counted with the default one.
    size_t cost = instance.approximateMemoryCost();
    if (&instance == m_instances->defaultInstance())
        cost += data()->size();

    JSC::VM& vm = commonVM();
    JSC::JSLockHolder lock(vm);
    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    vm.heap.deprecatedReportExtraMemory(cost);
}

EncodedDataStatus SVGImage::dataChanged(bool allDataReceived)
{
    // Don't do anything; it is an empty image.
    if (!data()->size())
        return EncodedDataStatus::Complete;

    if (allDataReceived) {
        std::optional<ImageContentSettings> contentSettings;
        if (RefPtr observer = imageObserver()) {
            if (RefPtr parentSettings = observer->settings())
                contentSettings = ImageContentSettings { *parentSettings };
        }

        Ref instance = SVGImageInstance::create(*this, contentSettings);
        m_instances->reset(instance.copyRef());
        reportApproximateMemoryCost(instance);
    }

    return defaultInstancePage() ? EncodedDataStatus::Complete : EncodedDataStatus::Unknown;
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
    ASSERT(defaultInstanceRootElement());
    if (embedderDocument)
        embedderDocument->incrementLoadEventDelayCount();
    protect(protect(defaultInstancePage())->localTopDocument())->whenWindowLoadEventOrDestroyed([embedderDocument = WeakPtr { embedderDocument }, completionHandler = WTF::move(completionHandler)] mutable {
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
    if (!svgImage->defaultInstanceRootElement()) {
        completionHandler(nullptr);
        return;
    }
    svgImage->subresourcesAreFinished(nullptr, [svgImage, completionHandler = WTF::move(completionHandler)] mutable {
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

} // namespace WebCore
