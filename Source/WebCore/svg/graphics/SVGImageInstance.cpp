/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "SVGImageInstance.h"

#include "AnimationTimelinesController.h"
#include "CSSAnimation.h"
#include "ContainerNodeInlines.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentSVG.h"
#include "DocumentTimeline.h"
#include "DocumentView.h"
#include "FrameLoader.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "RenderSVGRoot.h"
#include "SMILTimeContainer.h"
#include "SVGDocumentExtensions.h"
#include "SVGImage.h"
#include "SVGImageClients.h"
#include "SVGSVGElement.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleDocumentScope.h"
#include "StyleEnvironmentVariables.h"
#include "StyleImageDrawingExtras.h"
#include "StyleScope.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "WebAnimation.h"
#include "WebAnimationTime.h"
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGImageInstance);

Ref<SVGImageInstance> SVGImageInstance::create(SVGImage& image, const ImageEnvironmentSettings* environmentSettings)
{
    auto pageConfiguration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    auto chromeClient = makeUniqueRef<SVGImageChromeClient>(&image);
    auto& svgImageChromeClient = chromeClient.get();
    pageConfiguration.chromeClient = WTF::move(chromeClient);

    // FIXME: If this SVG ends up loading itself, we might leak the world.
    // The Cache code does not know about CachedImages holding Frames and
    // won't know to break the cycle.
    // This will become an issue when SVGImage will be able to load other
    // SVGImage objects, but we're safe now, because SVGImage can only be
    // loaded by a top-level document.
    Ref instance = adoptRef(*new SVGImageInstance(Page::create(WTF::move(pageConfiguration)), environmentSettings));
    Ref page = *instance->m_page;
    svgImageChromeClient.setInstance(instance);
    Ref settings = page->settings();
#if ENABLE(VIDEO)
    settings->setMediaEnabled(false);
#endif
    settings->setScriptEnabled(false);
    settings->setAcceleratedCompositingEnabled(false);
    settings->setShouldAllowUserInstalledFonts(false);

    if (environmentSettings) {
        settings->setLayerBasedSVGEngineEnabled(environmentSettings->layerBasedSVGEngineEnabled());
        settings->fontGenericFamilies() = environmentSettings->fontGenericFamilies();
        settings->setCSSDPropertyEnabled(environmentSettings->cssDPropertyEnabled());
        settings->setDownloadableBinaryFontTrustedTypes(environmentSettings->downloadableBinaryFontTrustedTypes());
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        instance->m_fallbackInvertContent = AXCustomColorModeController::shouldAdjustSVGImages(instance->m_page);
        instance->m_appliedDocumentState.invertContent = instance->m_fallbackInvertContent;
        settings->setAxCustomColorModeEnabled(instance->m_fallbackInvertContent);
        settings->setAxCustomColorModeAppearanceDetectionEnabled(false);
#endif
    }

    Ref localMainFrame = *page->localMainFrame();
    localMainFrame->setView(LocalFrameView::create(localMainFrame));
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
    protect(image.data())->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
        protect(activeDocumentLoader)->writer().addData(buffer);
    });
    activeDocumentLoader->writer().end();

    protect(localMainFrame->document())->updateLayoutIgnorePendingStylesheets();

    // Set the intrinsic size before a container size is available.
    instance->m_intrinsicSize = instance->containerSize();

    return instance;
}

SVGImageInstance::SVGImageInstance(Ref<Page>&& page, const ImageEnvironmentSettings* environmentSettings)
    : m_environmentSettings(environmentSettings)
    , m_page(WTF::move(page))
    , m_startAnimationTimer(*this, &SVGImageInstance::startAnimationTimerFired)
{
}

SVGImageInstance::~SVGImageInstance()
{
    if (m_page) {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        auto page = std::exchange(m_page, nullptr);
    }
}

RefPtr<SVGSVGElement> SVGImageInstance::rootElement() const
{
    if (!m_page)
        return nullptr;

    return DocumentSVG::rootElement(*protect(m_page)->localMainFrame()->document());
}

LocalFrameView* SVGImageInstance::frameView() const
{
    if (!m_page)
        return nullptr;

    return m_page->localMainFrame()->view();
}

