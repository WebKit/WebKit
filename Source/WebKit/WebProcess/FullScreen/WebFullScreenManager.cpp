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
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/Quirks.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>
#include <WebCore/TypedElementDescendantIterator.h>
#include <WebCore/UserGestureIndicator.h>

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManager.h"
#endif

namespace WebKit {

using WebCore::FloatSize;

static WebCore::IntRect screenRectOfContents(WebCore::Element* element)
{
    ASSERT(element);
    if (!element)
        return { };

    if (element->renderer() && element->renderer()->hasLayer() && element->renderer()->enclosingLayer()->isComposited()) {
        WebCore::FloatQuad contentsBox = static_cast<WebCore::FloatRect>(element->renderer()->enclosingLayer()->backing()->contentsBox());
        contentsBox = element->renderer()->localToAbsoluteQuad(contentsBox);
        return element->renderer()->view().frameView().contentsToScreen(contentsBox.enclosingBoundingBox());
    }

    return element->screenRect();
}

Ref<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return adoptRef(*new WebFullScreenManager(page));
}

WebFullScreenManager::WebFullScreenManager(WebPage* page)
    : WebCore::EventListener(WebCore::EventListener::CPPEventListenerType)
    , m_page(page)
#if ENABLE(VIDEO)
    , m_mainVideoElementTextRecognitionTimer(RunLoop::main(), this, &WebFullScreenManager::mainVideoElementTextRecognitionTimerFired)
#endif
{
}
    
WebFullScreenManager::~WebFullScreenManager()
{
}

WebCore::Element* WebFullScreenManager::element() 
{ 
    return m_element.get(); 
}

void WebFullScreenManager::videoControlsManagerDidChange()
{
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    LOG(Fullscreen, "WebFullScreenManager %p videoControlsManagerDidChange()", this);

    auto* currentPlaybackControlsElement = m_page->playbackSessionManager().currentPlaybackControlsElement();
    if (!m_element || !is<WebCore::HTMLVideoElement>(currentPlaybackControlsElement)) {
        setPIPStandbyElement(nullptr);
        return;
    }

    setPIPStandbyElement(downcast<WebCore::HTMLVideoElement>(currentPlaybackControlsElement));
#endif
}

void WebFullScreenManager::setPIPStandbyElement(WebCore::HTMLVideoElement* pipStandbyElement)
{
#if ENABLE(VIDEO)
    if (pipStandbyElement == m_pipStandbyElement)
        return;

    LOG(Fullscreen, "WebFullScreenManager %p setPIPStandbyElement() - old element %p, new element %p", this, m_pipStandbyElement.get(), pipStandbyElement);

    if (m_pipStandbyElement)
        m_pipStandbyElement->setVideoFullscreenStandby(false);

    m_pipStandbyElement = pipStandbyElement;

    if (m_pipStandbyElement)
        m_pipStandbyElement->setVideoFullscreenStandby(true);
#endif
}

void WebFullScreenManager::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    didReceiveWebFullScreenManagerMessage(connection, decoder);
}

bool WebFullScreenManager::supportsFullScreen(bool withKeyboard)
{
    if (!m_page->corePage()->settings().fullScreenEnabled())
        return false;

    return m_page->injectedBundleFullScreenClient().supportsFullScreen(m_page.get(), withKeyboard);
}

void WebFullScreenManager::setElement(WebCore::Element& element)
{
    if (m_element == &element)
        return;

    static NeverDestroyed eventsToObserve = std::array {
        WebCore::eventNames().playEvent,
        WebCore::eventNames().pauseEvent,
        WebCore::eventNames().loadedmetadataEvent,
    };

    if (m_element) {
        for (auto& eventName : eventsToObserve.get())
            m_element->removeEventListener(eventName, *this, { true });
    }

    m_element = &element;

    for (auto& eventName : eventsToObserve.get())
        m_element->addEventListener(eventName, *this, { true });
}

void WebFullScreenManager::enterFullScreenForElement(WebCore::Element* element)
{
    LOG(Fullscreen, "WebFullScreenManager %p enterFullScreenForElement(%p)", this, element);

    ASSERT(element);
    if (!element)
        return;

    setElement(*element);

    bool isVideoElement = false;
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (auto* currentPlaybackControlsElement = m_page->playbackSessionManager().currentPlaybackControlsElement()) {
        currentPlaybackControlsElement->prepareForVideoFullscreenStandby();
        isVideoElement = currentPlaybackControlsElement->controls();
    }
#endif

    m_initialFrame = screenRectOfContents(m_element.get());
#if ENABLE(VIDEO)
    updateMainVideoElement();
    auto videoDimensions = m_mainVideoElement ? FloatSize(m_mainVideoElement->videoWidth(), m_mainVideoElement->videoHeight()) : FloatSize(m_initialFrame.width(), m_initialFrame.height());
#else
    FloatSize videoDimensions;
#endif
    m_page->injectedBundleFullScreenClient().enterFullScreenForElement(m_page.get(), element, m_element->document().quirks().blocksReturnToFullscreenFromPictureInPictureQuirk(), isVideoElement, videoDimensions);
}

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element)
{
    LOG(Fullscreen, "WebFullScreenManager %p exitFullScreenForElement(%p) - fullscreen element %p", this, element, m_element.get());
    m_page->injectedBundleFullScreenClient().exitFullScreenForElement(m_page.get(), element);
#if ENABLE(VIDEO)
    setMainVideoElement(nullptr);
#endif
}

