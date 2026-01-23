/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#include "FullScreenMediaDetails.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "WebFrame.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include <WebCore/AddEventListenerOptionsInlines.h>
#include <WebCore/Color.h>
#include <WebCore/ContainerNodeInlines.h>
#include <WebCore/DocumentFullscreen.h>
#include <WebCore/DocumentQuirks.h>
#include <WebCore/DocumentView.h>
#include <WebCore/EventNames.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/JSDOMPromiseDeferred.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NodeDocument.h>
#include <WebCore/RenderImage.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderObjectInlines.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>
#include <WebCore/TreeScope.h>
#include <WebCore/TypedElementDescendantIteratorInlines.h>
#include <WebCore/UserGestureIndicator.h>
#include <ranges>
#include <wtf/LoggerHelper.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManager.h"
#include "VideoPresentationManager.h"
#endif

namespace WebKit {
using namespace WebCore;

using WebCore::FloatSize;

static WebCore::IntRect screenRectOfContents(WebCore::Element& element)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return { };

    IntRect contentsRect = renderer->absoluteBoundingBoxRect();
    if (contentsRect.isEmpty()) {
        // A zero-height element may contain visible overflow contents. If the element
        // itself is empty, traverse its children to find its visual content area.
        LayoutRect topLevelRect;
        contentsRect = snappedIntRect(renderer->paintingRootRect(topLevelRect));
    }

    if (contentsRect.isEmpty())
        return { };

    Ref frameView = renderer->view().frameView();

    // The element may have contents which are far outside the viewport of the page.
    // Clip the contents rect by the current viewport.
    auto viewportRect = snappedIntRect(frameView->layoutViewportRect());
    contentsRect.intersect(viewportRect);

    return frameView->contentsToScreen(contentsRect);
}

Ref<WebFullScreenManager> WebFullScreenManager::create(WebPage& page)
{
    return adoptRef(*new WebFullScreenManager(page));
}

WebFullScreenManager::WebFullScreenManager(WebPage& page)
    : WebCore::EventListener(WebCore::EventListener::CPPEventListenerType)
    , m_page(page)
#if ENABLE(VIDEO) && ENABLE(IMAGE_ANALYSIS)
    , m_mainVideoElementTextRecognitionTimer(RunLoop::mainSingleton(), "WebFullScreenManager::MainVideoElementTextRecognitionTimer"_s, this, &WebFullScreenManager::mainVideoElementTextRecognitionTimerFired)
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

    RefPtr currentPlaybackControlsElement = dynamicDowncast<WebCore::HTMLVideoElement>(m_page->protectedPlaybackSessionManager()->currentPlaybackControlsElement());
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
        Ref { *m_pipStandbyElement }->setVideoFullscreenStandby(false);

    m_pipStandbyElement = pipStandbyElement;

    if (m_pipStandbyElement)
        Ref { *m_pipStandbyElement }->setVideoFullscreenStandby(true);
#endif
}

bool WebFullScreenManager::supportsFullScreenForElement(const WebCore::Element& element, bool withKeyboard)
{
    if (!m_page->protectedCorePage()->isDocumentFullscreenEnabled())
        return false;

    if (m_inWindowFullScreenMode && &element != m_element)
        return false;

#if PLATFORM(IOS_FAMILY)
    return PAL::currentUserInterfaceIdiomIsDesktop() || !withKeyboard;
#else
    return true;
#endif
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

    m_element = element;
    m_elementToRestore = element;
    m_elementFrameIdentifier = element.document().frame()->frameID();

    for (auto& eventName : eventsToObserve())
        element.addEventListener(eventName, *this, { true });
}

