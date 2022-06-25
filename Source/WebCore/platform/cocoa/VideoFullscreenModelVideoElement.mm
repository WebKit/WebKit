/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "VideoFullscreenModelVideoElement.h"

#if ENABLE(VIDEO_PRESENTATION_MODE)

#import "AddEventListenerOptions.h"
#import "DOMWindow.h"
#import "Event.h"
#import "EventListener.h"
#import "EventNames.h"
#import "HTMLElement.h"
#import "HTMLVideoElement.h"
#import "History.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "Page.h"
#import "PlaybackSessionModelMediaElement.h"
#import "TextTrackList.h"
#import "TimeRanges.h"
#import <QuartzCore/CoreAnimation.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>

namespace WebCore {

VideoFullscreenModelVideoElement::VideoFullscreenModelVideoElement()
    : EventListener(EventListener::CPPEventListenerType)
{
    LOG(Fullscreen, "VideoFullscreenModelVideoElement %p ctor", this);
}

VideoFullscreenModelVideoElement::~VideoFullscreenModelVideoElement()
{
    LOG(Fullscreen, "VideoFullscreenModelVideoElement %p dtor", this);
}

void VideoFullscreenModelVideoElement::setVideoElement(HTMLVideoElement* videoElement)
{
    if (m_videoElement == videoElement)
        return;

    LOG(Fullscreen, "VideoFullscreenModelVideoElement %p setVideoElement(%p)", this, videoElement);

    if (m_videoElement && m_videoElement->videoFullscreenLayer())
        m_videoElement->setVideoFullscreenLayer(nullptr);

    if (m_videoElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            m_videoElement->removeEventListener(eventName, *this, false);
    }
    m_isListening = false;

    m_videoElement = videoElement;

    if (m_videoElement) {
        for (auto& eventName : observedEventNames())
            m_videoElement->addEventListener(eventName, *this, false);
        m_isListening = true;
    }

    updateForEventName(eventNameAll());
}

void VideoFullscreenModelVideoElement::handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event& event)
{
    updateForEventName(event.type());
}

void VideoFullscreenModelVideoElement::updateForEventName(const WTF::AtomString& eventName)
{
    if (m_clients.isEmpty())
        return;
    
    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().resizeEvent) {
        setHasVideo(m_videoElement);
        setVideoDimensions(m_videoElement ? FloatSize(m_videoElement->videoWidth(), m_videoElement->videoHeight()) : FloatSize());
    }

    if (all
        || eventName == eventNames().loadedmetadataEvent || eventName == eventNames().loadstartEvent) {
        setPlayerIdentifier([&]() -> std::optional<MediaPlayerIdentifier> {
            if (eventName == eventNames().loadstartEvent)
                return std::nullopt;

            if (!m_videoElement)
                return std::nullopt;

            auto player = m_videoElement->player();
            if (!player)
                return std::nullopt;

            if (auto identifier = player->identifier())
                return identifier;

            return std::nullopt;
        }());
    }
}

void VideoFullscreenModelVideoElement::willExitFullscreen()
{
    if (m_videoElement)
        m_videoElement->willExitFullscreen();
}

RetainPtr<PlatformLayer> VideoFullscreenModelVideoElement::createVideoFullscreenLayer()
{
    if (m_videoElement)
        return m_videoElement->createVideoFullscreenLayer();
    return nullptr;
}

void VideoFullscreenModelVideoElement::setVideoFullscreenLayer(PlatformLayer* videoLayer, WTF::Function<void()>&& completionHandler)
{
    if (m_videoFullscreenLayer == videoLayer) {
        completionHandler();
        return;
    }
    
    m_videoFullscreenLayer = videoLayer;
#if PLATFORM(MAC)
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0, 0)];
#else
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0.5, 0.5)];
#endif
    [m_videoFullscreenLayer setBounds:m_videoFrame];
    
    if (!m_videoElement) {
        completionHandler();
        return;
    }

    m_videoElement->setVideoFullscreenLayer(m_videoFullscreenLayer.get(), WTFMove(completionHandler));
}

