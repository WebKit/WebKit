/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "VideoPresentationModelVideoElement.h"

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "AddEventListenerOptions.h"
#import "DocumentFullscreen.h"
#import "DocumentInlines.h"
#import "Event.h"
#import "EventListener.h"
#import "EventNames.h"
#import "HTMLElement.h"
#import "HTMLVideoElement.h"
#import "History.h"
#import "LocalDOMWindow.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "NodeInlines.h"
#import "Page.h"
#import "PlaybackSessionModelMediaElement.h"
#import "TextTrackList.h"
#import "TimeRanges.h"
#import "UserGestureIndicator.h"
#import <QuartzCore/CoreAnimation.h>
#import <wtf/LoggerHelper.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>

namespace WebCore {

VideoPresentationModelVideoElement::VideoListener::VideoListener(VideoPresentationModelVideoElement& parent)
    : EventListener(EventListener::CPPEventListenerType)
    , m_parent(parent)
{
}

void VideoPresentationModelVideoElement::VideoListener::handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event& event)
{
    if (auto parent = m_parent.get())
        parent->updateForEventName(event.type());
}

VideoPresentationModelVideoElement::VideoPresentationModelVideoElement()
    : m_videoListener(VideoListener::create(*this))
{
}

VideoPresentationModelVideoElement::~VideoPresentationModelVideoElement()
{
    cleanVideoListeners();
}

void VideoPresentationModelVideoElement::cleanVideoListeners()
{
    if (!m_isListening)
        return;
    m_isListening = false;
    if (RefPtr videoElement = m_videoElement) {
        videoElement->removeClient(*this);
        for (auto& eventName : observedEventNames())
            videoElement->removeEventListener(eventName, m_videoListener, false);
    }
    if (RefPtr document = m_document.get()) {
        for (auto& eventName : documentObservedEventNames())
            document->removeEventListener(eventName, m_videoListener, false);
    }
}

void VideoPresentationModelVideoElement::setVideoElement(HTMLVideoElement* videoElement)
{
    if (m_videoElement == videoElement)
        return;

    if (RefPtr videoElement = m_videoElement; videoElement && videoElement->videoFullscreenLayer())
        videoElement->setVideoFullscreenLayer(nullptr);

    cleanVideoListeners();

    if (m_videoElement && !videoElement)
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "-> null");

    m_videoElement = videoElement;
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (RefPtr videoElement = m_videoElement) {
        videoElement->addClient(*this);
        m_document = videoElement->document();
        for (auto& eventName : observedEventNames())
            videoElement->addEventListener(eventName, m_videoListener, false);
        m_isListening = true;
        for (auto& eventName : documentObservedEventNames())
            videoElement->document().addEventListener(eventName, m_videoListener, false);
    }

    updateForEventName(eventNameAll());
}

void VideoPresentationModelVideoElement::updateForEventName(const WTF::AtomString& eventName)
{
    if (m_clients.isEmpty())
        return;

    bool all = eventName == eventNameAll();

    RefPtr videoElement = m_videoElement;
    if (all
        || eventName == eventNames().resizeEvent) {
        setHasVideo(videoElement);
        setVideoDimensions(videoElement ? FloatSize(videoElement->videoWidth(), videoElement->videoHeight()) : FloatSize());
    }

    if (all || eventName == eventNames().visibilitychangeEvent)
        documentVisibilityChanged();

#if ENABLE(FULLSCREEN_API)
    if (all || eventName == eventNames().fullscreenchangeEvent)
        documentFullscreenChanged();
#endif

    if (all
        || eventName == eventNames().loadedmetadataEvent || eventName == eventNames().loadstartEvent) {
        setPlayerIdentifier([&]() -> std::optional<MediaPlayerIdentifier> {
            if (eventName == eventNames().loadstartEvent)
                return std::nullopt;

            if (!videoElement)
                return std::nullopt;

            RefPtr player = videoElement->player();
            if (!player)
                return std::nullopt;

            if (auto identifier = player->identifier())
                return identifier;

            return std::nullopt;
        }());
    }

    // FIXME: We should only tag a media element as having been interacting with if those events were trigger by a user gesture.
    if (eventName == eventNames().playEvent || eventName == eventNames().pauseEvent)
        videoInteractedWith();
}

void VideoPresentationModelVideoElement::documentVisibilityChanged()
{
    RefPtr videoElement = m_videoElement;

    if (!videoElement)
        return;

    bool isDocumentVisible = !videoElement->document().hidden();

    if (isDocumentVisible == m_documentIsVisible)
        return;

    m_documentIsVisible = isDocumentVisible;

    for (auto& client : copyToVector(m_clients))
        client->documentVisibilityChanged(m_documentIsVisible);
}