void WebFullScreenManager::clearElement()
{
    RefPtr element = m_element;
    if (!element)
        return;

    for (auto& eventName : eventsToObserve())
        element->removeEventListener(eventName, *this, { true });

    m_element = nullptr;
    m_elementFrameIdentifier = std::nullopt;
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

void WebFullScreenManager::enterFullScreenForElement(Element& element, HTMLMediaElementEnums::VideoFullscreenMode mode, CompletionHandler<void(ExceptionOr<void>)>&& willEnterFullScreenCallback, CompletionHandler<bool(bool)>&& didEnterFullScreenCallback)
{
    ALWAYS_LOG(LOGIDENTIFIER, "<", element.tagName(), " id=\"", element.getIdAttribute(), "\">");

    setElement(element);

    FullScreenMediaDetails mediaDetails;
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    if (m_page->videoPresentationManager().videoElementInPictureInPicture() && m_element->protectedDocument()->quirks().blocksEnteringStandardFullscreenFromPictureInPictureQuirk()) {
        willEnterFullScreenCallback(Exception { ExceptionCode::NotAllowedError });
        didEnterFullScreenCallback(false);
        return;
    }

    if (RefPtr currentPlaybackControlsElement = m_page->protectedPlaybackSessionManager()->currentPlaybackControlsElement())
        currentPlaybackControlsElement->prepareForVideoFullscreenStandby();
#endif

#if PLATFORM(VISION) && ENABLE(QUICKLOOK_FULLSCREEN)
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(element.renderer()))
        mediaDetails = getImageMediaDetails(renderImage, IsUpdating::No);

    if (m_willUseQuickLookForFullscreen) {
        m_page->freezeLayerTree(WebPage::LayerTreeFreezeReason::OutOfProcessFullscreen);
        static constexpr auto maxViewportSize = FloatSize { 10000, 10000 };
        m_oldSize = m_page->viewportConfiguration().viewLayoutSize();
        m_scaleFactor = m_page->viewportConfiguration().layoutSizeScaleFactor();
        m_minEffectiveWidth = m_page->viewportConfiguration().minimumEffectiveDeviceWidth();
        m_page->setViewportConfigurationViewLayoutSize(maxViewportSize, m_scaleFactor, m_minEffectiveWidth);
    }
#endif

    m_initialFrame = screenRectOfContents(element);

#if ENABLE(VIDEO)
    updateMainVideoElement();

#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    if (RefPtr mainVideoElement = m_mainVideoElement.get()) {
        bool fullscreenElementIsVideoElement = is<HTMLVideoElement>(element);

        auto mainVideoElementSize = [&]() -> FloatSize {
#if PLATFORM(VISION)
            if (!fullscreenElementIsVideoElement && element.protectedDocument()->quirks().shouldDisableFullscreenVideoAspectRatioAdaptiveSizing())
                return { };
#endif
            return FloatSize(mainVideoElement->videoWidth(), mainVideoElement->videoHeight());
        }();

        mediaDetails = {
            fullscreenElementIsVideoElement ? FullScreenMediaDetails::Type::Video : FullScreenMediaDetails::Type::ElementWithVideo,
            mainVideoElementSize
        };
    }
#endif

    m_page->prepareToEnterElementFullScreen();

    if (RefPtr page = m_page->corePage()) {
        if (RefPtr view = page->protectedMainFrame()->virtualView())
            m_scrollPosition = view->scrollPosition();
    }

    if (mode == HTMLMediaElementEnums::VideoFullscreenModeInWindow) {
        willEnterFullScreen(element, WTF::move(willEnterFullScreenCallback), WTF::move(didEnterFullScreenCallback), mode);
        m_inWindowFullScreenMode = true;
    } else {
        ASSERT(m_elementFrameIdentifier);
        m_page->sendWithAsyncReply(Messages::WebFullScreenManagerProxy::EnterFullScreen(*m_elementFrameIdentifier, m_element->protectedDocument()->quirks().blocksReturnToFullscreenFromPictureInPictureQuirk(), WTF::move(mediaDetails)), [
            this,
            protectedThis = Ref { *this },
            element = Ref { element },
            willEnterFullScreenCallback = WTF::move(willEnterFullScreenCallback),
            didEnterFullScreenCallback = WTF::move(didEnterFullScreenCallback)
        ] (bool success) mutable {
            if (success) {
                willEnterFullScreen(element, WTF::move(willEnterFullScreenCallback), WTF::move(didEnterFullScreenCallback));
                return;
            }
            willEnterFullScreenCallback(Exception { ExceptionCode::InvalidStateError });
            didEnterFullScreenCallback(false);
        });
    }
