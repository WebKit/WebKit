/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2012 Igalia S.L.
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

#ifndef MediaControlsGtk_h
#define MediaControlsGtk_h

#if ENABLE(VIDEO)

#include "MediaControls.h"

namespace WebCore {

class MediaControlsGtkEventListener;

class MediaControlsGtk : public MediaControls {
public:
    // Called from port-specific parent create function to create custom controls.
    static PassRefPtr<MediaControlsGtk> createControls(Document&);

    virtual void setMediaController(MediaControllerInterface*) override;
    virtual void reset() override;
    virtual void playbackStarted() override;
    void changedMute() override;
    virtual void updateCurrentTimeDisplay() override;
    virtual void showVolumeSlider() override;
    virtual void makeTransparent() override;
    virtual void toggleClosedCaptionTrackList() override;

    void handleClickEvent(Event*);

#if ENABLE(VIDEO_TRACK)
    void createTextTrackDisplay() override;
#endif

protected:
    explicit MediaControlsGtk(Document&);
    bool initializeControls(Document&);

private:
    void showClosedCaptionTrackList();
    void hideClosedCaptionTrackList();

    PassRefPtr<MediaControlsGtkEventListener> eventListener();

    MediaControlTimeRemainingDisplayElement* m_durationDisplay;
    MediaControlPanelEnclosureElement* m_enclosure;
    MediaControlVolumeSliderContainerElement* m_volumeSliderContainer;
    MediaControlClosedCaptionsTrackListElement* m_closedCaptionsTrackList;
    MediaControlClosedCaptionsContainerElement* m_closedCaptionsContainer;

    RefPtr<MediaControlsGtkEventListener> m_eventListener;
};

}

#endif

#endif
