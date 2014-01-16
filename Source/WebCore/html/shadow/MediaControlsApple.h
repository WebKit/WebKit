/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlsApple_h
#define MediaControlsApple_h

#if ENABLE(VIDEO)

#include "MediaControls.h"

namespace WebCore {

class MediaControlsApple;

class MediaControlsAppleEventListener : public EventListener {
public:
    static PassRefPtr<MediaControlsAppleEventListener> create(MediaControlsApple* mediaControls) { return adoptRef(new MediaControlsAppleEventListener(mediaControls)); }
    static const MediaControlsAppleEventListener* cast(const EventListener* listener)
    {
        return listener->type() == MediaControlsAppleEventListenerType
            ? static_cast<const MediaControlsAppleEventListener*>(listener)
            : 0;
    }

    virtual bool operator==(const EventListener& other);

private:
    MediaControlsAppleEventListener(MediaControlsApple* mediaControls)
        : EventListener(MediaControlsAppleEventListenerType)
        , m_mediaControls(mediaControls)
    {
    }

    virtual void handleEvent(ScriptExecutionContext*, Event*);

    MediaControlsApple* m_mediaControls;
};

class MediaControlsApple : public MediaControls {
public:
    static PassRefPtr<MediaControlsApple> createControls(Document&);

    // MediaControls implementation.
    virtual void setMediaController(MediaControllerInterface*) override;

    virtual void hide() override;
    virtual void makeTransparent() override;

    virtual void reset() override;

    virtual void changedMute() override;
    virtual void changedVolume() override;

    virtual void enteredFullscreen() override;
    virtual void exitedFullscreen() override;

    virtual void reportedError() override;
    virtual void loadedMetadata() override;

    virtual void showVolumeSlider() override;
    virtual void updateCurrentTimeDisplay() override;
    virtual void updateStatusDisplay() override;

    virtual void changedClosedCaptionsVisibility() override;
    virtual void toggleClosedCaptionTrackList() override;
    virtual void closedCaptionTracksChanged() override;

    bool shouldClosedCaptionsContainerPreventPageScrolling(int wheelDeltaY);
    void handleClickEvent(Event*);

private:
    MediaControlsApple(Document&);

    virtual void defaultEventHandler(Event*) override;
    PassRefPtr<MediaControlsAppleEventListener> eventListener();

    void showClosedCaptionTrackList();
    void hideClosedCaptionTrackList();
    void setFullscreenSliderVolume();

    MediaControlRewindButtonElement* m_rewindButton;
    MediaControlReturnToRealtimeButtonElement* m_returnToRealTimeButton;
    MediaControlStatusDisplayElement* m_statusDisplay;
    MediaControlTimeRemainingDisplayElement* m_timeRemainingDisplay;
    MediaControlTimelineContainerElement* m_timelineContainer;
    MediaControlSeekBackButtonElement* m_seekBackButton;
    MediaControlSeekForwardButtonElement* m_seekForwardButton;
    MediaControlClosedCaptionsTrackListElement* m_closedCaptionsTrackList;
    MediaControlClosedCaptionsContainerElement* m_closedCaptionsContainer;
    MediaControlVolumeSliderMuteButtonElement* m_volumeSliderMuteButton;
    MediaControlVolumeSliderContainerElement* m_volumeSliderContainer;
    MediaControlFullscreenVolumeMinButtonElement* m_fullScreenMinVolumeButton;
    MediaControlFullscreenVolumeSliderElement* m_fullScreenVolumeSlider;
    MediaControlFullscreenVolumeMaxButtonElement* m_fullScreenMaxVolumeButton;
    RefPtr<MediaControlsAppleEventListener> m_eventListener;
};

}

#endif
#endif
