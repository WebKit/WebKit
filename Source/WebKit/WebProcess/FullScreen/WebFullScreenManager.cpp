/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFullScreenManager.h"

#if ENABLE(FULLSCREEN_API)

#include "Connection.h"
#include "Logging.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include <WebCore/AddEventListenerOptions.h>
#include <WebCore/Color.h>
#include <WebCore/EventNames.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/JSDOMPromiseDeferred.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/Quirks.h>
#include <WebCore/RenderImage.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>
#include <WebCore/TypedElementDescendantIteratorInlines.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/LoggerHelper.h>

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManager.h"
#include "VideoPresentationManager.h"
#endif

namespace WebKit {
using namespace WebCore;

using WebCore::FloatSize;

static WebCore::IntRect screenRectOfContents(WebCore::Element* element)
{
    ASSERT(element);
    if (!element)
        return { };

    if (element->renderer() && element->renderer()->hasLayer() && element->renderer()->enclosingLayer()->isComposited()) {
        WebCore::FloatQuad contentsBox = static_cast<WebCore::FloatRect>(element->renderer()->enclosingLayer()->backing()->compositedBounds());
        contentsBox = element->renderer()->localToAbsoluteQuad(contentsBox);
        return element->renderer()->view().frameView().contentsToScreen(contentsBox.enclosingBoundingBox());
    }

    return element->screenRect();
}

Ref<WebFullScreenManager> WebFullScreenManager::create(WebPage& page)
{
    return adoptRef(*new WebFullScreenManager(page));
}

WebFullScreenManager::WebFullScreenManager(WebPage& page)
    : WebCore::EventListener(WebCore::EventListener::CPPEventListenerType)
    , m_page(page)
#if ENABLE(VIDEO) && ENABLE(IMAGE_ANALYSIS)
    , m_mainVideoElementTextRecognitionTimer(RunLoop::main(), this, &WebFullScreenManager::mainVideoElementTextRecognitionTimerFired)
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(page.logIdentifier())
#endif
{
}
    
WebFullScreenManager::~WebFullScreenManager()
{
    invalidate();
}

void WebFullScreenManager::invalidate()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    clearElement();
#if ENABLE(VIDEO)
    setMainVideoElement(nullptr);
#if ENABLE(IMAGE_ANALYSIS)
    m_mainVideoElementTextRecognitionTimer.stop();
#endif
#endif
}

WebCore::Element* WebFullScreenManager::element()
{ 
    return m_element.get(); 
}

void WebFullScreenManager::videoControlsManagerDidChange()
{
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!m_element) {
        setPIPStandbyElement(nullptr);
        return;
    }

    RefPtr currentPlaybackControlsElement = dynamicDowncast<WebCore::HTMLVideoElement>(m_page->playbackSessionManager().currentPlaybackControlsElement());
    if (!currentPlaybackControlsElement) {
        setPIPStandbyElement(nullptr);
        return;
    }

    setPIPStandbyElement(currentPlaybackControlsElement.get());
#endif
}

void WebFullScreenManager::setPIPStandbyElement(WebCore::HTMLVideoElement* pipStandbyElement)
{
#if ENABLE(VIDEO)
    if (pipStandbyElement == m_pipStandbyElement)
        return;

#if !RELEASE_LOG_DISABLED
    auto logIdentifierForElement = [] (auto* element) { return element ? element->logIdentifier() : 0; };
#endif
    ALWAYS_LOG(LOGIDENTIFIER, "old element ", logIdentifierForElement(m_pipStandbyElement.get()), ", new element ", logIdentifierForElement(pipStandbyElement));

    if (m_pipStandbyElement)
        m_pipStandbyElement->setVideoFullscreenStandby(false);

    m_pipStandbyElement = pipStandbyElement;

    if (m_pipStandbyElement)
        m_pipStandbyElement->setVideoFullscreenStandby(true);
#endif
}

bool WebFullScreenManager::supportsFullScreenForElement(const WebCore::Element& element, bool withKeyboard)
{
    if (!m_page->corePage()->isFullscreenManagerEnabled())
        return false;

    return m_page->injectedBundleFullScreenClient().supportsFullScreen(m_page.ptr(), withKeyboard);
}

