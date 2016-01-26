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
#import "WebVideoFullscreenModelVideoElement.h"

#import "DOMEventInternal.h"
#import "Logging.h"
#import "MediaControlsHost.h"
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
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>


using namespace WebCore;

WebVideoFullscreenModelVideoElement::WebVideoFullscreenModelVideoElement()
    : EventListener(EventListener::CPPEventListenerType)
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

    if (m_videoFullscreenInterface) {
        m_videoFullscreenInterface->resetMediaState();
        if (m_videoElement) {
            m_videoFullscreenInterface->setVideoDimensions(true, m_videoElement->videoWidth(), m_videoElement->videoHeight());
            m_videoFullscreenInterface->setWirelessVideoPlaybackDisabled(m_videoElement->mediaSession().wirelessVideoPlaybackDisabled(*m_videoElement));
        }
    }
}

void WebVideoFullscreenModelVideoElement::setVideoElement(HTMLVideoElement* videoElement)
{
    if (m_videoElement == videoElement)
        return;

    if (m_videoFullscreenInterface)
        m_videoFullscreenInterface->resetMediaState();

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

    if (m_videoFullscreenInterface) {
        m_videoFullscreenInterface->setVideoDimensions(true, videoElement->videoWidth(), videoElement->videoHeight());
        m_videoFullscreenInterface->setWirelessVideoPlaybackDisabled(m_videoElement->mediaSession().wirelessVideoPlaybackDisabled(*m_videoElement));
    }
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
        || eventName == eventNames().durationchangeEvent) {
        m_videoFullscreenInterface->setDuration(m_videoElement->duration());
        // These is no standard event for minFastReverseRateChange; duration change is a reasonable proxy for it.
        // It happens every time a new item becomes ready to play.
        m_videoFullscreenInterface->setCanPlayFastReverse(m_videoElement->minFastReverseRate() < 0.0);
    }

    if (all
        || eventName == eventNames().pauseEvent
        || eventName == eventNames().playEvent
        || eventName == eventNames().ratechangeEvent)
        m_videoFullscreenInterface->setRate(!m_videoElement->paused(), m_videoElement->playbackRate());

    if (all
        || eventName == eventNames().timeupdateEvent) {
        m_videoFullscreenInterface->setCurrentTime(m_videoElement->currentTime(), [[NSProcessInfo processInfo] systemUptime]);
        m_videoFullscreenInterface->setBufferedTime(m_videoElement->maxBufferedTime());
        // FIXME: 130788 - find a better event to update seekable ranges from.
        m_videoFullscreenInterface->setSeekableRanges(*m_videoElement->seekable());
    }

    if (all
        || eventName == eventNames().addtrackEvent
        || eventName == eventNames().removetrackEvent)
        updateLegibleOptions();

    if (all
        || eventName == eventNames().webkitcurrentplaybacktargetiswirelesschangedEvent) {
        bool enabled = m_videoElement->webkitCurrentPlaybackTargetIsWireless();
        WebVideoFullscreenInterface::ExternalPlaybackTargetType targetType = WebVideoFullscreenInterface::TargetTypeNone;
        String localizedDeviceName;

        if (m_videoElement->mediaControlsHost()) {
            static NeverDestroyed<String> airplay(ASCIILiteral("airplay"));
            static NeverDestroyed<String> tvout(ASCIILiteral("tvout"));
            
            String type = m_videoElement->mediaControlsHost()->externalDeviceType();
            if (type == airplay)
                targetType = WebVideoFullscreenInterface::TargetTypeAirPlay;
            else if (type == tvout)
                targetType = WebVideoFullscreenInterface::TargetTypeTVOut;
            localizedDeviceName = m_videoElement->mediaControlsHost()->externalDeviceDisplayName();
        }
        m_videoFullscreenInterface->setExternalPlayback(enabled, targetType, localizedDeviceName);
        m_videoFullscreenInterface->setWirelessVideoPlaybackDisabled(m_videoElement->mediaSession().wirelessVideoPlaybackDisabled(*m_videoElement));
    }
}