void WebFullScreenManager::willEnterFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p willEnterFullScreen() - element %p", this, m_element.get());
    ASSERT(m_element);
    if (!m_element)
        return;

    if (!m_element->document().fullscreenManager().willEnterFullscreen(*m_element)) {
        close();
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    m_page->hidePageBanners();
#endif
    m_element->document().updateLayout();
    m_finalFrame = screenRectOfContents(m_element.get());
    m_page->injectedBundleFullScreenClient().beganEnterFullScreen(m_page.get(), m_initialFrame, m_finalFrame);
}

void WebFullScreenManager::didEnterFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p didEnterFullScreen() - element %p", this, m_element.get());
    ASSERT(m_element);
    if (!m_element)
        return;

    if (!m_element->document().fullscreenManager().didEnterFullscreen()) {
        close();
        return;
    }

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

            auto [renderer, bounds] = *rendererAndBounds;
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
    LOG(Fullscreen, "WebFullScreenManager %p willExitFullScreen() - element %p", this, m_element.get());
    ASSERT(m_element);
    if (!m_element)
        return;

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
    m_page->injectedBundleFullScreenClient().beganExitFullScreen(m_page.get(), m_finalFrame, m_initialFrame);
}

void WebFullScreenManager::didExitFullScreen()
{
    LOG(Fullscreen, "WebFullScreenManager %p didExitFullScreen() - element %p", this, m_element.get());
    ASSERT(m_element);
    if (!m_element)
        return;

    setFullscreenInsets(WebCore::FloatBoxExtent());
    setFullscreenAutoHideDuration(0_s);
    m_element->document().fullscreenManager().didExitFullscreen();
}

void WebFullScreenManager::setAnimatingFullScreen(bool animating)
{
    ASSERT(m_element);
    if (!m_element)
        return;
    m_element->document().fullscreenManager().setAnimatingFullscreen(animating);
}

void WebFullScreenManager::requestEnterFullScreen()
{
    ASSERT(m_element);
    if (!m_element)
        return;

    WebCore::UserGestureIndicator gestureIndicator(WebCore::ProcessingUserGesture);
    m_element->document().fullscreenManager().requestFullscreenForElement(*m_element, WebCore::FullscreenManager::ExemptIFrameAllowFullscreenRequirement);
}

void WebFullScreenManager::requestExitFullScreen()
{
    ASSERT(m_element);
    if (!m_element) {
        close();
        return;
    }

    auto& topDocument = m_element->document().topDocument();
    if (!topDocument.fullscreenManager().fullscreenElement()) {
        close();
        return;
    }
    m_element->document().fullscreenManager().cancelFullscreen();
}

void WebFullScreenManager::close()
{
    if (m_closing)
        return;
    m_closing = true;
    LOG(Fullscreen, "WebFullScreenManager %p close()", this);
    m_page->injectedBundleFullScreenClient().closeFullScreen(m_page.get());
#if ENABLE(VIDEO)
    setMainVideoElement(nullptr);
#endif
    m_closing = false;
}

void WebFullScreenManager::saveScrollPosition()
{
    m_scrollPosition = m_page->corePage()->mainFrame().view()->scrollPosition();
}

void WebFullScreenManager::restoreScrollPosition()
{
    m_page->corePage()->mainFrame().view()->setScrollPosition(m_scrollPosition);
}

void WebFullScreenManager::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page->corePage()->setFullscreenInsets(insets);
}

void WebFullScreenManager::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page->corePage()->setFullscreenAutoHideDuration(duration);
}

void WebFullScreenManager::setFullscreenControlsHidden(bool hidden)
{
    m_page->corePage()->setFullscreenControlsHidden(hidden);
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

    if (targetElement == m_mainVideoElement.get()) {
        auto& targetVideoElement = downcast<WebCore::HTMLVideoElement>(*targetElement);
        if (targetVideoElement.paused() && !targetVideoElement.seeking())
            scheduleTextRecognitionForMainVideo();
        else
            endTextRecognitionForMainVideoIfNeeded();
    }
#else
    UNUSED_PARAM(event);
    UNUSED_PARAM(context);
#endif
}

#if ENABLE(VIDEO)

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

        endTextRecognitionForMainVideoIfNeeded();
    }

    m_mainVideoElement = WTFMove(element);

    if (m_mainVideoElement) {
        for (auto& eventName : eventsToObserve.get())
            m_mainVideoElement->addEventListener(eventName, *this, { });

        if (m_mainVideoElement->paused())
            scheduleTextRecognitionForMainVideo();
    }
}

#endif // ENABLE(VIDEO)

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