static auto& eventsToObserve()
{
    static NeverDestroyed eventsToObserve = std::array {
        WebCore::eventNames().playEvent,
        WebCore::eventNames().pauseEvent,
        WebCore::eventNames().loadedmetadataEvent,
    };
    return eventsToObserve.get();
}

void WebFullScreenManager::setElement(WebCore::Element& element)
{
    if (m_element == &element)
        return;

    clearElement();

    m_element = &element;
    m_elementToRestore = element;

    for (auto& eventName : eventsToObserve())
        m_element->addEventListener(eventName, *this, { true });
}

void WebFullScreenManager::clearElement()
{
    if (!m_element)
        return;
    for (auto& eventName : eventsToObserve())
        m_element->removeEventListener(eventName, *this, { true });
    m_element = nullptr;
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
FullScreenMediaDetails WebFullScreenManager::getImageMediaDetails(CheckedPtr<RenderImage> renderImage, IsUpdating updating)
{
    if (!renderImage)
        return { };

    auto* cachedImage = renderImage->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return { };

    RefPtr image = cachedImage->image();
    if (!image)
        return { };
    if (!(image->shouldUseQuickLookForFullscreen() || updating == IsUpdating::Yes))
        return { };

    auto* buffer = cachedImage->resourceBuffer();
    if (!buffer)
        return { };

    auto imageSize = image->size();

    auto mimeType = image->mimeType();
    if (!MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        mimeType = MIMETypeRegistry::mimeTypeForExtension(image->filenameExtension());
    if (!MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        mimeType = MIMETypeRegistry::mimeTypeForPath(cachedImage->url().string());
    if (!MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return { };

    auto sharedMemoryBuffer = SharedMemory::copyBuffer(*buffer);
    if (!sharedMemoryBuffer)
        return { };

    auto imageResourceHandle = sharedMemoryBuffer->createHandle(SharedMemory::Protection::ReadOnly);

    m_willUseQuickLookForFullscreen = true;

    return {
        FullScreenMediaDetails::Type::Image,
        imageSize,
        mimeType,
        imageResourceHandle
    };
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebFullScreenManager::enterFullScreenForElement(WebCore::Element* element, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(element);
    if (!element)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");

    setElement(*element);


    FullScreenMediaDetails mediaDetails;
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (m_page->videoPresentationManager().videoElementInPictureInPicture() && m_element->document().quirks().blocksEnteringStandardFullscreenFromPictureInPictureQuirk())
        return;

    if (auto* currentPlaybackControlsElement = m_page->playbackSessionManager().currentPlaybackControlsElement())
        currentPlaybackControlsElement->prepareForVideoFullscreenStandby();
#endif

#if PLATFORM(VISION) && ENABLE(QUICKLOOK_FULLSCREEN)
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(element->renderer()))
        mediaDetails = getImageMediaDetails(renderImage, IsUpdating::No);

    if (m_willUseQuickLookForFullscreen)
        m_page->freezeLayerTree(WebPage::LayerTreeFreezeReason::OutOfProcessFullscreen);
#endif

    m_initialFrame = screenRectOfContents(m_element.get());

#if ENABLE(VIDEO)
    updateMainVideoElement();

#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    if (m_mainVideoElement) {
        bool fullscreenElementIsVideoElement = is<HTMLVideoElement>(element);

        auto mainVideoElementSize = [&]() -> FloatSize {
#if PLATFORM(VISION)
            if (!fullscreenElementIsVideoElement && element->document().quirks().shouldDisableFullscreenVideoAspectRatioAdaptiveSizing())
                return { };
#endif
            return FloatSize(m_mainVideoElement->videoWidth(), m_mainVideoElement->videoHeight());
        }();

        mediaDetails = {
            fullscreenElementIsVideoElement ? FullScreenMediaDetails::Type::Video : FullScreenMediaDetails::Type::ElementWithVideo,
            mainVideoElementSize
        };
    }
#endif

    m_page->prepareToEnterElementFullScreen();
    m_page->injectedBundleFullScreenClient().enterFullScreenForElement(m_page.ptr(), element, m_element->document().quirks().blocksReturnToFullscreenFromPictureInPictureQuirk(), mode, WTFMove(mediaDetails));

    if (mode == WebCore::HTMLMediaElementEnums::VideoFullscreenModeInWindow) {
        willEnterFullScreen(mode);
        didEnterFullScreen();
        m_inWindowFullScreenMode = true;
    }
#endif
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebFullScreenManager::updateImageSource(WebCore::Element& element)
{
    FullScreenMediaDetails mediaDetails;
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(element.renderer());
    if (renderImage && m_willUseQuickLookForFullscreen) {
        mediaDetails = getImageMediaDetails(renderImage, IsUpdating::Yes);
        m_page->send(Messages::WebFullScreenManagerProxy::UpdateImageSource(WTFMove(mediaDetails)));
    }
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element)
{
    if (element)
        ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");
    else
        ALWAYS_LOG(LOGIDENTIFIER, "null");

    m_page->prepareToExitElementFullScreen();
    m_page->injectedBundleFullScreenClient().exitFullScreenForElement(m_page.ptr(), element, m_inWindowFullScreenMode);

    if (m_inWindowFullScreenMode) {
        willExitFullScreen();
        didExitFullScreen();
        m_inWindowFullScreenMode = false;
    }
#if ENABLE(VIDEO)
    setMainVideoElement(nullptr);
#endif
}

void WebFullScreenManager::willEnterFullScreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT(m_element);
    if (!m_element)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "<", m_element->tagName(), " id=\"", m_element->getIdAttribute(), "\">");

    m_page->isInFullscreenChanged(WebPage::IsInFullscreenMode::Yes);

    if (!m_element->document().fullscreenManager().willEnterFullscreen(*m_element, mode)) {
        close();
        return;
    }

    if (!m_element) {
        close();
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    m_page->hidePageBanners();
#endif
    m_element->protectedDocument()->updateLayout();
    m_finalFrame = screenRectOfContents(m_element.get());
    m_page->injectedBundleFullScreenClient().beganEnterFullScreen(m_page.ptr(), m_initialFrame, m_finalFrame);
}

void WebFullScreenManager::didEnterFullScreen()
{
    ASSERT(m_element);
    if (!m_element)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "<", m_element->tagName(), " id=\"", m_element->getIdAttribute(), "\">");

    if (!m_element->document().fullscreenManager().didEnterFullscreen()) {
        close();
        return;
    }

#if ENABLE(QUICKLOOK_FULLSCREEN)
    static constexpr auto maxViewportSize = FloatSize { 10000, 10000 };
    if (m_willUseQuickLookForFullscreen) {
        m_oldSize = m_page->viewportConfiguration().viewLayoutSize();
        m_scaleFactor = m_page->viewportConfiguration().layoutSizeScaleFactor();
        m_minEffectiveWidth = m_page->viewportConfiguration().minimumEffectiveDeviceWidth();
        m_page->setViewportConfigurationViewLayoutSize(maxViewportSize, m_scaleFactor, m_minEffectiveWidth);
    }
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    auto* currentPlaybackControlsElement = m_page->playbackSessionManager().currentPlaybackControlsElement();
    setPIPStandbyElement(dynamicDowncast<WebCore::HTMLVideoElement>(currentPlaybackControlsElement));
#endif

#if ENABLE(VIDEO)
    updateMainVideoElement();
#endif
}

#if ENABLE(VIDEO)

void WebFullScreenManager::updateMainVideoElement()
{
    setMainVideoElement([&]() -> RefPtr<WebCore::HTMLVideoElement> {
        if (!m_element)
            return nullptr;

        if (auto video = dynamicDowncast<WebCore::HTMLVideoElement>(*m_element))
            return video;

        RefPtr<WebCore::HTMLVideoElement> mainVideo;
        WebCore::FloatRect mainVideoBounds;
        for (auto& video : WebCore::descendantsOfType<WebCore::HTMLVideoElement>(*m_element)) {
            auto rendererAndBounds = video.boundingAbsoluteRectWithoutLayout();
            if (!rendererAndBounds)
                continue;

            auto& [renderer, bounds] = *rendererAndBounds;
            if (!renderer || bounds.isEmpty())
                continue;

            if (bounds.area() <= mainVideoBounds.area())
                continue;

            mainVideoBounds = bounds;
            mainVideo = &video;
        }
        return mainVideo;
    }());
}

#endif // ENABLE(VIDEO)

void WebFullScreenManager::willExitFullScreen()
{
    if (!m_element)
        return;
    ALWAYS_LOG(LOGIDENTIFIER, "<", m_element->tagName(), " id=\"", m_element->getIdAttribute(), "\">");

#if ENABLE(VIDEO)
    setPIPStandbyElement(nullptr);
#endif

    m_finalFrame = screenRectOfContents(m_element.get());
    if (!m_element->document().fullscreenManager().willExitFullscreen()) {
        close();
        return;
    }
#if !PLATFORM(IOS_FAMILY)
    m_page->showPageBanners();
#endif
    m_page->injectedBundleFullScreenClient().beganExitFullScreen(m_page.ptr(), m_finalFrame, m_initialFrame);
}

static Vector<Ref<Element>> collectFullscreenElementsFromElement(Element* element)
{
    Vector<Ref<Element>> fullscreenElements;

    while (element && element->document().fullscreenManager().fullscreenElement() == element) {
        fullscreenElements.append(*element);
        auto parentDocument = element->document().parentDocument();
        element = parentDocument ? parentDocument->fullscreenManager().fullscreenElement() : nullptr;
    }

    return fullscreenElements;
}

void WebFullScreenManager::didExitFullScreen()
{
#if PLATFORM(VISION) && ENABLE(QUICKLOOK_FULLSCREEN)
    if (std::exchange(m_willUseQuickLookForFullscreen, false)) {
        m_page->setViewportConfigurationViewLayoutSize(m_oldSize, m_scaleFactor, m_minEffectiveWidth);
        m_page->unfreezeLayerTree(WebPage::LayerTreeFreezeReason::OutOfProcessFullscreen);
    }
#endif

    m_page->isInFullscreenChanged(WebPage::IsInFullscreenMode::No);

    if (!m_element)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "<", m_element->tagName(), " id=\"", m_element->getIdAttribute(), "\">");

    setFullscreenInsets(WebCore::FloatBoxExtent());
    setFullscreenAutoHideDuration(0_s);

    auto fullscreenElements = collectFullscreenElementsFromElement(m_element.get());

    m_element->document().fullscreenManager().didExitFullscreen();

    // Ensure the element (and all its parent fullscreen elements) that just exited fullscreen are still in view:
    while (!fullscreenElements.isEmpty()) {
        auto element = fullscreenElements.takeLast();
        element->scrollIntoViewIfNotVisible(true);
    }

    clearElement();
}

void WebFullScreenManager::setAnimatingFullScreen(bool animating)
{
    ASSERT(m_element);
    if (!m_element)
        return;
    m_element->document().fullscreenManager().setAnimatingFullscreen(animating);
}

void WebFullScreenManager::requestRestoreFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!m_element);
    if (m_element)
        return;

    auto element = RefPtr { m_elementToRestore.get() };
    if (!element) {
        ALWAYS_LOG(LOGIDENTIFIER, "no element to restore");
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");
    WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, &element->document());
    element->document().fullscreenManager().requestFullscreenForElement(*element, nullptr, WebCore::FullscreenManager::ExemptIFrameAllowFullscreenRequirement, WTFMove(completionHandler));
}

