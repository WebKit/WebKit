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

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#import "VideoFullscreenModelVideoElement.h"

#import "DOMWindow.h"
#import "History.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "PlaybackSessionModelMediaElement.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/Event.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/Page.h>
#import <WebCore/TextTrackList.h>
#import <WebCore/TimeRanges.h>
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

void VideoFullscreenModelVideoElement::updateForEventName(const WTF::AtomicString& eventName)
{
    if (m_clients.isEmpty())
        return;
    
    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().resizeEvent) {
        setHasVideo(m_videoElement);
        setVideoDimensions(m_videoElement ? FloatSize(m_videoElement->videoWidth(), m_videoElement->videoHeight()) : FloatSize());
    }
}

void VideoFullscreenModelVideoElement::willExitFullscreen()
{
    if (m_videoElement)
        m_videoElement->willExitFullscreen();
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
    if (m_videoElement && m_videoElement->fullscreenMode() != mode)
        m_videoElement->setFullscreenMode(mode);

    if (m_videoElement && finishedWithMedia && mode == MediaPlayerEnums::VideoFullscreenModeNone) {
        if (m_videoElement->document().isMediaDocument()) {
            if (DOMWindow* window = m_videoElement->document().domWindow()) {
                if (History* history = window->history())
                    history->back();
            }
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

void VideoFullscreenModelVideoElement::setVideoLayerGravity(MediaPlayerEnums::VideoGravity gravity)
{
    m_videoElement->setVideoFullscreenGravity(gravity);
}

const Vector<AtomicString>& VideoFullscreenModelVideoElement::observedEventNames()
{
    static const auto names = makeNeverDestroyed(Vector<AtomicString> { eventNames().resizeEvent });
    return names;
}

const AtomicString& VideoFullscreenModelVideoElement::eventNameAll()
{
    static NeverDestroyed<AtomicString> sEventNameAll = "allEvents";
    return sEventNameAll;
}

void VideoFullscreenModelVideoElement::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    if (m_videoElement)
        m_videoElement->fullscreenModeChanged(videoFullscreenMode);
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

bool VideoFullscreenModelVideoElement::isVisible() const
{
    if (!m_videoElement)
        return false;

    if (Page* page = m_videoElement->document().page())
        return page->isVisible();

    return false;
}

void VideoFullscreenModelVideoElement::setHasVideo(bool hasVideo)
{
    if (hasVideo == m_hasVideo)
        return;

    m_hasVideo = hasVideo;

    for (auto& client : m_clients)
        client->hasVideoChanged(m_hasVideo);
}

void VideoFullscreenModelVideoElement::setVideoDimensions(const FloatSize& videoDimensions)
{
    if (m_videoDimensions == videoDimensions)
        return;

    m_videoDimensions = videoDimensions;

    for (auto& client : m_clients)
        client->videoDimensionsChanged(m_videoDimensions);
}

void VideoFullscreenModelVideoElement::willEnterPictureInPicture()
{
    for (auto& client : m_clients)
        client->willEnterPictureInPicture();
}

void VideoFullscreenModelVideoElement::didEnterPictureInPicture()
{
    for (auto& client : m_clients)
        client->didEnterPictureInPicture();
}

void VideoFullscreenModelVideoElement::failedToEnterPictureInPicture()
{
    for (auto& client : m_clients)
        client->failedToEnterPictureInPicture();
}

void VideoFullscreenModelVideoElement::willExitPictureInPicture()
{
    for (auto& client : m_clients)
        client->willExitPictureInPicture();
}

void VideoFullscreenModelVideoElement::didExitPictureInPicture()
{
    for (auto& client : m_clients)
        client->didExitPictureInPicture();
}

} // namespace WebCore

#endif
