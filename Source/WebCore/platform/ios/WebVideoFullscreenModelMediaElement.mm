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

#if PLATFORM(IOS)

#import "WebVideoFullscreenModelMediaElement.h"

#import "DOMEventInternal.h"
#import "Logging.h"
#import "WebVideoFullscreenInterface.h"
#import <WebCore/DOMEventListener.h>
#import <WebCore/Event.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

WebVideoFullscreenModelMediaElement::WebVideoFullscreenModelMediaElement()
    : EventListener(EventListener::CPPEventListenerType)
    , m_isListening{false}
{
}

WebVideoFullscreenModelMediaElement::~WebVideoFullscreenModelMediaElement()
{
}

void WebVideoFullscreenModelMediaElement::setMediaElement(HTMLMediaElement* mediaElement)
{
    if (m_mediaElement == mediaElement)
        return;

    if (m_mediaElement && m_isListening) {
        m_mediaElement->removeEventListener(eventNames().durationchangeEvent, this, false);
        m_mediaElement->removeEventListener(eventNames().pauseEvent, this, false);
        m_mediaElement->removeEventListener(eventNames().playEvent, this, false);
        m_mediaElement->removeEventListener(eventNames().ratechangeEvent, this, false);
        m_mediaElement->removeEventListener(eventNames().timeupdateEvent, this, false);
    }
    m_isListening = false;

    m_mediaElement = mediaElement;

    if (!m_mediaElement)
        return;

    m_mediaElement->addEventListener(eventNames().durationchangeEvent, this, false);
    m_mediaElement->addEventListener(eventNames().pauseEvent, this, false);
    m_mediaElement->addEventListener(eventNames().playEvent, this, false);
    m_mediaElement->addEventListener(eventNames().ratechangeEvent, this, false);
    m_mediaElement->addEventListener(eventNames().timeupdateEvent, this, false);
    m_isListening = true;

    m_videoFullscreenInterface->setDuration(m_mediaElement->duration());
    m_videoFullscreenInterface->setRate(m_mediaElement->isPlaying(), m_mediaElement->playbackRate());

    m_videoFullscreenInterface->setCurrentTime(m_mediaElement->currentTime(), [[NSProcessInfo processInfo] systemUptime]);

    if (isHTMLVideoElement(m_mediaElement.get())) {
        HTMLVideoElement *videoElement = toHTMLVideoElement(m_mediaElement.get());
        m_videoFullscreenInterface->setVideoDimensions(true, videoElement->videoWidth(), videoElement->videoHeight());
    }
}

void WebVideoFullscreenModelMediaElement::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event* event)
{
    if (!m_mediaElement || !m_videoFullscreenInterface)
        return;

    LOG(Media, "handleEvent %s", event->type().characters8());
    
    if (event->type() == eventNames().durationchangeEvent)
        m_videoFullscreenInterface->setDuration(m_mediaElement->duration());
    else if (event->type() == eventNames().pauseEvent
        || event->type() == eventNames().playEvent
        || event->type() == eventNames().ratechangeEvent)
        m_videoFullscreenInterface->setRate(m_mediaElement->isPlaying(), m_mediaElement->playbackRate());
    else if (event->type() == eventNames().timeupdateEvent)
        m_videoFullscreenInterface->setCurrentTime(m_mediaElement->currentTime(), [[NSProcessInfo processInfo] systemUptime]);
}

void WebVideoFullscreenModelMediaElement::borrowVideoLayer()
{
    ASSERT(!m_borrowedVideoLayer);
    
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        m_videoFullscreenInterface->willLendVideoLayer(m_mediaElement->platformLayer());
        m_borrowedVideoLayer = m_mediaElement->borrowPlatformLayer();
        if (m_borrowedVideoLayer.get())
            m_videoFullscreenInterface->didLendVideoLayer();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::returnVideoLayer()
{
    ASSERT(m_borrowedVideoLayer.get());
    
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        ASSERT(m_mediaElement);
        m_mediaElement->returnPlatformLayer(m_borrowedVideoLayer.get());
        m_borrowedVideoLayer.clear();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::play() {
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        m_mediaElement->play();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::pause()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        m_mediaElement->pause();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::togglePlayState()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        m_mediaElement->togglePlayState();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::seekToTime(double time)
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        m_mediaElement->setCurrentTime(time);
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::requestExitFullscreen()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        m_mediaElement->exitFullscreen();
        protect.clear();
    });
}

#endif