void WebFullScreenManager::requestExitFullScreen()
{
    if (!m_element) {
        ALWAYS_LOG(LOGIDENTIFIER, "no element, closing");
        close();
        return;
    }

    auto& topDocument = m_element->document().topDocument();
    if (!topDocument.fullscreenManager().fullscreenElement()) {
        ALWAYS_LOG(LOGIDENTIFIER, "top document not in fullscreen, closing");
        close();
        return;
    }
    ALWAYS_LOG(LOGIDENTIFIER);
    m_element->document().fullscreenManager().cancelFullscreen();
}

void WebFullScreenManager::close()
{
    if (m_closing)
        return;
    m_closing = true;
    ALWAYS_LOG(LOGIDENTIFIER);
    m_page->closeFullScreen();
    invalidate();
    m_closing = false;
}

void WebFullScreenManager::saveScrollPosition()
{
    m_scrollPosition = m_page->corePage()->mainFrame().virtualView()->scrollPosition();
}

void WebFullScreenManager::restoreScrollPosition()
{
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->corePage()->mainFrame())) {
        // Make sure overflow: hidden is unapplied from the root element before restoring.
        localMainFrame->view()->forceLayout();
        localMainFrame->view()->setScrollPosition(m_scrollPosition);
    }
}

void WebFullScreenManager::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page->corePage()->setFullscreenInsets(insets);
}