#if ENABLE(FULLSCREEN_API)
void VideoPresentationModelVideoElement::documentFullscreenChanged()
{
    RefPtr videoElement = m_videoElement;

    if (!videoElement)
        return;

    bool isChildOfElementFullscreen = [&] {
        auto* fullscreen = videoElement->document().fullscreenIfExists();
        if (!fullscreen)
            return false;
        RefPtr fullscreenElement = fullscreen->fullscreenElement();
        if (!fullscreenElement)
            return false;
        RefPtr ancestor = videoElement->parentNode();
        while (ancestor && ancestor != fullscreenElement)
            ancestor = ancestor->parentNode();
        return !!ancestor.get();
    }();

    if (std::exchange(m_isChildOfElementFullscreen, isChildOfElementFullscreen) == isChildOfElementFullscreen)
        return;

    videoElement->documentFullscreenChanged(isChildOfElementFullscreen);

    for (auto& client : copyToVector(m_clients))
        client->isChildOfElementFullscreenChanged(m_isChildOfElementFullscreen);
}
#endif

void VideoPresentationModelVideoElement::videoInteractedWith()
{
    RefPtr videoElement = m_videoElement;

    if (!videoElement)
        return;

    RefPtr mediaSession = videoElement->mediaSessionIfExists();
    if (!mediaSession || (!mediaSession->mostRecentUserInteractionTime() && mediaSession->hasBehaviorRestriction(MediaElementSession::RequireUserGestureForAudioRateChange)))
        return;

    for (auto& client : copyToVector(m_clients))
        client->hasBeenInteractedWith();
}

void VideoPresentationModelVideoElement::willExitFullscreen()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr videoElement = m_videoElement)
        videoElement->willExitFullscreen();
}

RetainPtr<PlatformLayer> VideoPresentationModelVideoElement::createVideoFullscreenLayer()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr videoElement = m_videoElement)
        return videoElement->createVideoFullscreenLayer();
    return nullptr;
}

void VideoPresentationModelVideoElement::setVideoFullscreenLayer(PlatformLayer* videoLayer, WTF::Function<void()>&& completionHandler)
{
    if (m_videoFullscreenLayer == videoLayer) {
        completionHandler();
        return;
    }

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_videoFullscreenLayer = videoLayer;
#if PLATFORM(MAC)
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0, 0)];
#else
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0.5, 0.5)];
#endif
    [m_videoFullscreenLayer setFrame:m_videoFrame];

    if (RefPtr videoElement = m_videoElement) {
        videoElement->setVideoFullscreenLayer(m_videoFullscreenLayer.get(), WTFMove(completionHandler));
        return;
    }

    completionHandler();
}

void VideoPresentationModelVideoElement::waitForPreparedForInlineThen(WTF::Function<void()>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (RefPtr videoElement = m_videoElement) {
        videoElement->waitForPreparedForInlineThen(WTFMove(completionHandler));
        return;
    }

    completionHandler();
}

void VideoPresentationModelVideoElement::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    RefPtr videoElement = m_videoElement;
    if (!videoElement)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, mode, ", finishedWithMedia: ", finishedWithMedia);
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, &videoElement->document());

    videoElement->setPresentationMode(HTMLVideoElement::toPresentationMode(mode));

    if (finishedWithMedia && mode == MediaPlayer::VideoFullscreenModeNone) {
        if (videoElement->document().isMediaDocument()) {
            if (RefPtr window = videoElement->document().window())
                window->history().back();
        }
    }
}

void VideoPresentationModelVideoElement::setVideoLayerFrame(FloatRect rect)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, rect.size());
    m_videoFrame = rect;
    [m_videoFullscreenLayer setFrame:CGRect(rect)];
    if (RefPtr videoElement = m_videoElement)
        videoElement->setVideoFullscreenFrame(rect);
}

void VideoPresentationModelVideoElement::setVideoSizeFenced(const FloatSize& size, WTF::MachSendRightAnnotated&& fence)
{
    RefPtr videoElement = m_videoElement;
    if (!videoElement)
        return;

    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, size);
    videoElement->setVideoLayerSizeFenced(size, WTFMove(fence));
    videoElement->setVideoFullscreenFrame({ { }, size });
}

void VideoPresentationModelVideoElement::setVideoFullscreenFrame(FloatRect rect)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, rect.size());
    if (RefPtr videoElement = m_videoElement.get())
        videoElement->setVideoFullscreenFrame(rect);
}

void VideoPresentationModelVideoElement::setVideoLayerGravity(MediaPlayer::VideoGravity gravity)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, gravity);
    if (RefPtr videoElement = m_videoElement)
        videoElement->setVideoFullscreenGravity(gravity);
}