void VideoFullscreenModelVideoElement::waitForPreparedForInlineThen(WTF::Function<void()>&& completionHandler)
{
    if (!m_videoElement) {
        completionHandler();
        return;
    }

    m_videoElement->waitForPreparedForInlineThen(WTFMove(completionHandler));
}

void VideoFullscreenModelVideoElement::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool finishedWithMedia)
{
    if (m_videoElement)
        m_videoElement->setPresentationMode(HTMLVideoElement::toPresentationMode(mode));

    if (m_videoElement && finishedWithMedia && mode == MediaPlayer::VideoFullscreenModeNone) {
        if (m_videoElement->document().isMediaDocument()) {
            if (auto* window = m_videoElement->document().domWindow())
                window->history().back();
        }
    }
}

void VideoFullscreenModelVideoElement::setVideoLayerFrame(FloatRect rect)
{
    m_videoFrame = rect;
    [m_videoFullscreenLayer setBounds:CGRect(rect)];
    if (m_videoElement)
        m_videoElement->setVideoFullscreenFrame(rect);
}

void VideoFullscreenModelVideoElement::setVideoLayerGravity(MediaPlayer::VideoGravity gravity)
{
    if (m_videoElement)
        m_videoElement->setVideoFullscreenGravity(gravity);
}

Span<const AtomString> VideoFullscreenModelVideoElement::observedEventNames()
{
    static NeverDestroyed names = std::array { eventNames().resizeEvent, eventNames().loadstartEvent, eventNames().loadedmetadataEvent };
    return names.get();
}

const AtomString& VideoFullscreenModelVideoElement::eventNameAll()
{
    static MainThreadNeverDestroyed<const AtomString> sEventNameAll = "allEvents"_s;
    return sEventNameAll;
}

void VideoFullscreenModelVideoElement::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    if (m_videoElement) {
        UserGestureIndicator gestureIndicator(ProcessingUserGesture, &m_videoElement->document());
        m_videoElement->setPresentationMode(HTMLVideoElement::toPresentationMode(videoFullscreenMode));
    }
}

void VideoFullscreenModelVideoElement::requestRouteSharingPolicyAndContextUID(CompletionHandler<void(RouteSharingPolicy, String)>&& completionHandler)
{
    auto& session = AudioSession::sharedSession();
    completionHandler(session.routeSharingPolicy(), session.routingContextUID());
}

void VideoFullscreenModelVideoElement::addClient(VideoFullscreenModelClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void VideoFullscreenModelVideoElement::removeClient(VideoFullscreenModelClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void VideoFullscreenModelVideoElement::setHasVideo(bool hasVideo)
{
    if (hasVideo == m_hasVideo)
        return;

    m_hasVideo = hasVideo;

    for (auto& client : copyToVector(m_clients))
        client->hasVideoChanged(m_hasVideo);
}

void VideoFullscreenModelVideoElement::setVideoDimensions(const FloatSize& videoDimensions)
{
    if (m_videoDimensions == videoDimensions)
        return;

    m_videoDimensions = videoDimensions;

    for (auto& client : copyToVector(m_clients))
        client->videoDimensionsChanged(m_videoDimensions);
}

void VideoFullscreenModelVideoElement::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    if (m_playerIdentifier == identifier)
        return;

    m_playerIdentifier = identifier;

    for (auto* client : copyToVector(m_clients))
        client->setPlayerIdentifier(identifier);
}

void VideoFullscreenModelVideoElement::willEnterPictureInPicture()
{
    for (auto& client : copyToVector(m_clients))
        client->willEnterPictureInPicture();
}

void VideoFullscreenModelVideoElement::didEnterPictureInPicture()
{
    for (auto& client : copyToVector(m_clients))
        client->didEnterPictureInPicture();
}

void VideoFullscreenModelVideoElement::failedToEnterPictureInPicture()
{
    for (auto& client : copyToVector(m_clients))
        client->failedToEnterPictureInPicture();
}

void VideoFullscreenModelVideoElement::willExitPictureInPicture()
{
    for (auto& client : copyToVector(m_clients))
        client->willExitPictureInPicture();
}

void VideoFullscreenModelVideoElement::didExitPictureInPicture()
{
    for (auto& client : copyToVector(m_clients))
        client->didExitPictureInPicture();
}

} // namespace WebCore

#endif