void SVGImageInstance::setContainerSize(const FloatSize& size)
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->renderer() || !rootElement->renderer()->isRenderOrLegacyRenderSVGRoot())
        return;

    if (CheckedPtr renderer = dynamicDowncast<LegacyRenderSVGRoot>(rootElement->renderer())) {
        renderer->setContainerSize(IntSize(size));
        return;
    }

    if (CheckedPtr renderer = dynamicDowncast<RenderSVGRoot>(rootElement->renderer())) {
        renderer->setContainerSize(IntSize(size));
        return;
    }
}

IntSize SVGImageInstance::containerSize() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->renderer() || !rootElement->renderer()->isRenderOrLegacyRenderSVGRoot())
        return { };

    // If a container size is available it has precedence.
    auto computeContainerSize = [&]() -> IntSize {
        if (auto* renderer = dynamicDowncast<LegacyRenderSVGRoot>(rootElement->renderer()))
            return renderer->containerSize();

        if (auto* renderer = dynamicDowncast<RenderSVGRoot>(rootElement->renderer()))
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

auto SVGImageInstance::documentStateForDraw(ImageDrawingOptions options, const ImageDrawingExtras* extras, bool fallbackUseDarkAppearance) const -> SVGImageDocumentState
{
    auto state = m_appliedDocumentState;

    state.useDarkAppearance = fallbackUseDarkAppearance;
    state.useElevatedUserInterfaceLevel = false;
    if (auto* styleExtras = dynamicDowncast<Style::ImageDrawingExtras>(extras)) {
        if (!styleExtras->fragmentURL().isNull()) {
            state.fragmentURL = styleExtras->fragmentURL();
            state.linkParameters = styleExtras->linkParameters();
        }
        state.useDarkAppearance = styleExtras->useDarkAppearance().value_or(fallbackUseDarkAppearance);
        state.useElevatedUserInterfaceLevel = styleExtras->useElevatedUserInterfaceLevel().value_or(false);
    }

    state.ignoreRootPreserveAspectRatio = options.ignoreRootPreserveAspectRatio();

    state.heldAnimationTimeline = nullptr;
    if (RefPtr timeline = extras ? extras->animationTimeline() : nullptr; timeline && timeline->mode() != ImageAnimationTimeline::Mode::Running)
        state.heldAnimationTimeline = *timeline;

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    auto invert = options.invertContent();
    state.invertContent = invert == InvertContent::FromResource ? m_fallbackInvertContent : invert == InvertContent::Yes;
#endif

    return state;
}

bool SVGImageInstance::matchesDocumentState(const SVGImageDocumentState& state) const
{
    if (heldAnimationTimelineIsGone())
        return false;

    if (!m_hasBeenDrawn || state == m_appliedDocumentState)
        return true;

    if (m_animationIsHeld || !state.heldAnimationTimeline)
        return false;
    auto playingState = state;
    playingState.heldAnimationTimeline = nullptr;
    return playingState == m_appliedDocumentState && !hasAnimations();
}

void SVGImageInstance::applyDocumentState(Document& document, LocalFrameView& view, const SVGImageDocumentState& state)
{
    if (state == m_appliedDocumentState)
        return;

    bool updateStyle = false;

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (state.invertContent != m_appliedDocumentState.invertContent) {
        protect(m_page)->settings().setAxCustomColorModeEnabled(state.invertContent);
        updateStyle = true;
    }
#endif

    if (state.useDarkAppearance != m_appliedDocumentState.useDarkAppearance || state.useElevatedUserInterfaceLevel != m_appliedDocumentState.useElevatedUserInterfaceLevel) {
        protect(m_page)->setUseColorAppearance(state.useDarkAppearance, state.useElevatedUserInterfaceLevel);
        updateStyle = true;
    }

    if (state.linkParameters != m_appliedDocumentState.linkParameters) {
        protect(document)->styleScope().environmentVariables().setLinkParameters(state.linkParameters);
        updateStyle = true;
    }

    if (state.fragmentURL != m_appliedDocumentState.fragmentURL || state.ignoreRootPreserveAspectRatio != m_appliedDocumentState.ignoreRootPreserveAspectRatio) {
        if (state.fragmentURL != m_appliedDocumentState.fragmentURL)
            protect(view)->scrollToFragment(state.fragmentURL);

        if (RefPtr root = rootElement()) {
            root->setIgnoresPreserveAspectRatioForSVGImage(state.ignoreRootPreserveAspectRatio == IgnoreRootPreserveAspectRatio::Yes);
            if (CheckedPtr renderer = root->renderer())
                root->invalidateCurrentView(*renderer);
        }

        updateStyle = true;
    }

    if (updateStyle) {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        protect(document)->updateStyleIfNeeded();
    }

    auto heldAnimationTimeline = hasAnimations() ? state.heldAnimationTimeline : nullptr;
    if (heldAnimationTimeline != m_appliedDocumentState.heldAnimationTimeline) {
        if (RefPtr timeline = heldAnimationTimeline.get())
            holdAnimations(document, timeline->mode());
        else
            releaseAnimations(document);
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        protect(document)->updateStyleIfNeeded();
    }

    m_appliedDocumentState = state;
    m_appliedDocumentState.heldAnimationTimeline = WTF::move(heldAnimationTimeline);
}

void SVGImageInstance::holdAnimations(Document& document, ImageAnimationTimeline::Mode mode)
{
    m_animationIsHeld = true;
    m_startAnimationTimer.stop();
    if (RefPtr rootElement = this->rootElement()) {
        rootElement->pauseAnimations();
        if (mode == ImageAnimationTimeline::Mode::Stopped)
            rootElement->setCurrentTime(0);
    }

    if (mode == ImageAnimationTimeline::Mode::Stopped) {
        for (Ref animation : document.getAnimations())
            std::ignore = animation->setCurrentTime(WebAnimationTime::fromMilliseconds(0));
    }
    document.ensureTimelinesController().suspendAnimations();
}

void SVGImageInstance::releaseGoneTimeline()
{
    RefPtr document = m_page ? protect(m_page)->localTopDocument() : nullptr;
    if (document)
        releaseAnimations(*document);
    else
        m_animationIsHeld = false;
    m_appliedDocumentState.heldAnimationTimeline = nullptr;
}

void SVGImageInstance::releaseAnimations(Document& document)
{
    m_animationIsHeld = false;
    if (CheckedPtr timelinesController = document.timelinesController())
        timelinesController->resumeAnimations();
    if (RefPtr rootElement = this->rootElement())
        rootElement->unpauseAnimations();
}

static Vector<std::pair<String, Ref<WebAnimation>>> keyedAnimations(Document& document)
{
    HashMap<RefPtr<Element>, unsigned> elementIndices;
    unsigned elementIndex = 0;
    for (Ref element : descendantsOfType<Element>(document))
        elementIndices.add(element.ptr(), elementIndex++);

    HashMap<String, unsigned> occurrences;
    Vector<std::pair<String, Ref<WebAnimation>>> animations;
    for (Ref animation : document.getAnimations()) {
        RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation.get());
        auto owningElement = cssAnimation ? cssAnimation->owningElement() : std::nullopt;
        if (!owningElement) {
            animations.append({ String { }, WTF::move(animation) });
            continue;
        }
        auto pseudoId = owningElement->pseudoElementIdentifier ? static_cast<unsigned>(owningElement->pseudoElementIdentifier->type) + 1 : 0;
        auto key = makeString(elementIndices.get(&owningElement->element), ' ', pseudoId, ' ', cssAnimation->animationName());
        auto occurrence = occurrences.add(key, 0).iterator->value++;
        animations.append({ makeString(key, ' ', occurrence), WTF::move(animation) });
    }
    return animations;
}