#endif
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebFullScreenManager::updateImageSource(WebCore::Element& element)
{
    if (&element != m_element)
        return;

    FullScreenMediaDetails mediaDetails;
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(element.renderer());
    if (renderImage && m_willUseQuickLookForFullscreen) {
        mediaDetails = getImageMediaDetails(renderImage, IsUpdating::Yes);
        m_page->send(Messages::WebFullScreenManagerProxy::UpdateImageSource(WTF::move(mediaDetails)));
    }
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element, CompletionHandler<void()>&& completionHandler)
{
    if (element)
        ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");
    else
        ALWAYS_LOG(LOGIDENTIFIER, "null");

    m_page->prepareToExitElementFullScreen();

    if (m_inWindowFullScreenMode) {
        willExitFullScreen([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] mutable {
            didExitFullScreen(WTF::move(completionHandler));
            m_inWindowFullScreenMode = false;
        });
    } else {
        m_page->sendWithAsyncReply(Messages::WebFullScreenManagerProxy::ExitFullScreen(), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] mutable {
            willExitFullScreen(WTF::move(completionHandler));
        });
    }
#if ENABLE(VIDEO)
    setMainVideoElement(nullptr);
#endif
}

void WebFullScreenManager::willEnterFullScreen(Element& element, CompletionHandler<void(ExceptionOr<void>)>&& willEnterFullscreenCallback, CompletionHandler<bool(bool)>&& didEnterFullscreenCallback, WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ALWAYS_LOG(LOGIDENTIFIER, "<", element.tagName(), " id=\"", element.getIdAttribute(), "\">");

    m_page->isInFullscreenChanged(WebPage::IsInFullscreenMode::Yes);

    auto result = element.protectedDocument()->protectedFullscreen()->willEnterFullscreen(element, mode);
    if (result.hasException())
        close();
    willEnterFullscreenCallback(result);

#if !PLATFORM(IOS_FAMILY)
    m_page->hidePageBanners();
#endif
    element.protectedDocument()->updateLayout();
    m_finalFrame = screenRectOfContents(element);

    m_page->sendWithAsyncReply(Messages::WebFullScreenManagerProxy::BeganEnterFullScreen(m_initialFrame, m_finalFrame), [this, protectedThis = Ref { *this }, mode, completionHandler = WTF::move(didEnterFullscreenCallback)] (bool success) mutable {
        if (!success && mode != WebCore::HTMLMediaElementEnums::VideoFullscreenModeInWindow) {
            completionHandler(false);
            return;
        }
        didEnterFullScreen(WTF::move(completionHandler));
    });
}

void WebFullScreenManager::didEnterFullScreen(CompletionHandler<bool(bool)>&& completionHandler)
{
    RefPtr element = m_element;
    if (!element) {
        completionHandler(false);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");

    if (!completionHandler(true)) {
        close();
        return;
    }

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    RefPtr currentPlaybackControlsElement = m_page->protectedPlaybackSessionManager()->currentPlaybackControlsElement();
    setPIPStandbyElement(dynamicDowncast<WebCore::HTMLVideoElement>(currentPlaybackControlsElement.get()));
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
        for (Ref video : WebCore::descendantsOfType<WebCore::HTMLVideoElement>(*m_element)) {
            auto rendererAndBounds = video->boundingAbsoluteRectWithoutLayout();
            if (!rendererAndBounds)
                continue;

            auto& [renderer, bounds] = *rendererAndBounds;
            if (!renderer || bounds.isEmpty())
                continue;

            if (bounds.area() <= mainVideoBounds.area())
                continue;

            mainVideoBounds = bounds;
            mainVideo = WTF::move(video);
        }
        return mainVideo;
    }());
}