std::span<const AtomString> VideoPresentationModelVideoElement::observedEventNames()
{
    static NeverDestroyed names = std::array { eventNames().resizeEvent, eventNames().loadstartEvent, eventNames().loadedmetadataEvent, eventNames().playEvent, eventNames().pauseEvent };
    return names.get();
}

std::span<const AtomString> VideoPresentationModelVideoElement::documentObservedEventNames()
{
    static NeverDestroyed names = std::array { eventNames().visibilitychangeEvent, eventNames().fullscreenchangeEvent };
    return names.get();
}

const AtomString& VideoPresentationModelVideoElement::eventNameAll()
{
    static MainThreadNeverDestroyed<const AtomString> sEventNameAll = "allEvents"_s;
    return sEventNameAll;
}

void VideoPresentationModelVideoElement::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, videoFullscreenMode);
    if (RefPtr videoElement = m_videoElement) {
        UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, &videoElement->document());
        videoElement->setPresentationMode(HTMLVideoElement::toPresentationMode(videoFullscreenMode));
    }
}

void VideoPresentationModelVideoElement::requestRouteSharingPolicyAndContextUID(CompletionHandler<void(RouteSharingPolicy, String)>&& completionHandler)
{
    completionHandler(AudioSession::singleton().routeSharingPolicy(), AudioSession::singleton().routingContextUID());
}

void VideoPresentationModelVideoElement::addClient(VideoPresentationModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void VideoPresentationModelVideoElement::removeClient(VideoPresentationModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void VideoPresentationModelVideoElement::setHasVideo(bool hasVideo)
{
    if (hasVideo == m_hasVideo)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, hasVideo);
    m_hasVideo = hasVideo;

    for (auto& client : copyToVector(m_clients))
        client->hasVideoChanged(m_hasVideo);
}

void VideoPresentationModelVideoElement::setVideoDimensions(const FloatSize& videoDimensions)
{
    if (m_videoDimensions == videoDimensions)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, videoDimensions, ", clients=", m_clients.size());
    m_videoDimensions = videoDimensions;

    for (auto& client : copyToVector(m_clients))
        client->videoDimensionsChanged(m_videoDimensions);
}

void VideoPresentationModelVideoElement::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    if (m_playerIdentifier == identifier)
        return;

    m_playerIdentifier = identifier;

    for (auto& client : copyToVector(m_clients))
        client->setPlayerIdentifier(identifier);
}

void VideoPresentationModelVideoElement::willEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    for (auto& client : copyToVector(m_clients))
        client->willEnterPictureInPicture();
}

void VideoPresentationModelVideoElement::didEnterPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    for (auto& client : copyToVector(m_clients))
        client->didEnterPictureInPicture();
}

void VideoPresentationModelVideoElement::failedToEnterPictureInPicture()
{
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    for (auto& client : copyToVector(m_clients))
        client->failedToEnterPictureInPicture();
}

void VideoPresentationModelVideoElement::willExitPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    for (auto& client : copyToVector(m_clients))
        client->willExitPictureInPicture();
}

void VideoPresentationModelVideoElement::didExitPictureInPicture()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    for (auto& client : copyToVector(m_clients))
        client->didExitPictureInPicture();
}

void VideoPresentationModelVideoElement::setRequiresTextTrackRepresentation(bool requiresTextTrackRepresentation)
{
    RefPtr videoElement = m_videoElement;
    if (!videoElement)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    videoElement->setRequiresTextTrackRepresentation(requiresTextTrackRepresentation);
}

void VideoPresentationModelVideoElement::setTextTrackRepresentationBounds(const IntRect& bounds)
{
    RefPtr videoElement = m_videoElement;
    if (!videoElement)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, bounds.size());
    videoElement->setTextTrackRepresentataionBounds(bounds);
}

void VideoPresentationModelVideoElement::audioSessionCategoryChanged(AudioSessionCategory category, AudioSessionMode mode, RouteSharingPolicy policy)
{
    for (auto& client : copyToVector(m_clients))
        client->audioSessionCategoryChanged(category, mode, policy);
}

#if !RELEASE_LOG_DISABLED
const Logger* VideoPresentationModelVideoElement::loggerPtr() const
{
    if (RefPtr videoElement = m_videoElement)
        return &videoElement->logger();
    return nullptr;
}

uint64_t VideoPresentationModelVideoElement::logIdentifier() const
{
    if (RefPtr videoElement = m_videoElement)
        return videoElement->logIdentifier();
    return 0;
}

uint64_t VideoPresentationModelVideoElement::nextChildIdentifier() const
{
    return LoggerHelper::childLogIdentifier(logIdentifier(), ++m_childIdentifierSeed);
}

WTFLogChannel& VideoPresentationModelVideoElement::logChannel() const
{
    return LogFullscreen;
}
#endif

} // namespace WebCore

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
