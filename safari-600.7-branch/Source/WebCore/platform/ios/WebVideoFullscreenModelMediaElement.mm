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
#import "MediaControlsHost.h"
#import "WebVideoFullscreenInterface.h"
#import <WebCore/DOMEventListener.h>
#import <WebCore/Event.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/Page.h>
#import <WebCore/PageGroup.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextTrackList.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/WebCoreThreadRun.h>
#import <QuartzCore/CoreAnimation.h>
#import <wtf/NeverDestroyed.h>
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
        for (auto eventName : observedEventNames())
            m_mediaElement->removeEventListener(eventName, this, false);
    }
    m_isListening = false;

    m_mediaElement = mediaElement;

    if (!m_mediaElement)
        return;

    for (auto eventName : observedEventNames())
        m_mediaElement->addEventListener(eventName, this, false);
    m_isListening = true;

    updateForEventName(eventNameAll());

    if (isHTMLVideoElement(m_mediaElement.get())) {
        HTMLVideoElement *videoElement = toHTMLVideoElement(m_mediaElement.get());
        m_videoFullscreenInterface->setVideoDimensions(true, videoElement->videoWidth(), videoElement->videoHeight());
    } else
        m_videoFullscreenInterface->setVideoDimensions(false, 0, 0);
}

void WebVideoFullscreenModelMediaElement::handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event* event)
{
    LOG(Media, "handleEvent %s", event->type().characters8());
    updateForEventName(event->type());
}

void WebVideoFullscreenModelMediaElement::updateForEventName(const WTF::AtomicString& eventName)
{
    if (!m_mediaElement || !m_videoFullscreenInterface)
        return;
    
    bool all = eventName == eventNameAll();

    if (all
        || eventName == eventNames().durationchangeEvent) {
        m_videoFullscreenInterface->setDuration(m_mediaElement->duration());
        // These is no standard event for minFastReverseRateChange; duration change is a reasonable proxy for it.
        // It happens every time a new item becomes ready to play.
        m_videoFullscreenInterface->setCanPlayFastReverse(m_mediaElement->minFastReverseRate() < 0.0);
    }

    if (all
        || eventName == eventNames().pauseEvent
        || eventName == eventNames().playEvent
        || eventName == eventNames().ratechangeEvent)
        m_videoFullscreenInterface->setRate(!m_mediaElement->paused(), m_mediaElement->playbackRate());

    if (all
        || eventName == eventNames().timeupdateEvent) {
        m_videoFullscreenInterface->setCurrentTime(m_mediaElement->currentTime(), [[NSProcessInfo processInfo] systemUptime]);
        // FIXME: 130788 - find a better event to update seekable ranges from.
        m_videoFullscreenInterface->setSeekableRanges(*m_mediaElement->seekable());
    }

    if (all
        || eventName == eventNames().addtrackEvent
        || eventName == eventNames().removetrackEvent)
        updateLegibleOptions();

    if (all
        || eventName == eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent) {
        bool enabled = m_mediaElement->mediaSession().currentPlaybackTargetIsWireless(*m_mediaElement);
        WebVideoFullscreenInterface::ExternalPlaybackTargetType targetType = WebVideoFullscreenInterface::TargetTypeNone;
        String localizedDeviceName;

        if (m_mediaElement->mediaControlsHost()) {
            DEPRECATED_DEFINE_STATIC_LOCAL(String, airplay, (ASCIILiteral("airplay")));
            DEPRECATED_DEFINE_STATIC_LOCAL(String, tvout, (ASCIILiteral("tvout")));
            
            String type = m_mediaElement->mediaControlsHost()->externalDeviceType();
            if (type == airplay)
                targetType = WebVideoFullscreenInterface::TargetTypeAirPlay;
            else if (type == tvout)
                targetType = WebVideoFullscreenInterface::TargetTypeTVOut;
            localizedDeviceName = m_mediaElement->mediaControlsHost()->externalDeviceDisplayName();
        }
        m_videoFullscreenInterface->setExternalPlayback(enabled, targetType, localizedDeviceName);
    }
}