#endif // ENABLE(VIDEO)

void WebFullScreenManager::willExitFullScreen(CompletionHandler<void()>&& completionHandler)
{
    RefPtr element = m_element;
    if (!element || !m_elementFrameIdentifier)
        return completionHandler();
    ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");

#if ENABLE(VIDEO)
    setPIPStandbyElement(nullptr);
#endif

    m_finalFrame = screenRectOfContents(*element);
    if (!element->protectedDocument()->protectedFullscreen()->willExitFullscreen()) {
        close();
        return completionHandler();
    }
#if !PLATFORM(IOS_FAMILY)
    m_page->showPageBanners();
#endif
    // FIXME: The order of these frames is switched, but that is kept for historical reasons.
    // It should probably be fixed to be consistent at some point.
    m_page->sendWithAsyncReply(Messages::WebFullScreenManagerProxy::BeganExitFullScreen(*m_elementFrameIdentifier, m_finalFrame, m_initialFrame), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] mutable {
        didExitFullScreen(WTF::move(completionHandler));
    });
}

static Vector<Ref<Element>> collectFullscreenElementsFromElement(Element* rawElement)
{
    if (!rawElement)
        return { };

    RefPtr document = rawElement->document();

    if (rawElement != document->protectedFullscreen()->fullscreenElement())
        return { };

    RefPtr element = rawElement;

    Vector<Ref<Element>> fullscreenElements;

    while (true) {
        fullscreenElements.append(element.releaseNonNull());

        document = document->parentDocument();
        if (!document)
            break;

        element = document->protectedFullscreen()->fullscreenElement();
        if (!element)
            break;
    }

    return fullscreenElements;
}

void WebFullScreenManager::didExitFullScreen(CompletionHandler<void()>&& completionHandler)
{
#if PLATFORM(VISION) && ENABLE(QUICKLOOK_FULLSCREEN)
    if (std::exchange(m_willUseQuickLookForFullscreen, false)) {
        m_page->setViewportConfigurationViewLayoutSize(m_oldSize, m_scaleFactor, m_minEffectiveWidth);
        m_page->unfreezeLayerTree(WebPage::LayerTreeFreezeReason::OutOfProcessFullscreen);
    }
#endif

    m_page->isInFullscreenChanged(WebPage::IsInFullscreenMode::No);

    RefPtr element = m_element;
    if (!element)
        return completionHandler();

    ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");

    setFullscreenInsets(WebCore::FloatBoxExtent());
    setFullscreenAutoHideDuration(0_s);

    auto fullscreenElements = collectFullscreenElementsFromElement(element.get());

    completionHandler();

    // Ensure the element (and all its parent fullscreen elements) that just exited fullscreen are still in view:
    while (!fullscreenElements.isEmpty()) {
        auto fullscreenElement = fullscreenElements.takeLast();
        fullscreenElement->scrollIntoViewIfNotVisible(true, AllowScrollingOverflowHidden::No);
    }

    clearElement();

    if (RefPtr localMainFrame = m_page->protectedCorePage()->localMainFrame()) {
        // Make sure overflow: hidden is unapplied from the root element before restoring.
        RefPtr view = localMainFrame->view();
        view->forceLayout();
        view->setScrollPosition(m_scrollPosition);
    }
}

void WebFullScreenManager::setAnimatingFullScreen(bool animating)
{
    if (!m_element)
        return;
    m_element->protectedDocument()->protectedFullscreen()->setAnimatingFullscreen(animating);
}