void SVGImageInstance::copyAnimationTimes(const SVGImageInstance& source)
{
    RefPtr rootElement = this->rootElement();
    RefPtr sourceRootElement = source.rootElement();
    if (!rootElement || !sourceRootElement)
        return;

    rootElement->setCurrentTime(sourceRootElement->getCurrentTime());

    Ref sourceDocument = sourceRootElement->document();
    HashMap<String, Ref<WebAnimation>> sourceAnimations;
    for (auto& [key, animation] : keyedAnimations(sourceDocument)) {
        if (!key.isNull())
            sourceAnimations.add(key, animation);
    }

    RefPtr sourceTimeline = sourceDocument->existingTimeline();
    auto sourceTimelineTime = sourceTimeline ? sourceTimeline->currentTime() : std::nullopt;

    for (auto& [key, animation] : keyedAnimations(protect(rootElement->document()))) {
        if (RefPtr sourceAnimation = key.isNull() ? nullptr : sourceAnimations.get(key))
            std::ignore = animation->setCurrentTime(sourceAnimation->currentTime());
        else if (sourceTimelineTime)
            std::ignore = animation->setCurrentTime(sourceTimelineTime);
    }
}

bool SVGImageInstance::hasHDRContent() const
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (!m_page)
        return false;

    if (RefPtr localTopDocument = protect(m_page)->localTopDocument())
        return localTopDocument->hasHDRContent();