void WebFullScreenManager::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page->corePage()->setFullscreenAutoHideDuration(duration);
}

void WebFullScreenManager::handleEvent(WebCore::ScriptExecutionContext& context, WebCore::Event& event)
{
#if ENABLE(VIDEO)
    RefPtr targetElement = dynamicDowncast<WebCore::Element>(event.currentTarget());
    if (!m_element || !targetElement)
        return;

    Ref document = m_element->document();
    if (&context != document.ptr() || !document->fullscreenManager().isFullscreen())
        return;

    if (targetElement == m_element) {
        updateMainVideoElement();
        return;
    }

#if ENABLE(IMAGE_ANALYSIS)
    if (targetElement == m_mainVideoElement.get()) {
        auto& targetVideoElement = downcast<WebCore::HTMLVideoElement>(*targetElement);
        if (targetVideoElement.paused() && !targetVideoElement.seeking())
            scheduleTextRecognitionForMainVideo();
        else
            endTextRecognitionForMainVideoIfNeeded();
    }
#endif
#else
    UNUSED_PARAM(event);
    UNUSED_PARAM(context);
#endif
}

#if ENABLE(VIDEO)

#if ENABLE(IMAGE_ANALYSIS)
void WebFullScreenManager::mainVideoElementTextRecognitionTimerFired()
{
    if (!m_element || !m_element->document().fullscreenManager().isFullscreen())
        return;

    updateMainVideoElement();

    if (!m_mainVideoElement)
        return;

    if (m_isPerformingTextRecognitionInMainVideo)
        m_page->cancelTextRecognitionForVideoInElementFullScreen();

    m_isPerformingTextRecognitionInMainVideo = true;
    m_page->beginTextRecognitionForVideoInElementFullScreen(*m_mainVideoElement);
}