void WebFullScreenManager::requestRestoreFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!m_element);
    if (m_element)
        return completionHandler(false);

    auto element = RefPtr { m_elementToRestore.get() };
    if (!element) {
        ALWAYS_LOG(LOGIDENTIFIER, "no element to restore");
        return completionHandler(false);
    }

    ALWAYS_LOG(LOGIDENTIFIER, "<", element->tagName(), " id=\"", element->getIdAttribute(), "\">");
    WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, &element->document());
    element->protectedDocument()->protectedFullscreen()->requestFullscreen(*element, WebCore::DocumentFullscreen::ExemptIFrameAllowFullscreenRequirement, [completionHandler = WTF::move(completionHandler)] (auto result) mutable {
        completionHandler(!result.hasException());
    });
}

void WebFullScreenManager::requestExitFullScreen()
{
    if (!m_element) {
        ALWAYS_LOG(LOGIDENTIFIER, "no element, closing");
        close();
        return;
    }

    RefPtr localMainFrame = m_page->localMainFrame();
    RefPtr topDocument = localMainFrame ? localMainFrame->document() : nullptr;
    if (!topDocument || !topDocument->protectedFullscreen()->fullscreenElement()) {
        ALWAYS_LOG(LOGIDENTIFIER, "top document not in fullscreen, closing");
        close();
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);
    m_element->protectedDocument()->protectedFullscreen()->fullyExitFullscreen();
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

void WebFullScreenManager::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page->protectedCorePage()->setFullscreenInsets(insets);
}

void WebFullScreenManager::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page->protectedCorePage()->setFullscreenAutoHideDuration(duration);
}

void WebFullScreenManager::handleEvent(WebCore::ScriptExecutionContext& context, WebCore::Event& event)
{
#if ENABLE(VIDEO)
    RefPtr targetElement = dynamicDowncast<WebCore::Element>(event.currentTarget());
    if (!m_element || !targetElement)
        return;

    Ref document = m_element->document();
    if (&context != document.ptr() || !document->protectedFullscreen()->isFullscreen())
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
    if (!m_element || !m_element->protectedDocument()->protectedFullscreen()->isFullscreen())
        return;

    updateMainVideoElement();

    RefPtr mainVideoElement = m_mainVideoElement.get();
    if (!mainVideoElement)
        return;

    if (m_isPerformingTextRecognitionInMainVideo)
        m_page->cancelTextRecognitionForVideoInElementFullScreen();

    m_isPerformingTextRecognitionInMainVideo = true;
    m_page->beginTextRecognitionForVideoInElementFullScreen(*mainVideoElement);
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

    if (RefPtr mainVideoElement = m_mainVideoElement.get()) {
        for (auto& eventName : eventsToObserve.get())
            mainVideoElement->removeEventListener(eventName, *this, { });

#if ENABLE(IMAGE_ANALYSIS)
        endTextRecognitionForMainVideoIfNeeded();
#endif
    }

    m_mainVideoElement = element;

    if (element) {
        for (auto& eventName : eventsToObserve.get())
            element->addEventListener(eventName, *this, { });

#if ENABLE(IMAGE_ANALYSIS)
        if (element->paused())
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

void WebFullScreenManager::enterFullScreenForOwnerElements(WebCore::FrameIdentifier frameID, CompletionHandler<void()>&& completionHandler)
{
    RefPtr webFrame = WebFrame::webFrame(frameID);
    if (!webFrame)
        return completionHandler();
    RefPtr coreFrame = webFrame->coreFrame();
    if (!coreFrame)
        return completionHandler();

    Vector<Ref<Element>> elements;
    for (RefPtr frame = coreFrame; frame; frame = frame->tree().parent()) {
        if (RefPtr element = frame->ownerElement())
            elements.append(element.releaseNonNull());
    }
    for (auto element : elements | std::views::reverse)
        DocumentFullscreen::elementEnterFullscreen(element);

    completionHandler();
}

void WebFullScreenManager::exitFullScreenInMainFrame(CompletionHandler<void()>&& completionHandler)
{
    RefPtr mainFrame = m_page->mainFrame();
    if (!mainFrame)
        return completionHandler();

    DocumentFullscreen::finishExitFullscreen(*mainFrame, DocumentFullscreen::ExitMode::Resize);
    completionHandler();
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
