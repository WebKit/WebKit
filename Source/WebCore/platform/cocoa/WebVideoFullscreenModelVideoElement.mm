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
#import "WebVideoFullscreenModelVideoElement.h"

#import "DOMEventInternal.h"
#import "Logging.h"
#import "MediaControlsHost.h"
#import "WebPlaybackSessionModelMediaElement.h"
#import "WebVideoFullscreenInterface.h"
#import <QuartzCore/CoreAnimation.h>
#import <WebCore/DOMEventListener.h>
#import <WebCore/Event.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/Page.h>
#import <WebCore/PageGroup.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextTrackList.h>
#import <WebCore/TimeRanges.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>


using namespace WebCore;

WebVideoFullscreenModelVideoElement::WebVideoFullscreenModelVideoElement(WebPlaybackSessionModelMediaElement& playbackSessionModel)
    : EventListener(EventListener::CPPEventListenerType)
    , m_playbackSessionModel(playbackSessionModel)
{
}

WebVideoFullscreenModelVideoElement::~WebVideoFullscreenModelVideoElement()
{
}

void WebVideoFullscreenModelVideoElement::setWebVideoFullscreenInterface(WebVideoFullscreenInterface* interface)
{
    if (interface == m_videoFullscreenInterface)
        return;

    m_videoFullscreenInterface = interface;
    m_playbackSessionModel->setWebPlaybackSessionInterface(interface);

    if (m_videoFullscreenInterface && m_videoElement)
        m_videoFullscreenInterface->setVideoDimensions(true, m_videoElement->videoWidth(), m_videoElement->videoHeight());
}

void WebVideoFullscreenModelVideoElement::setVideoElement(HTMLVideoElement* videoElement)
{
    m_playbackSessionModel->setMediaElement(videoElement);
    if (m_videoElement == videoElement)
        return;

    if (m_videoElement && m_videoElement->videoFullscreenLayer())
        m_videoElement->setVideoFullscreenLayer(nullptr);

    if (m_videoElement && m_isListening) {
        for (auto& eventName : observedEventNames())
            m_videoElement->removeEventListener(eventName, this, false);
    }
    m_isListening = false;

    m_videoElement = videoElement;

    if (!m_videoElement)
        return;

    for (auto& eventName : observedEventNames())
        m_videoElement->addEventListener(eventName, this, false);
    m_isListening = true;

    updateForEventName(eventNameAll());

    if (m_videoFullscreenInterface)
        m_videoFullscreenInterface->setVideoDimensions(true, videoElement->videoWidth(), videoElement->videoHeight());
}

void WebVideoFullscreenModelVideoElement::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event* event)
{
    updateForEventName(event->type());
}

void WebVideoFullscreenModelVideoElement::updateForEventName(const WTF::AtomicString& eventName)
{
    if (!m_videoElement || !m_videoFullscreenInterface)
        return;
    
    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().resizeEvent)
        m_videoFullscreenInterface->setVideoDimensions(true, m_videoElement->videoWidth(), m_videoElement->videoHeight());
}

void WebVideoFullscreenModelVideoElement::setVideoFullscreenLayer(PlatformLayer* videoLayer)
{
    if (m_videoFullscreenLayer == videoLayer)
        return;
    
    m_videoFullscreenLayer = videoLayer;
#if PLATFORM(MAC)
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0, 0)];
#else
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0.5, 0.5)];
#endif
    [m_videoFullscreenLayer setBounds:m_videoFrame];
    
    if (m_videoElement)
        m_videoElement->setVideoFullscreenLayer(m_videoFullscreenLayer.get());
}

void WebVideoFullscreenModelVideoElement::play()
{
    m_playbackSessionModel->play();
}

void WebVideoFullscreenModelVideoElement::pause()
{
    m_playbackSessionModel->pause();
}

void WebVideoFullscreenModelVideoElement::togglePlayState()
{
    m_playbackSessionModel->togglePlayState();
}

void WebVideoFullscreenModelVideoElement::beginScrubbing()
{
    m_playbackSessionModel->beginScrubbing();
}

void WebVideoFullscreenModelVideoElement::endScrubbing()
{
    m_playbackSessionModel->endScrubbing();
}

void WebVideoFullscreenModelVideoElement::seekToTime(double time)
{
    m_playbackSessionModel->seekToTime(time);
}

void WebVideoFullscreenModelVideoElement::fastSeek(double time)
{
    m_playbackSessionModel->fastSeek(time);
}

void WebVideoFullscreenModelVideoElement::beginScanningForward()
{
    m_playbackSessionModel->beginScanningForward();
}

void WebVideoFullscreenModelVideoElement::beginScanningBackward()
{
    m_playbackSessionModel->beginScanningBackward();
}

void WebVideoFullscreenModelVideoElement::endScanning()
{
    m_playbackSessionModel->endScanning();
}

void WebVideoFullscreenModelVideoElement::requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (m_videoElement && m_videoElement->fullscreenMode() != mode)
        m_videoElement->setFullscreenMode(mode);
}

void WebVideoFullscreenModelVideoElement::setVideoLayerFrame(FloatRect rect)
{
    m_videoFrame = rect;
    [m_videoFullscreenLayer setBounds:CGRect(rect)];
    if (m_videoElement)
        m_videoElement->setVideoFullscreenFrame(rect);
}

void WebVideoFullscreenModelVideoElement::setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity gravity)
{
    MediaPlayer::VideoGravity videoGravity = MediaPlayer::VideoGravityResizeAspect;
    if (gravity == WebVideoFullscreenModel::VideoGravityResize)
        videoGravity = MediaPlayer::VideoGravityResize;
    else if (gravity == WebVideoFullscreenModel::VideoGravityResizeAspect)
        videoGravity = MediaPlayer::VideoGravityResizeAspect;
    else if (gravity == WebVideoFullscreenModel::VideoGravityResizeAspectFill)
        videoGravity = MediaPlayer::VideoGravityResizeAspectFill;
    else
        ASSERT_NOT_REACHED();
    
    m_videoElement->setVideoFullscreenGravity(videoGravity);
}

void WebVideoFullscreenModelVideoElement::selectAudioMediaOption(uint64_t selectedAudioIndex)
{
    m_playbackSessionModel->selectAudioMediaOption(selectedAudioIndex);
}

void WebVideoFullscreenModelVideoElement::selectLegibleMediaOption(uint64_t index)
{
    m_playbackSessionModel->selectLegibleMediaOption(index);
}

const Vector<AtomicString>& WebVideoFullscreenModelVideoElement::observedEventNames()
{
    static NeverDestroyed<Vector<AtomicString>> sEventNames;

    if (!sEventNames.get().size())
        sEventNames.get().append(eventNames().resizeEvent);
    return sEventNames.get();
}

const AtomicString& WebVideoFullscreenModelVideoElement::eventNameAll()
{
    static NeverDestroyed<AtomicString> sEventNameAll = "allEvents";
    return sEventNameAll;
}

void WebVideoFullscreenModelVideoElement::fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode)
{
    if (m_videoElement)
        m_videoElement->fullscreenModeChanged(videoFullscreenMode);
}

bool WebVideoFullscreenModelVideoElement::isVisible() const
{
    if (!m_videoElement)
        return false;

    if (Page* page = m_videoElement->document().page())
        return page->isVisible();

    return false;
}

#endif