void WebFullScreenManager::scheduleTextRecognitionForMainVideo()
{
    m_mainVideoElementTextRecognitionTimer.startOneShot(250_ms);
}

void WebFullScreenManager::endTextRecognitionForMainVideoIfNeeded()
{
    m_mainVideoElementTextRecognitionTimer.stop();

    if (m_isPerformingTextRecognitionInMainVideo) {
        m_page->cancelTextRecognitionForVideoInElementFullScreen();
        m_isPerformingTextRecognitionInMainVideo = false;
    }
}
#endif // ENABLE(IMAGE_ANALYSIS)

void WebFullScreenManager::setMainVideoElement(RefPtr<WebCore::HTMLVideoElement>&& element)
{
    if (element == m_mainVideoElement.get())
        return;

    static NeverDestroyed eventsToObserve = std::array {
        WebCore::eventNames().seekingEvent,
        WebCore::eventNames().seekedEvent,
        WebCore::eventNames().playingEvent,
        WebCore::eventNames().pauseEvent,
    };

    if (m_mainVideoElement) {
        for (auto& eventName : eventsToObserve.get())
            m_mainVideoElement->removeEventListener(eventName, *this, { });

#if ENABLE(IMAGE_ANALYSIS)
        endTextRecognitionForMainVideoIfNeeded();
#endif
    }

    m_mainVideoElement = WTFMove(element);

    if (m_mainVideoElement) {
        for (auto& eventName : eventsToObserve.get())
            m_mainVideoElement->addEventListener(eventName, *this, { });

#if ENABLE(IMAGE_ANALYSIS)
        if (m_mainVideoElement->paused())
            scheduleTextRecognitionForMainVideo();
#endif
    }
}

#endif // ENABLE(VIDEO)

#if !RELEASE_LOG_DISABLED
WTFLogChannel& WebFullScreenManager::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
