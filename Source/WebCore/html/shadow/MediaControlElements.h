/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlElements_h
#define MediaControlElements_h

#if ENABLE(VIDEO)

#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "MediaControllerInterface.h"
#include "RenderBlock.h"

// These are the shadow elements used in RenderMedia

namespace WebCore {

class Event;
class Frame;
class HTMLMediaElement;
class MediaControls;

// Must match WebKitSystemInterface.h
enum MediaControlElementType {
    MediaEnterFullscreenButton = 0,
    MediaMuteButton,
    MediaPlayButton,
    MediaSeekBackButton,
    MediaSeekForwardButton,
    MediaSlider,
    MediaSliderThumb,
    MediaRewindButton,
    MediaReturnToRealtimeButton,
    MediaShowClosedCaptionsButton,
    MediaHideClosedCaptionsButton,
    MediaUnMuteButton,
    MediaPauseButton,
    MediaTimelineContainer,
    MediaCurrentTimeDisplay,
    MediaTimeRemainingDisplay,
    MediaStatusDisplay,
    MediaControlsPanel,
    MediaVolumeSliderContainer,
    MediaVolumeSlider,
    MediaVolumeSliderThumb,
    MediaFullScreenVolumeSlider,
    MediaFullScreenVolumeSliderThumb,
    MediaVolumeSliderMuteButton,
    MediaTextTrackDisplayContainer,
    MediaTextTrackDisplay,
    MediaExitFullscreenButton,
};

HTMLMediaElement* toParentMediaElement(Node*);
inline HTMLMediaElement* toParentMediaElement(RenderObject* renderer) { return toParentMediaElement(renderer->node()); }

MediaControlElementType mediaControlElementType(Node*);

// ----------------------------

class MediaControlElement : public HTMLDivElement {
public:
    void hide();
    void show();

    virtual MediaControlElementType displayType() const = 0;

    void setMediaController(MediaControllerInterface* controller) { m_mediaController = controller; }
    MediaControllerInterface* mediaController() const { return m_mediaController; }

protected:
    MediaControlElement(Document*);

private:
    virtual bool isMediaControlElement() const { return true; }

    MediaControllerInterface* m_mediaController;   
};

// ----------------------------

class MediaControlPanelElement : public MediaControlElement {
public:
    static PassRefPtr<MediaControlPanelElement> create(Document*);

    void setCanBeDragged(bool);
    void setIsDisplayed(bool);

    void resetPosition();
    void makeOpaque();
    void makeTransparent();

private:
    MediaControlPanelElement(Document*);
    virtual MediaControlElementType displayType() const;
    virtual const AtomicString& shadowPseudoId() const;
    virtual void defaultEventHandler(Event*);

    void startDrag(const LayoutPoint& eventLocation);
    void continueDrag(const LayoutPoint& eventLocation);
    void endDrag();

    void startTimer();
    void stopTimer();
    void transitionTimerFired(Timer<MediaControlPanelElement>*);

    void setPosition(const LayoutPoint&);

    bool m_canBeDragged;
    bool m_isBeingDragged;
    bool m_isDisplayed;
    bool m_opaque;
    LayoutPoint m_dragStartEventLocation;

    Timer<MediaControlPanelElement> m_transitionTimer;
};

// ----------------------------

class MediaControlTimelineContainerElement : public MediaControlElement {
public:
    static PassRefPtr<MediaControlTimelineContainerElement> create(Document*);

private:
    MediaControlTimelineContainerElement(Document*);
    virtual const AtomicString& shadowPseudoId() const;