#endif
    return false;
}

ImageDrawResult SVGImageInstance::draw(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, ImageDrawingOptions options, const SVGImageDocumentState& state)
{
    if (!m_page)
        return ImageDrawResult::DidNothing;

    RefPtr document = protect(m_page)->localTopDocument();
    if (!document)
        return ImageDrawResult::DidNothing;

    auto concreteSize = concreteObjectSize.size();
    if (concreteSize.isEmpty())
        return ImageDrawResult::DidNothing;

    RefPtr view = frameView();
    ASSERT(view);

    auto containerSize = roundedIntSize(concreteSize);
    setContainerSize(containerSize);
    applyDocumentState(*document, *view, state);
    m_hasBeenDrawn = true;

    auto adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(1 / concreteObjectSize.zoom());

    // Compensate for the container size rounding by adjusting the source rect.
    auto adjustedSrcSize = adjustedSrcRect.size();
    adjustedSrcSize.scale(containerSize.width() / concreteSize.width(), containerSize.height() / concreteSize.height());
    adjustedSrcRect.setSize(adjustedSrcSize);

    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    if (options.interpolationQuality() != InterpolationQuality::Default)
        context.setImageInterpolationQuality(options.interpolationQuality());
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

    view->resize(containerSize);

    {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        if (view->needsLayout())
            protect(view->layoutContext())->layout();
    }

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(view->useDarkAppearance());
#endif

    view->paint(context, intersection(context.clipBounds(), enclosingIntRect(adjustedSrcRect)));

    if (compositingRequiresTransparencyLayer)
        context.endTransparencyLayer();

    return ImageDrawResult::DidDraw;
}

std::optional<Style::ImageDrawingExtras> SVGImageInstance::drawingExtrasForChanges() const
{
    if (!m_hasBeenDrawn)
        return std::nullopt;
    return Style::ImageDrawingExtras { URL { }, CSS::Keyword::None { }, m_appliedDocumentState.useDarkAppearance, m_appliedDocumentState.useElevatedUserInterfaceLevel, m_environmentSettings };
}

void SVGImageInstance::startAnimation()
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    m_startAnimationTimer.startOneShot(0_s);
}

void SVGImageInstance::startAnimationTimerFired()
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement || !rootElement->animationsPaused())
        return;
    rootElement->unpauseAnimations();
    if (CheckedPtr timelinesController = protect(rootElement->document())->timelinesController())
        timelinesController->resumeAnimations();
}

void SVGImageInstance::stopAnimation()
{
    m_startAnimationTimer.stop();
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return;
    rootElement->pauseAnimations();
    protect(rootElement->document())->ensureTimelinesController().suspendAnimations();
}

void SVGImageInstance::resetAnimation()
{
    stopAnimation();
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return;
    rootElement->setCurrentTime(0);
}

bool SVGImageInstance::isAnimating() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return false;

    Ref document = rootElement->document();
    if (CheckedPtr svgExtensions = document->svgExtensionsIfExists()) {
        if (svgExtensions->hasActiveSMILAnimations())
            return true;
    }

    RefPtr timeline = document->existingTimeline();
    return timeline && !timeline->animationsAreSuspended() && !timeline->relevantAnimations().isEmpty();
}

bool SVGImageInstance::hasAnimations() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return false;

    if (Ref timeContainer = rootElement->timeContainer(); timeContainer->hasAnimations())
        return true;

    RefPtr timeline = protect(rootElement->document())->existingTimeline();
    return timeline && !timeline->relevantAnimations().isEmpty();
}

Seconds SVGImageInstance::animationTime() const
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return { };

    Seconds time { rootElement->getCurrentTime() };
    for (Ref animation : protect(rootElement->document())->getAnimations()) {
        if (auto currentTime = animation->currentTime(); currentTime && currentTime->time())
            time = std::max(time, *currentTime->time());
    }
    return time;
}

size_t SVGImageInstance::approximateMemoryCost() const
{
    RefPtr localTopDocument = protect(m_page)->localTopDocument();
    if (!localTopDocument)
        return 0;

    size_t cost = 0;
    for (RefPtr<Node> node = localTopDocument; node; node = NodeTraversal::next(*node))
        cost += node->approximateMemoryCost();
    return cost;
}

} // namespace WebCore
