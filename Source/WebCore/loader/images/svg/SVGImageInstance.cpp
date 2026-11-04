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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SVGImageInstance.h"

#include "Chrome.h"
#include "ContainerNodeInlines.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentSVG.h"
#include "DocumentTimeline.h"
#include "DocumentView.h"
#include "FrameLoader.h"
#include "GraphicsContext.h"
#include "ImageDrawDescriptor.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "RenderSVGRoot.h"
#include "SVGDocumentExtensions.h"
#include "SVGImage.h"
#include "SVGImageClients.h"
#include "SVGSVGElement.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StyleDocumentScope.h"
#include "StyleEnvironmentVariables.h"
#include "StyleScope.h"
#include "WebAnimation.h"
#include "WebAnimationTime.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ZippedRange.h>

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGImageInstance);

Ref<SVGImageInstance> SVGImageInstance::create(SVGImage& image, const std::optional<ImageContentSettings>& contentSettings)
{
    auto pageConfiguration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    pageConfiguration.chromeClient = makeUniqueRef<SVGImageChromeClient>(&image);

    // FIXME: If this SVG ends up loading itself, we might leak the world.
    // The Cache code does not know about CachedImages holding Frames and
    // won't know to break the cycle.
    // This will become an issue when SVGImage will be able to load other
    // SVGImage objects, but we're safe now, because SVGImage can only be
    // loaded by a top-level document.
    Ref instance = adoptRef(*new SVGImageInstance(Page::create(WTF::move(pageConfiguration))));
    Ref page = *instance->m_page;
    Ref settings = page->settings();
#if ENABLE(VIDEO)
    settings->setMediaEnabled(false);
#endif
    settings->setScriptEnabled(false);
    settings->setAcceleratedCompositingEnabled(false);
    settings->setShouldAllowUserInstalledFonts(false);

    if (contentSettings) {
        settings->setLayerBasedSVGEngineEnabled(contentSettings->layerBasedSVGEngineEnabled);
        settings->setFontGenericFamilies(contentSettings->fontGenericFamilies);
        settings->setCSSDPropertyEnabled(contentSettings->cssDPropertyEnabled);
        settings->setDownloadableBinaryFontTrustedTypes(contentSettings->downloadableBinaryFontTrustedTypes);
        instance->m_appliedDocumentState.rendition.contentSettings = contentSettings;
    }
    instance->m_parseSettings = { settings->layerBasedSVGEngineEnabled(), settings->cssDPropertyEnabled() };

    RefPtr localMainFrame = page->localMainFrame();
    if (!localMainFrame)
        return instance;

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
    protect(image.data())->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
        protect(activeDocumentLoader)->writer().addData(buffer);
    });
    activeDocumentLoader->writer().end();

    protect(localMainFrame->document())->updateLayoutIgnorePendingStylesheets();

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    instance->m_fallbackInvertContent = AXCustomColorModeController::shouldAdjustSVGImages(protect(*localMainFrame->document()));
    instance->m_appliedDocumentState.rendition.invertContent = instance->m_fallbackInvertContent ? InvertContent::Yes : InvertContent::No;
    settings->setAxCustomColorModeEnabled(instance->m_fallbackInvertContent);
    settings->setAxCustomColorModeAppearanceDetectionEnabled(false);
#endif

    return instance;
}

SVGImageInstance::SVGImageInstance(Ref<Page>&& page)
    : m_page(WTF::move(page))
    , m_appliedDocumentState {
        .containerSize = IntSize { },
        .rendition = {
            .fragmentURL = URL { },
            .linkParameters = CSS::Keyword::None { },
            .ignoreRootPreserveAspectRatio = IgnoreRootPreserveAspectRatio::No,
#if ENABLE(AX_CUSTOM_COLOR_MODE)
            .invertContent = InvertContent::No,
#endif
        },
    }
{
}

SVGImageInstance::~SVGImageInstance()
{
    ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
    m_page = nullptr;
}

void SVGImageInstance::detachFromImage()
{
    if (RefPtr page = m_page)
        downcast<SVGImageChromeClient>(page->chrome().client()).detachFromImage();
}

SVGImageInstanceKey SVGImageInstance::key(const ImageContentSettings& contentSettings)
{
    return { { contentSettings.layerBasedSVGEngineEnabled, contentSettings.cssDPropertyEnabled } };
}

void SVGImageInstance::updateRendering()
{
    if (RefPtr page = m_page)
        page->isolatedUpdateRendering();
}

RefPtr<SVGSVGElement> SVGImageInstance::rootElement() const
{
    RefPtr page = this->page();
    if (!page)
        return nullptr;

    RefPtr localMainFrame = page->localMainFrame();
    if (!localMainFrame)
        return nullptr;

    return DocumentSVG::rootElement(*localMainFrame->document());
}

LocalFrameView* SVGImageInstance::frameView() const
{
    RefPtr page = this->page();
    if (!page)
        return nullptr;

    RefPtr localMainFrame = page->localMainFrame();
    if (!localMainFrame)
        return nullptr;

    return localMainFrame->view();
}

void SVGImageInstance::startAnimation()
{
    if (RefPtr rootElement = this->rootElement(); rootElement && rootElement->animationsPaused())
        rootElement->unpauseAnimations();
}

void SVGImageInstance::stopAnimation()
{
    if (RefPtr rootElement = this->rootElement())
        rootElement->pauseAnimations();
}