    virtual MediaControlElementType displayType() const;
};

// ----------------------------

class MediaControlVolumeSliderContainerElement : public MediaControlElement {
public:
    static PassRefPtr<MediaControlVolumeSliderContainerElement> create(Document*);

private:
    MediaControlVolumeSliderContainerElement(Document*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void defaultEventHandler(Event*);
    virtual MediaControlElementType displayType() const;
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlStatusDisplayElement : public MediaControlElement {
public:
    static PassRefPtr<MediaControlStatusDisplayElement> create(Document*);

    void update();

private:
    MediaControlStatusDisplayElement(Document*);

    virtual MediaControlElementType displayType() const;
    virtual const AtomicString& shadowPseudoId() const;

    enum StateBeingDisplayed { Nothing, Loading, LiveBroadcast };
    StateBeingDisplayed m_stateBeingDisplayed;
};

// ----------------------------

class MediaControlInputElement : public HTMLInputElement {
public:
    void hide();
    void show();

    MediaControlElementType displayType() const { return m_displayType; }

    void setMediaController(MediaControllerInterface* controller) { m_mediaController = controller; }
    MediaControllerInterface* mediaController() const { return m_mediaController; }

protected:
    MediaControlInputElement(Document*, MediaControlElementType);

    void setDisplayType(MediaControlElementType);

private:
    virtual bool isMediaControlElement() const { return true; }

    virtual void updateDisplayType() { }

    MediaControllerInterface* m_mediaController;
    MediaControlElementType m_displayType;
};

// ----------------------------

class MediaControlMuteButtonElement : public MediaControlInputElement {
public:
    void changedMute();

protected:
    MediaControlMuteButtonElement(Document*, MediaControlElementType);
    virtual void defaultEventHandler(Event*);


private:
    virtual void updateDisplayType();
};

// ----------------------------

class MediaControlPanelMuteButtonElement : public MediaControlMuteButtonElement {
public:
    static PassRefPtr<MediaControlPanelMuteButtonElement> create(Document*, MediaControls*);

private:
    MediaControlPanelMuteButtonElement(Document*, MediaControls*);

    virtual void defaultEventHandler(Event*);
    virtual const AtomicString& shadowPseudoId() const;

    MediaControls* m_controls;
};

// ----------------------------

class MediaControlVolumeSliderMuteButtonElement : public MediaControlMuteButtonElement {
public:
    static PassRefPtr<MediaControlVolumeSliderMuteButtonElement> create(Document*);

private:
    MediaControlVolumeSliderMuteButtonElement(Document*);

    virtual const AtomicString& shadowPseudoId() const;
};


// ----------------------------

class MediaControlPlayButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlPlayButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);
    virtual void updateDisplayType();

private:
    MediaControlPlayButtonElement(Document*);

    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlSeekButtonElement : public MediaControlInputElement {
public:
    virtual void defaultEventHandler(Event*);

protected:
    MediaControlSeekButtonElement(Document*, MediaControlElementType);

private:
    virtual bool isForwardButton() const = 0;
    virtual void setActive(bool /*flag*/ = true, bool /*pause*/ = false);

    void startTimer();
    void stopTimer();
    float nextRate() const;
    void seekTimerFired(Timer<MediaControlSeekButtonElement>*);

    enum ActionType { Nothing, Play, Pause };
    ActionType m_actionOnStop;
    enum SeekType { Skip, Scan };
    SeekType m_seekType;
    Timer<MediaControlSeekButtonElement> m_seekTimer;
};

// ----------------------------

class MediaControlSeekForwardButtonElement : public MediaControlSeekButtonElement {
public:
    static PassRefPtr<MediaControlSeekForwardButtonElement> create(Document*);

private:
    MediaControlSeekForwardButtonElement(Document*);

    virtual bool isForwardButton() const { return true; }
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlSeekBackButtonElement : public MediaControlSeekButtonElement {
public:
    static PassRefPtr<MediaControlSeekBackButtonElement> create(Document*);

private:
    MediaControlSeekBackButtonElement(Document*);

    virtual bool isForwardButton() const { return false; }
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlRewindButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlRewindButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);

private:
    MediaControlRewindButtonElement(Document*);

    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlReturnToRealtimeButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlReturnToRealtimeButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);

private:
    MediaControlReturnToRealtimeButtonElement(Document*);