void WebVideoFullscreenModelMediaElement::setVideoFullscreenLayer(PlatformLayer* videoLayer)
{
    if (m_videoFullscreenLayer == videoLayer)
        return;
    
    m_videoFullscreenLayer = videoLayer;
    [m_videoFullscreenLayer setFrame:m_videoFrame];
    
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->setVideoFullscreenLayer(m_videoFullscreenLayer.get());
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::play() {
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->play();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::pause()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->pause();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::togglePlayState()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->togglePlayState();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::beginScrubbing()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->beginScrubbing();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::endScrubbing()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->endScrubbing();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::seekToTime(double time)
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->setCurrentTime(time);
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::fastSeek(double time)
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->fastSeek(time);
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::beginScanningForward()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->beginScanning(MediaControllerInterface::Forward);
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::beginScanningBackward()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->beginScanning(MediaControllerInterface::Backward);
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::endScanning()
{
    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement)
            m_mediaElement->endScanning();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::requestExitFullscreen()
{
    if (!m_mediaElement)
        return;

    __block RefPtr<WebVideoFullscreenModelMediaElement> protect(this);
    WebThreadRun(^{
        if (m_mediaElement && m_mediaElement->isFullscreen())
            m_mediaElement->exitFullscreen();
        protect.clear();
    });
}

void WebVideoFullscreenModelMediaElement::setVideoLayerFrame(FloatRect rect)
{
    m_videoFrame = rect;
    [m_videoFullscreenLayer setFrame:CGRect(rect)];
    m_mediaElement->setVideoFullscreenFrame(rect);
}

void WebVideoFullscreenModelMediaElement::setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity gravity)
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
    
    m_mediaElement->setVideoFullscreenGravity(videoGravity);
}

void WebVideoFullscreenModelMediaElement::selectAudioMediaOption(uint64_t)
{
    // FIXME: 131236 Implement audio track selection.
}

void WebVideoFullscreenModelMediaElement::selectLegibleMediaOption(uint64_t index)
{
    TextTrack* textTrack = nullptr;
    
    if (index < m_legibleTracksForMenu.size())
        textTrack = m_legibleTracksForMenu[static_cast<size_t>(index)].get();
    
    m_mediaElement->setSelectedTextTrack(textTrack);
}

void WebVideoFullscreenModelMediaElement::updateLegibleOptions()
{
    TextTrackList* trackList = m_mediaElement->textTracks();
    if (!trackList || !m_mediaElement->document().page() || !m_mediaElement->mediaControlsHost())
        return;
    
    WTF::AtomicString displayMode = m_mediaElement->mediaControlsHost()->captionDisplayMode();
    TextTrack* offItem = m_mediaElement->mediaControlsHost()->captionMenuOffItem();
    TextTrack* automaticItem = m_mediaElement->mediaControlsHost()->captionMenuAutomaticItem();
    CaptionUserPreferences& captionPreferences = *m_mediaElement->document().page()->group().captionPreferences();
    m_legibleTracksForMenu = captionPreferences.sortedTrackListForMenu(trackList);
    Vector<String> trackDisplayNames;
    uint64_t selectedIndex = 0;
    uint64_t offIndex = 0;
    bool trackMenuItemSelected = false;
    
    for (size_t index = 0; index < m_legibleTracksForMenu.size(); index++) {
        auto& track = m_legibleTracksForMenu[index];
        trackDisplayNames.append(captionPreferences.displayNameForTrack(track.get()));
        
        if (track == offItem)
            offIndex = index;
        
        if (track == automaticItem && displayMode == MediaControlsHost::automaticKeyword()) {
            selectedIndex = index;
            trackMenuItemSelected = true;
        }
        
        if (displayMode != MediaControlsHost::automaticKeyword() && track->mode() == TextTrack::showingKeyword()) {
            selectedIndex = index;
            trackMenuItemSelected = true;
        }
    }
    
    if (offIndex && !trackMenuItemSelected && displayMode == MediaControlsHost::forcedOnlyKeyword()) {
        selectedIndex = offIndex;
        trackMenuItemSelected = true;
    }
    
    m_videoFullscreenInterface->setLegibleMediaSelectionOptions(trackDisplayNames, selectedIndex);
}

const Vector<AtomicString>& WebVideoFullscreenModelMediaElement::observedEventNames()
{
    static NeverDestroyed<Vector<AtomicString>> sEventNames;

    if (!sEventNames.get().size()) {
        sEventNames.get().append(eventNames().durationchangeEvent);
        sEventNames.get().append(eventNames().pauseEvent);
        sEventNames.get().append(eventNames().playEvent);
        sEventNames.get().append(eventNames().ratechangeEvent);
        sEventNames.get().append(eventNames().timeupdateEvent);
        sEventNames.get().append(eventNames().addtrackEvent);
        sEventNames.get().append(eventNames().removetrackEvent);
        sEventNames.get().append(eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent);
    }
    return sEventNames.get();
}

const AtomicString& WebVideoFullscreenModelMediaElement::eventNameAll()
{
    static NeverDestroyed<AtomicString> sEventNameAll = "allEvents";
    return sEventNameAll;
}

#endif