void SVGImageInstance::resetAnimation()
{
    if (RefPtr rootElement = this->rootElement())
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

bool SVGImageInstance::isAnimationPaused() const
{
    RefPtr rootElement = this->rootElement();
    return rootElement && rootElement->animationsPaused();
}

void SVGImageInstance::copyAnimationTimes(const SVGImageInstance& source)
{
    RefPtr rootElement = this->rootElement();
    if (!rootElement)
        return;
    RefPtr sourceRootElement = source.rootElement();
    if (!sourceRootElement)
        return;

    rootElement->setCurrentTime(sourceRootElement->getCurrentTime());

    // Both documents are parsed from the same data, so their animations should pair up in order.
    auto animations = protect(rootElement->document())->getAnimations();
    auto sourceAnimations = protect(sourceRootElement->document())->getAnimations();
    for (auto [animation, sourceAnimation] : zippedRange(animations, sourceAnimations))
        std::ignore = animation->setCurrentTime(sourceAnimation->currentTime());
}

size_t SVGImageInstance::approximateMemoryCost() const
{
    RefPtr localTopDocument = m_page ? protect(m_page)->localTopDocument() : nullptr;
    if (!localTopDocument)
        return 0;

    size_t cost = 0;
    for (RefPtr<Node> node = localTopDocument; node; node = NodeTraversal::next(*node))
        cost += node->approximateMemoryCost();
    return cost;
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

auto SVGImageInstance::documentStateForDraw(const ImageDrawDescriptor& descriptor) const -> DocumentState
{
    DocumentState state { roundedIntSize(descriptor.concreteObjectSize.size()), descriptor.rendition };

    if (!state.rendition.contentSettings)
        state.rendition.contentSettings = m_appliedDocumentState.rendition.contentSettings;

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (state.rendition.invertContent == InvertContent::FromResource)
        state.rendition.invertContent = m_fallbackInvertContent ? InvertContent::Yes : InvertContent::No;
    else if (state.rendition.invertContent == InvertContent::Forced)
        state.rendition.invertContent = InvertContent::Yes;
#endif

    return state;
}

void SVGImageInstance::applyDocumentState(const DocumentState& state)
{
    RefPtr page = m_page;
    if (!page)
        return;
    RefPtr localMainFrame = page->localMainFrame();
    if (!localMainFrame)
        return;
    RefPtr document = localMainFrame->document();
    if (!document)
        return;
    RefPtr frameView = localMainFrame->view();
    if (!frameView)
        return;

    setContainerSize(state.containerSize);

    if (state == m_appliedDocumentState)
        return;

    bool updateStyle = false;

    if (state.rendition.useDarkAppearance != m_appliedDocumentState.rendition.useDarkAppearance || state.rendition.useElevatedUserInterfaceLevel != m_appliedDocumentState.rendition.useElevatedUserInterfaceLevel) {
        page->setUseColorAppearance(state.rendition.useDarkAppearance, state.rendition.useElevatedUserInterfaceLevel);
        updateStyle = true;
    }

    if (state.rendition.contentSettings != m_appliedDocumentState.rendition.contentSettings) {
        Ref settings = page->settings();
        settings->setFontGenericFamilies(state.rendition.contentSettings->fontGenericFamilies);
        settings->setDownloadableBinaryFontTrustedTypes(state.rendition.contentSettings->downloadableBinaryFontTrustedTypes);
        updateStyle = true;
    }

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (state.rendition.invertContent != m_appliedDocumentState.rendition.invertContent) {
        page->settings().setAxCustomColorModeEnabled(state.rendition.invertContent == InvertContent::Yes);
        updateStyle = true;
    }
#endif

    // FIXME: webkit.org/b/322833 - the resource document holds one container's parameters at a time,
    // so this restyles it once per draw in the container.
    if (state.rendition.linkParameters != m_appliedDocumentState.rendition.linkParameters) {
        document->styleScope().environmentVariables().setLinkParameters(state.rendition.linkParameters);
        updateStyle = true;
    }

    if (state.rendition.fragmentURL != m_appliedDocumentState.rendition.fragmentURL || state.rendition.ignoreRootPreserveAspectRatio != m_appliedDocumentState.rendition.ignoreRootPreserveAspectRatio) {
        if (state.rendition.fragmentURL != m_appliedDocumentState.rendition.fragmentURL)
            frameView->scrollToFragment(state.rendition.fragmentURL);

        if (RefPtr rootElement = this->rootElement()) {
            rootElement->setIgnoresPreserveAspectRatioForSVGImage(state.rendition.ignoreRootPreserveAspectRatio == IgnoreRootPreserveAspectRatio::Yes);
            if (CheckedPtr renderer = rootElement->renderer())
                rootElement->invalidateCurrentView(*renderer);
        }

        updateStyle = true;
    }

    if (updateStyle) {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        document->updateStyleIfNeeded();
    }

    m_appliedDocumentState = state;
}

void SVGImageInstance::paint(GraphicsContext& context, const FloatRect& source)
{
    RefPtr frameView = this->frameView();
    if (!frameView)
        return;

    frameView->resize(m_appliedDocumentState.containerSize);

    {
        ScriptDisallowedScope::DisableAssertionsInScope disabledScope;
        if (frameView->needsLayout())
            protect(frameView->layoutContext())->layout();
    }

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(frameView->useDarkAppearance());
#endif

    frameView->paint(context, intersection(context.clipBounds(), enclosingIntRect(source)));
}

} // namespace WebCore