    virtual const AtomicString& shadowPseudoId() const;
};    

// ----------------------------

class MediaControlToggleClosedCaptionsButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlToggleClosedCaptionsButtonElement> create(Document*);

    virtual void defaultEventHandler(Event*);
    virtual void updateDisplayType();

private:
    MediaControlToggleClosedCaptionsButtonElement(Document*);

    virtual const AtomicString& shadowPseudoId() const;
};    

// ----------------------------

class MediaControlTimelineElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlTimelineElement> create(Document*, MediaControls*);

    virtual void defaultEventHandler(Event*);
    void setPosition(float);
    void setDuration(float);

private:
    MediaControlTimelineElement(Document*, MediaControls*);

    virtual const AtomicString& shadowPseudoId() const;

    MediaControls* m_controls;
};

// ----------------------------

class MediaControlVolumeSliderElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlVolumeSliderElement> create(Document*);

    virtual void defaultEventHandler(Event*);
    void setVolume(float);

protected:
    MediaControlVolumeSliderElement(Document*);

private:
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlFullscreenButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlFullscreenButtonElement> create(Document*, MediaControls*);

    virtual void defaultEventHandler(Event*);
    void setIsFullscreen(bool);

private:
    MediaControlFullscreenButtonElement(Document*, MediaControls*);

    virtual const AtomicString& shadowPseudoId() const;

    MediaControls* m_controls;
};

// ----------------------------

class MediaControlFullscreenVolumeSliderElement : public MediaControlVolumeSliderElement {
public:
    static PassRefPtr<MediaControlFullscreenVolumeSliderElement> create(Document*);
    
private:
    MediaControlFullscreenVolumeSliderElement(Document*);
    
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlFullscreenVolumeMinButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlFullscreenVolumeMinButtonElement> create(Document*);
    
    virtual void defaultEventHandler(Event*);
    
private:
    MediaControlFullscreenVolumeMinButtonElement(Document*);
    
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlFullscreenVolumeMaxButtonElement : public MediaControlInputElement {
public:
    static PassRefPtr<MediaControlFullscreenVolumeMaxButtonElement> create(Document*);
    
    virtual void defaultEventHandler(Event*);
    
private:
    MediaControlFullscreenVolumeMaxButtonElement(Document*);
    
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlTimeDisplayElement : public MediaControlElement {
public:
    void setCurrentValue(float);
    float currentValue() const { return m_currentValue; }

protected:
    MediaControlTimeDisplayElement(Document*);

private:
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    float m_currentValue;
};

// ----------------------------

class MediaControlTimeRemainingDisplayElement : public MediaControlTimeDisplayElement {
public:
    static PassRefPtr<MediaControlTimeRemainingDisplayElement> create(Document*);

private:
    MediaControlTimeRemainingDisplayElement(Document*);

    virtual MediaControlElementType displayType() const;
    virtual const AtomicString& shadowPseudoId() const;
};

// ----------------------------

class MediaControlCurrentTimeDisplayElement : public MediaControlTimeDisplayElement {
public:
    static PassRefPtr<MediaControlCurrentTimeDisplayElement> create(Document*);

private:
    MediaControlCurrentTimeDisplayElement(Document*);

    virtual MediaControlElementType displayType() const;
    virtual const AtomicString& shadowPseudoId() const;
};
 
// ----------------------------

#if ENABLE(VIDEO_TRACK)
class MediaControlTextTrackContainerElement : public MediaControlElement {
public:
    static PassRefPtr<MediaControlTextTrackContainerElement> create(Document*);

    void updateDisplay();
    void updateSizes();

private:
    MediaControlTextTrackContainerElement(Document*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual MediaControlElementType displayType() const { return MediaTextTrackDisplayContainer; }
    virtual const AtomicString& shadowPseudoId() const;

    IntRect m_videoDisplaySize;
    float m_fontSize;
};

#endif
// ----------------------------

} // namespace WebCore

#endif // ENABLE(VIDEO)

#endif // MediaControlElements_h