void WebVideoFullscreenModelVideoElement::setVideoFullscreenLayer(PlatformLayer* videoLayer)
{
    if (m_videoFullscreenLayer == videoLayer)
        return;
    
    m_videoFullscreenLayer = videoLayer;
    [m_videoFullscreenLayer setAnchorPoint:CGPointMake(0.5, 0.5)];
    [m_videoFullscreenLayer setBounds:m_videoFrame];
    
    if (m_videoElement)
        m_videoElement->setVideoFullscreenLayer(m_videoFullscreenLayer.get());
}

void WebVideoFullscreenModelVideoElement::play()
{
    if (m_videoElement)
        m_videoElement->play();
}

void WebVideoFullscreenModelVideoElement::pause()
{
    if (m_videoElement)
        m_videoElement->pause();
}

void WebVideoFullscreenModelVideoElement::togglePlayState()
{
    if (m_videoElement)
        m_videoElement->togglePlayState();
}

void WebVideoFullscreenModelVideoElement::beginScrubbing()
{
    if (m_videoElement)
        m_videoElement->beginScrubbing();
}

void WebVideoFullscreenModelVideoElement::endScrubbing()
{
    if (m_videoElement)
        m_videoElement->endScrubbing();
}

void WebVideoFullscreenModelVideoElement::seekToTime(double time)
{
    if (m_videoElement)
        m_videoElement->setCurrentTime(time);
}

void WebVideoFullscreenModelVideoElement::fastSeek(double time)
{
    if (m_videoElement)
        m_videoElement->fastSeek(time);
}

void WebVideoFullscreenModelVideoElement::beginScanningForward()
{
    if (m_videoElement)
        m_videoElement->beginScanning(MediaControllerInterface::Forward);
}

void WebVideoFullscreenModelVideoElement::beginScanningBackward()
{
    if (m_videoElement)
        m_videoElement->beginScanning(MediaControllerInterface::Backward);
}

void WebVideoFullscreenModelVideoElement::endScanning()
{
    if (m_videoElement)
        m_videoElement->endScanning();
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
    AudioTrack* selectedAudioTrack = nullptr;

    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        auto& audioTrack = m_audioTracksForMenu[index];
        audioTrack->setEnabled(index == static_cast<size_t>(selectedAudioIndex));
        if (audioTrack->enabled())
            selectedAudioTrack = audioTrack.get();
    }

    m_videoElement->audioTrackEnabledChanged(selectedAudioTrack);
}

void WebVideoFullscreenModelVideoElement::selectLegibleMediaOption(uint64_t index)
{
    TextTrack* textTrack = nullptr;
    
    if (index < m_legibleTracksForMenu.size())
        textTrack = m_legibleTracksForMenu[static_cast<size_t>(index)].get();
    else
        textTrack = TextTrack::captionMenuOffItem();
    
    m_videoElement->setSelectedTextTrack(textTrack);
}

void WebVideoFullscreenModelVideoElement::updateLegibleOptions()
{
    AudioTrackList* audioTrackList = m_videoElement->audioTracks();
    TextTrackList* trackList = m_videoElement->textTracks();

    if ((!trackList && !audioTrackList) || !m_videoElement->document().page() || !m_videoElement->mediaControlsHost())
        return;
    
    WTF::AtomicString displayMode = m_videoElement->mediaControlsHost()->captionDisplayMode();
    TextTrack* offItem = m_videoElement->mediaControlsHost()->captionMenuOffItem();
    TextTrack* automaticItem = m_videoElement->mediaControlsHost()->captionMenuAutomaticItem();
    CaptionUserPreferences& captionPreferences = *m_videoElement->document().page()->group().captionPreferences();
    m_legibleTracksForMenu = captionPreferences.sortedTrackListForMenu(trackList);

    m_audioTracksForMenu = captionPreferences.sortedTrackListForMenu(audioTrackList);
    
    Vector<String> audioTrackDisplayNames;
    uint64_t selectedAudioIndex = 0;
    
    for (size_t index = 0; index < m_audioTracksForMenu.size(); ++index) {
        auto& track = m_audioTracksForMenu[index];
        audioTrackDisplayNames.append(captionPreferences.displayNameForTrack(track.get()));
        
        if (track->enabled())
            selectedAudioIndex = index;
    }
    
    m_videoFullscreenInterface->setAudioMediaSelectionOptions(audioTrackDisplayNames, selectedAudioIndex);

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

const Vector<AtomicString>& WebVideoFullscreenModelVideoElement::observedEventNames()
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
