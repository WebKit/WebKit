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

#include "config.h"

#if ENABLE(VIDEO)

#include "MediaControlElements.h"

#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "DOMTokenList.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "MediaControls.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderMedia.h"
#include "RenderSlider.h"
#include "RenderTheme.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;
using namespace std;

// FIXME: These constants may need to be tweaked to better match the seeking in the QuickTime plug-in.
static const float cSkipRepeatDelay = 0.1f;
static const float cSkipTime = 0.2f;
static const float cScanRepeatDelay = 1.5f;
static const float cScanMaximumRate = 8;

HTMLMediaElement* toParentMediaElement(Node* node)
{
    Node* mediaNode = node ? node->shadowAncestorNode() : 0;
    if (!mediaNode || !mediaNode->isElementNode() || !static_cast<Element*>(mediaNode)->isMediaElement())
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}

MediaControlElementType mediaControlElementType(Node* node)
{
    ASSERT(node->isMediaControlElement());
    HTMLElement* element = toHTMLElement(node);
    if (element->hasTagName(inputTag))
        return static_cast<MediaControlInputElement*>(element)->displayType();
    return static_cast<MediaControlElement*>(element)->displayType();
}

// ----------------------------

MediaControlElement::MediaControlElement(Document* document)
    : HTMLDivElement(divTag, document)
    , m_mediaController(0)
{
}

void MediaControlElement::show()
{
    ensureInlineStyleDecl()->removeProperty(CSSPropertyDisplay);
}

void MediaControlElement::hide()
{
    ensureInlineStyleDecl()->setProperty(CSSPropertyDisplay, CSSValueNone);
}

// ----------------------------

inline MediaControlPanelElement::MediaControlPanelElement(Document* document)
    : MediaControlElement(document)
    , m_canBeDragged(false)
    , m_isBeingDragged(false)
    , m_opaque(true)
{
}

PassRefPtr<MediaControlPanelElement> MediaControlPanelElement::create(Document* document)
{
    return adoptRef(new MediaControlPanelElement(document));
}

MediaControlElementType MediaControlPanelElement::displayType() const
{
    return MediaControlsPanel;
}

const AtomicString& MediaControlPanelElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-panel"));
    return id;
}

void MediaControlPanelElement::startDrag(const LayoutPoint& eventLocation)
{
    if (!m_canBeDragged)
        return;

    if (m_isBeingDragged)
        return;

    RenderObject* renderer = this->renderer();
    if (!renderer || !renderer->isBox())
        return;

    Frame* frame = document()->frame();
    if (!frame)
        return;

    m_dragStartPosition = toRenderBox(renderer)->location();
    m_dragStartEventLocation = eventLocation;

    frame->eventHandler()->setCapturingMouseEventsNode(this);

    m_isBeingDragged = true;
}

void MediaControlPanelElement::continueDrag(const LayoutPoint& eventLocation)
{
    if (!m_isBeingDragged)
        return;

    LayoutSize distanceDragged = eventLocation - m_dragStartEventLocation;
    setPosition(m_dragStartPosition + distanceDragged);
}

void MediaControlPanelElement::endDrag()
{
    if (!m_isBeingDragged)
        return;

    m_isBeingDragged = false;

    Frame* frame = document()->frame();
    if (!frame)
        return;

    frame->eventHandler()->setCapturingMouseEventsNode(0);
}

void MediaControlPanelElement::setPosition(const LayoutPoint& position)
{
    CSSElementStyleDeclaration* style = ensureInlineStyleDecl();

    double left = position.x();
    double top = position.y();

    // Set the left and top to control the panel's position; this depends on it being absolute positioned.
    // Set the margin to zero since the position passed in will already include the effect of the margin.
    style->setProperty(CSSPropertyLeft, left, CSSPrimitiveValue::CSS_PX);
    style->setProperty(CSSPropertyTop, top, CSSPrimitiveValue::CSS_PX);
    style->setProperty(CSSPropertyMarginLeft, 0.0, CSSPrimitiveValue::CSS_PX);
    style->setProperty(CSSPropertyMarginTop, 0.0, CSSPrimitiveValue::CSS_PX);

    ExceptionCode ignored;
    classList()->add("dragged", ignored);
}

void MediaControlPanelElement::resetPosition()
{
    CSSElementStyleDeclaration* style = ensureInlineStyleDecl();

    style->removeProperty(CSSPropertyLeft);
    style->removeProperty(CSSPropertyTop);
    style->removeProperty(CSSPropertyMarginLeft);
    style->removeProperty(CSSPropertyMarginTop);

    ExceptionCode ignored;
    classList()->remove("dragged", ignored);
}

void MediaControlPanelElement::makeOpaque()
{
    if (m_opaque)
        return;

    double duration = document()->page() ? document()->page()->theme()->mediaControlsFadeInDuration() : 0;

    CSSElementStyleDeclaration* style = ensureInlineStyleDecl();
    style->setProperty(CSSPropertyWebkitTransitionProperty, CSSPropertyOpacity);
    style->setProperty(CSSPropertyWebkitTransitionDuration, duration, CSSPrimitiveValue::CSS_S);
    style->setProperty(CSSPropertyOpacity, 1.0, CSSPrimitiveValue::CSS_NUMBER);

    m_opaque = true;
}

void MediaControlPanelElement::makeTransparent()
{
    if (!m_opaque)
        return;

    CSSElementStyleDeclaration* style = ensureInlineStyleDecl();
    style->setProperty(CSSPropertyWebkitTransitionProperty, CSSPropertyOpacity);
    style->setProperty(CSSPropertyWebkitTransitionDuration, document()->page()->theme()->mediaControlsFadeOutDuration(), CSSPrimitiveValue::CSS_S);
    style->setProperty(CSSPropertyOpacity, 0.0, CSSPrimitiveValue::CSS_NUMBER);

    m_opaque = false;
}

void MediaControlPanelElement::defaultEventHandler(Event* event)
{
    MediaControlElement::defaultEventHandler(event);

    if (event->isMouseEvent()) {
        LayoutPoint location = static_cast<MouseEvent*>(event)->absoluteLocation();
        if (event->type() == eventNames().mousedownEvent && event->target() == this) {
            startDrag(location);
            event->setDefaultHandled();
        } else if (event->type() == eventNames().mousemoveEvent && m_isBeingDragged)
            continueDrag(location);
        else if (event->type() == eventNames().mouseupEvent && m_isBeingDragged) {
            continueDrag(location);
            endDrag();
            event->setDefaultHandled();
        }
    }
}

void MediaControlPanelElement::setCanBeDragged(bool canBeDragged)
{
    if (m_canBeDragged == canBeDragged)
        return;

    m_canBeDragged = canBeDragged;

    if (!canBeDragged)
        endDrag();
}

// ----------------------------

inline MediaControlTimelineContainerElement::MediaControlTimelineContainerElement(Document* document)
    : MediaControlElement(document)
{
}

PassRefPtr<MediaControlTimelineContainerElement> MediaControlTimelineContainerElement::create(Document* document)
{
    RefPtr<MediaControlTimelineContainerElement> element = adoptRef(new MediaControlTimelineContainerElement(document));
    element->hide();
    return element.release();
}

MediaControlElementType MediaControlTimelineContainerElement::displayType() const
{
    return MediaTimelineContainer;
}

const AtomicString& MediaControlTimelineContainerElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-timeline-container"));
    return id;
}

// ----------------------------

class RenderMediaVolumeSliderContainer : public RenderBlock {
public:
    RenderMediaVolumeSliderContainer(Node*);

private:
    virtual void layout();
};

RenderMediaVolumeSliderContainer::RenderMediaVolumeSliderContainer(Node* node)
    : RenderBlock(node)
{
}

void RenderMediaVolumeSliderContainer::layout()
{
    RenderBlock::layout();
    if (style()->display() == NONE || !previousSibling() || !previousSibling()->isBox())
        return;

    RenderBox* buttonBox = toRenderBox(previousSibling());

    LayoutStateDisabler layoutStateDisabler(view());

    LayoutPoint offset = theme()->volumeSliderOffsetFromMuteButton(buttonBox, size());
    setX(offset.x() + buttonBox->offsetLeft());
    setY(offset.y() + buttonBox->offsetTop());
}

inline MediaControlVolumeSliderContainerElement::MediaControlVolumeSliderContainerElement(Document* document)
    : MediaControlElement(document)
{
}

PassRefPtr<MediaControlVolumeSliderContainerElement> MediaControlVolumeSliderContainerElement::create(Document* document)
{
    RefPtr<MediaControlVolumeSliderContainerElement> element = adoptRef(new MediaControlVolumeSliderContainerElement(document));
    element->hide();
    return element.release();
}

RenderObject* MediaControlVolumeSliderContainerElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderMediaVolumeSliderContainer(this);
}

void MediaControlVolumeSliderContainerElement::defaultEventHandler(Event* event)
{
    if (!event->isMouseEvent() || event->type() != eventNames().mouseoutEvent)
        return;

    // Poor man's mouseleave event detection.
    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    if (!mouseEvent->relatedTarget() || !mouseEvent->relatedTarget()->toNode())
        return;

    if (this->containsIncludingShadowDOM(mouseEvent->relatedTarget()->toNode()))
        return;

    hide();
}


MediaControlElementType MediaControlVolumeSliderContainerElement::displayType() const
{
    return MediaVolumeSliderContainer;
}

const AtomicString& MediaControlVolumeSliderContainerElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-volume-slider-container"));
    return id;
}

// ----------------------------

inline MediaControlStatusDisplayElement::MediaControlStatusDisplayElement(Document* document)
    : MediaControlElement(document)
    , m_stateBeingDisplayed(Nothing)
{
}

PassRefPtr<MediaControlStatusDisplayElement> MediaControlStatusDisplayElement::create(Document* document)
{
    RefPtr<MediaControlStatusDisplayElement> element = adoptRef(new MediaControlStatusDisplayElement(document));
    element->hide();
    return element.release();
}

void MediaControlStatusDisplayElement::update()
{
    // Get the new state that we'll have to display.
    StateBeingDisplayed newStateToDisplay = Nothing;

    if (mediaController()->readyState() <= MediaControllerInterface::HAVE_METADATA && mediaController()->hasCurrentSrc())
        newStateToDisplay = Loading;
    else if (mediaController()->isLiveStream())
        newStateToDisplay = LiveBroadcast;

    if (newStateToDisplay == m_stateBeingDisplayed)
        return;

    ExceptionCode e;

    if (m_stateBeingDisplayed == Nothing)
        show();
    else if (newStateToDisplay == Nothing)
        hide();

    m_stateBeingDisplayed = newStateToDisplay;

    switch (m_stateBeingDisplayed) {
    case Nothing:
        setInnerText("", e);
        break;
    case Loading:
        setInnerText(mediaElementLoadingStateText(), e);
        break;
    case LiveBroadcast:
        setInnerText(mediaElementLiveBroadcastStateText(), e);
        break;
    }
}

MediaControlElementType MediaControlStatusDisplayElement::displayType() const
{
    return MediaStatusDisplay;
}

const AtomicString& MediaControlStatusDisplayElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-status-display"));
    return id;
}

// ----------------------------
    
MediaControlInputElement::MediaControlInputElement(Document* document, MediaControlElementType displayType)
    : HTMLInputElement(inputTag, document, 0, false)
    , m_mediaController(0)
    , m_displayType(displayType)
{
}

void MediaControlInputElement::show()
{
    ensureInlineStyleDecl()->removeProperty(CSSPropertyDisplay);
}

void MediaControlInputElement::hide()
{
    ensureInlineStyleDecl()->setProperty(CSSPropertyDisplay, CSSValueNone);
}


void MediaControlInputElement::setDisplayType(MediaControlElementType displayType)
{
    if (displayType == m_displayType)
        return;

    m_displayType = displayType;
    if (RenderObject* object = renderer())
        object->repaint();
}

// ----------------------------

inline MediaControlMuteButtonElement::MediaControlMuteButtonElement(Document* document, MediaControlElementType displayType)
    : MediaControlInputElement(document, displayType)
{
}

void MediaControlMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        mediaController()->setMuted(!mediaController()->muted());
        event->setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlMuteButtonElement::changedMute()
{
    updateDisplayType();
}

void MediaControlMuteButtonElement::updateDisplayType()
{
    setDisplayType(mediaController()->muted() ? MediaUnMuteButton : MediaMuteButton);
}

// ----------------------------

inline MediaControlPanelMuteButtonElement::MediaControlPanelMuteButtonElement(Document* document, MediaControls* controls)
    : MediaControlMuteButtonElement(document, MediaMuteButton)
    , m_controls(controls)
{
}

PassRefPtr<MediaControlPanelMuteButtonElement> MediaControlPanelMuteButtonElement::create(Document* document, MediaControls* controls)
{
    ASSERT(controls);

    RefPtr<MediaControlPanelMuteButtonElement> button = adoptRef(new MediaControlPanelMuteButtonElement(document, controls));
    button->setType("button");
    return button.release();
}

void MediaControlPanelMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().mouseoverEvent)
        m_controls->showVolumeSlider();

    MediaControlMuteButtonElement::defaultEventHandler(event);
}

const AtomicString& MediaControlPanelMuteButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-mute-button"));
    return id;
}

// ----------------------------

inline MediaControlVolumeSliderMuteButtonElement::MediaControlVolumeSliderMuteButtonElement(Document* document)
    : MediaControlMuteButtonElement(document, MediaMuteButton)
{
}

PassRefPtr<MediaControlVolumeSliderMuteButtonElement> MediaControlVolumeSliderMuteButtonElement::create(Document* document)
{
    RefPtr<MediaControlVolumeSliderMuteButtonElement> button = adoptRef(new MediaControlVolumeSliderMuteButtonElement(document));
    button->setType("button");
    return button.release();
}

const AtomicString& MediaControlVolumeSliderMuteButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-volume-slider-mute-button"));
    return id;
}

// ----------------------------

inline MediaControlPlayButtonElement::MediaControlPlayButtonElement(Document* document)
    : MediaControlInputElement(document, MediaPlayButton)
{
}

PassRefPtr<MediaControlPlayButtonElement> MediaControlPlayButtonElement::create(Document* document)
{
    RefPtr<MediaControlPlayButtonElement> button = adoptRef(new MediaControlPlayButtonElement(document));
    button->setType("button");
    return button.release();
}

void MediaControlPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        if (mediaController()->canPlay())
            mediaController()->play();
        else
            mediaController()->pause();
        updateDisplayType();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlPlayButtonElement::updateDisplayType()
{
    setDisplayType(mediaController()->canPlay() ? MediaPlayButton : MediaPauseButton);
}

const AtomicString& MediaControlPlayButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-play-button"));
    return id;
}

// ----------------------------

inline MediaControlSeekButtonElement::MediaControlSeekButtonElement(Document* document, MediaControlElementType displayType)
    : MediaControlInputElement(document, displayType)
    , m_actionOnStop(Nothing)
    , m_seekType(Skip)
    , m_seekTimer(this, &MediaControlSeekButtonElement::seekTimerFired)
{
}

void MediaControlSeekButtonElement::defaultEventHandler(Event* event)
{
    // Set the mousedown and mouseup events as defaultHandled so they
    // do not trigger drag start or end actions in MediaControlPanelElement.
    if (event->type() == eventNames().mousedownEvent || event->type() == eventNames().mouseupEvent)
        event->setDefaultHandled();
}

void MediaControlSeekButtonElement::setActive(bool flag, bool pause)
{
    if (flag == active())
        return;

    if (flag)
        startTimer();
    else
        stopTimer();

    MediaControlInputElement::setActive(flag, pause);
}

void MediaControlSeekButtonElement::startTimer()
{
    m_seekType = mediaController()->supportsScanning() ? Scan : Skip;

    if (m_seekType == Skip) {
        // Seeking by skipping requires the video to be paused during seeking.
        m_actionOnStop = mediaController()->paused() ? Nothing : Play;
        mediaController()->pause();
    } else {
        // Seeking by scanning requires the video to be playing during seeking.
        m_actionOnStop = mediaController()->paused() ? Pause : Nothing;
        mediaController()->play();
        mediaController()->setPlaybackRate(nextRate());
    }

    m_seekTimer.start(0, m_seekType == Skip ? cSkipRepeatDelay : cScanRepeatDelay);
}

void MediaControlSeekButtonElement::stopTimer()
{
    if (m_seekType == Scan)
        mediaController()->setPlaybackRate(mediaController()->defaultPlaybackRate());

    if (m_actionOnStop == Play)
        mediaController()->play();
    else if (m_actionOnStop == Pause)
        mediaController()->pause();

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
}

float MediaControlSeekButtonElement::nextRate() const
{
    float rate = std::min(cScanMaximumRate, fabsf(mediaController()->playbackRate() * 2));
    if (!isForwardButton())
        rate *= -1;
    return rate;
}

void MediaControlSeekButtonElement::seekTimerFired(Timer<MediaControlSeekButtonElement>*)
{
    if (m_seekType == Skip) {
        ExceptionCode ec;
        float skipTime = isForwardButton() ? cSkipTime : -cSkipTime;
        mediaController()->setCurrentTime(mediaController()->currentTime() + skipTime, ec);
    } else
        mediaController()->setPlaybackRate(nextRate());
}

// ----------------------------

inline MediaControlSeekForwardButtonElement::MediaControlSeekForwardButtonElement(Document* document)
    : MediaControlSeekButtonElement(document, MediaSeekForwardButton)
{
}

PassRefPtr<MediaControlSeekForwardButtonElement> MediaControlSeekForwardButtonElement::create(Document* document)
{
    RefPtr<MediaControlSeekForwardButtonElement> button = adoptRef(new MediaControlSeekForwardButtonElement(document));
    button->setType("button");
    return button.release();
}

const AtomicString& MediaControlSeekForwardButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-seek-forward-button"));
    return id;
}

// ----------------------------

inline MediaControlSeekBackButtonElement::MediaControlSeekBackButtonElement(Document* document)
    : MediaControlSeekButtonElement(document, MediaSeekBackButton)
{
}

PassRefPtr<MediaControlSeekBackButtonElement> MediaControlSeekBackButtonElement::create(Document* document)
{
    RefPtr<MediaControlSeekBackButtonElement> button = adoptRef(new MediaControlSeekBackButtonElement(document));
    button->setType("button");
    return button.release();
}

const AtomicString& MediaControlSeekBackButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-seek-back-button"));
    return id;
}

// ----------------------------

inline MediaControlRewindButtonElement::MediaControlRewindButtonElement(Document* document)
    : MediaControlInputElement(document, MediaRewindButton)
{
}

PassRefPtr<MediaControlRewindButtonElement> MediaControlRewindButtonElement::create(Document* document)
{
    RefPtr<MediaControlRewindButtonElement> button = adoptRef(new MediaControlRewindButtonElement(document));
    button->setType("button");
    return button.release();
}

void MediaControlRewindButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        ExceptionCode ignoredCode;
        mediaController()->setCurrentTime(max(0.0f, mediaController()->currentTime() - 30), ignoredCode);
        event->setDefaultHandled();
    }    
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlRewindButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-rewind-button"));
    return id;
}

// ----------------------------

inline MediaControlReturnToRealtimeButtonElement::MediaControlReturnToRealtimeButtonElement(Document* document)
    : MediaControlInputElement(document, MediaReturnToRealtimeButton)
{
}

PassRefPtr<MediaControlReturnToRealtimeButtonElement> MediaControlReturnToRealtimeButtonElement::create(Document* document)
{
    RefPtr<MediaControlReturnToRealtimeButtonElement> button = adoptRef(new MediaControlReturnToRealtimeButtonElement(document));
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlReturnToRealtimeButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        mediaController()->returnToRealtime();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlReturnToRealtimeButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-return-to-realtime-button"));
    return id;
}

// ----------------------------

inline MediaControlToggleClosedCaptionsButtonElement::MediaControlToggleClosedCaptionsButtonElement(Document* document)
    : MediaControlInputElement(document, MediaShowClosedCaptionsButton)
{
}

PassRefPtr<MediaControlToggleClosedCaptionsButtonElement> MediaControlToggleClosedCaptionsButtonElement::create(Document* document)
{
    RefPtr<MediaControlToggleClosedCaptionsButtonElement> button = adoptRef(new MediaControlToggleClosedCaptionsButtonElement(document));
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlToggleClosedCaptionsButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        mediaController()->setClosedCaptionsVisible(!mediaController()->closedCaptionsVisible());
        setChecked(mediaController()->closedCaptionsVisible());
        updateDisplayType();
        event->setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlToggleClosedCaptionsButtonElement::updateDisplayType()
{
    setDisplayType(mediaController()->closedCaptionsVisible() ? MediaHideClosedCaptionsButton : MediaShowClosedCaptionsButton);
}

const AtomicString& MediaControlToggleClosedCaptionsButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-toggle-closed-captions-button"));
    return id;
}

// ----------------------------

MediaControlTimelineElement::MediaControlTimelineElement(Document* document, MediaControls* controls)
    : MediaControlInputElement(document, MediaSlider)
    , m_controls(controls)
{
}

PassRefPtr<MediaControlTimelineElement> MediaControlTimelineElement::create(Document* document, MediaControls* controls)
{
    ASSERT(controls);

    RefPtr<MediaControlTimelineElement> timeline = adoptRef(new MediaControlTimelineElement(document, controls));
    timeline->setType("range");
    timeline->setAttribute(precisionAttr, "float");
    return timeline.release();
}

void MediaControlTimelineElement::defaultEventHandler(Event* event)
{
    // Left button is 0. Rejects mouse events not from left button.
    if (event->isMouseEvent() && static_cast<MouseEvent*>(event)->button())
        return;

    if (!attached())
        return;

    if (event->type() == eventNames().mousedownEvent)
        mediaController()->beginScrubbing();

    if (event->type() == eventNames().mouseupEvent)
        mediaController()->endScrubbing();

    MediaControlInputElement::defaultEventHandler(event);

    if (event->type() == eventNames().mouseoverEvent || event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mousemoveEvent)
        return;

    float time = narrowPrecisionToFloat(value().toDouble());
    if (time != mediaController()->currentTime()) {
        // FIXME: This is fired 3 times on every click. We should not be doing that <http:/webkit.org/b/58160>.
        ExceptionCode ec;
        mediaController()->setCurrentTime(time, ec);
    }

    RenderSlider* slider = toRenderSlider(renderer());
    if (slider && slider->inDragMode())
        m_controls->updateTimeDisplay();
}

void MediaControlTimelineElement::setPosition(float currentTime) 
{
    setValue(String::number(currentTime));
}

void MediaControlTimelineElement::setDuration(float duration)
{
    setAttribute(maxAttr, String::number(isfinite(duration) ? duration : 0));
}


const AtomicString& MediaControlTimelineElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-timeline"));
    return id;
}

// ----------------------------

inline MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(Document* document)
    : MediaControlInputElement(document, MediaVolumeSlider)
{
}

PassRefPtr<MediaControlVolumeSliderElement> MediaControlVolumeSliderElement::create(Document* document)
{
    RefPtr<MediaControlVolumeSliderElement> slider = adoptRef(new MediaControlVolumeSliderElement(document));
    slider->setType("range");
    slider->setAttribute(precisionAttr, "float");
    slider->setAttribute(maxAttr, "1");
    return slider.release();
}

void MediaControlVolumeSliderElement::defaultEventHandler(Event* event)
{
    // Left button is 0. Rejects mouse events not from left button.
    if (event->isMouseEvent() && static_cast<MouseEvent*>(event)->button())
        return;

    if (!attached())
        return;

    MediaControlInputElement::defaultEventHandler(event);

    if (event->type() == eventNames().mouseoverEvent || event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mousemoveEvent)
        return;

    float volume = narrowPrecisionToFloat(value().toDouble());
    if (volume != mediaController()->volume()) {
        ExceptionCode ec = 0;
        mediaController()->setVolume(volume, ec);
        ASSERT(!ec);
    }
}

void MediaControlVolumeSliderElement::setVolume(float volume)
{
    if (value().toFloat() != volume)
        setValue(String::number(volume));
}

const AtomicString& MediaControlVolumeSliderElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-volume-slider"));
    return id;
}

// ----------------------------

inline MediaControlFullscreenVolumeSliderElement::MediaControlFullscreenVolumeSliderElement(Document* document)
    : MediaControlVolumeSliderElement(document)
{
}

PassRefPtr<MediaControlFullscreenVolumeSliderElement> MediaControlFullscreenVolumeSliderElement::create(Document* document)
{
    RefPtr<MediaControlFullscreenVolumeSliderElement> slider = adoptRef(new MediaControlFullscreenVolumeSliderElement(document));
    slider->setType("range");
    slider->setAttribute(precisionAttr, "float");
    slider->setAttribute(maxAttr, "1");
    return slider.release();
}

const AtomicString& MediaControlFullscreenVolumeSliderElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-fullscreen-volume-slider"));
    return id;
}

// ----------------------------

inline MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(Document* document, MediaControls* controls)
    : MediaControlInputElement(document, MediaFullscreenButton)
    , m_controls(controls)
{
}

PassRefPtr<MediaControlFullscreenButtonElement> MediaControlFullscreenButtonElement::create(Document* document, MediaControls* controls)
{
    ASSERT(controls);

    RefPtr<MediaControlFullscreenButtonElement> button = adoptRef(new MediaControlFullscreenButtonElement(document, controls));
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlFullscreenButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
#if ENABLE(FULLSCREEN_API)
        // Only use the new full screen API if the fullScreenEnabled setting has 
        // been explicitly enabled. Otherwise, use the old fullscreen API. This
        // allows apps which embed a WebView to retain the existing full screen
        // video implementation without requiring them to implement their own full 
        // screen behavior.
        if (document()->settings() && document()->settings()->fullScreenEnabled()) {
            if (document()->webkitIsFullScreen() && document()->webkitCurrentFullScreenElement() == toParentMediaElement(this))
                document()->webkitCancelFullScreen();
            else
                document()->requestFullScreenForElement(toParentMediaElement(this), 0, Document::ExemptIFrameAllowFulScreenRequirement);
        } else
#endif
            mediaController()->enterFullscreen();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlFullscreenButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-fullscreen-button"));
    return id;
}

// ----------------------------

inline MediaControlFullscreenVolumeMinButtonElement::MediaControlFullscreenVolumeMinButtonElement(Document* document)
    : MediaControlInputElement(document, MediaUnMuteButton)
{
}

PassRefPtr<MediaControlFullscreenVolumeMinButtonElement> MediaControlFullscreenVolumeMinButtonElement::create(Document* document)
{
    RefPtr<MediaControlFullscreenVolumeMinButtonElement> button = adoptRef(new MediaControlFullscreenVolumeMinButtonElement(document));
    button->setType("button");
    return button.release();
}

void MediaControlFullscreenVolumeMinButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        ExceptionCode code = 0;
        mediaController()->setVolume(0, code);
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlFullscreenVolumeMinButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-fullscreen-volume-min-button"));
    return id;
}

// ----------------------------

inline MediaControlFullscreenVolumeMaxButtonElement::MediaControlFullscreenVolumeMaxButtonElement(Document* document)
: MediaControlInputElement(document, MediaMuteButton)
{
}

PassRefPtr<MediaControlFullscreenVolumeMaxButtonElement> MediaControlFullscreenVolumeMaxButtonElement::create(Document* document)
{
    RefPtr<MediaControlFullscreenVolumeMaxButtonElement> button = adoptRef(new MediaControlFullscreenVolumeMaxButtonElement(document));
    button->setType("button");
    return button.release();
}

void MediaControlFullscreenVolumeMaxButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        ExceptionCode code = 0;
        mediaController()->setVolume(1, code);
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlFullscreenVolumeMaxButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-fullscreen-volume-max-button"));
    return id;
}

// ----------------------------

class RenderMediaControlTimeDisplay : public RenderDeprecatedFlexibleBox {
public:
    RenderMediaControlTimeDisplay(Node*);

private:
    virtual void layout();
};

RenderMediaControlTimeDisplay::RenderMediaControlTimeDisplay(Node* node)
    : RenderDeprecatedFlexibleBox(node)
{
}

// We want the timeline slider to be at least 100 pixels wide.
// FIXME: Eliminate hard-coded widths altogether.
static const int minWidthToDisplayTimeDisplays = 45 + 100 + 45;

void RenderMediaControlTimeDisplay::layout()
{
    RenderDeprecatedFlexibleBox::layout();
    RenderBox* timelineContainerBox = parentBox();
    while (timelineContainerBox && timelineContainerBox->isAnonymous())
        timelineContainerBox = timelineContainerBox->parentBox();

    if (timelineContainerBox && timelineContainerBox->width() < minWidthToDisplayTimeDisplays)
        setWidth(0);
}

inline MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(Document* document)
    : MediaControlElement(document)
    , m_currentValue(0)
{
}

void MediaControlTimeDisplayElement::setCurrentValue(float time)
{
    m_currentValue = time;
}

RenderObject* MediaControlTimeDisplayElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderMediaControlTimeDisplay(this);
}

// ----------------------------

PassRefPtr<MediaControlTimeRemainingDisplayElement> MediaControlTimeRemainingDisplayElement::create(Document* document)
{
    return adoptRef(new MediaControlTimeRemainingDisplayElement(document));
}

MediaControlTimeRemainingDisplayElement::MediaControlTimeRemainingDisplayElement(Document* document)
    : MediaControlTimeDisplayElement(document)
{
}

MediaControlElementType MediaControlTimeRemainingDisplayElement::displayType() const
{
    return MediaTimeRemainingDisplay;
}

const AtomicString& MediaControlTimeRemainingDisplayElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-time-remaining-display"));
    return id;
}

// ----------------------------

PassRefPtr<MediaControlCurrentTimeDisplayElement> MediaControlCurrentTimeDisplayElement::create(Document* document)
{
    return adoptRef(new MediaControlCurrentTimeDisplayElement(document));
}

MediaControlCurrentTimeDisplayElement::MediaControlCurrentTimeDisplayElement(Document* document)
    : MediaControlTimeDisplayElement(document)
{
}

MediaControlElementType MediaControlCurrentTimeDisplayElement::displayType() const
{
    return MediaCurrentTimeDisplay;
}

const AtomicString& MediaControlCurrentTimeDisplayElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-current-time-display"));
    return id;
}

// ----------------------------

#if ENABLE(VIDEO_TRACK)

class RenderTextTrackContainerElement : public RenderBlock {
public:
    RenderTextTrackContainerElement(Node*);
    
private:
    virtual void layout();
};

RenderTextTrackContainerElement::RenderTextTrackContainerElement(Node* node)
    : RenderBlock(node)
{
}

void RenderTextTrackContainerElement::layout()
{
    RenderBlock::layout();
    if (style()->display() == NONE)
        return;

    ASSERT(mediaControlElementType(node()) == MediaTextTrackDisplayContainer);

    LayoutStateDisabler layoutStateDisabler(view());
    static_cast<MediaControlTextTrackContainerElement*>(node())->updateSizes();
}

inline MediaControlTextTrackContainerElement::MediaControlTextTrackContainerElement(Document* document)
    : MediaControlElement(document)
    , m_fontSize(0)
    , m_bottom(0)
{
}

PassRefPtr<MediaControlTextTrackContainerElement> MediaControlTextTrackContainerElement::create(Document* document)
{
    RefPtr<MediaControlTextTrackContainerElement> element = adoptRef(new MediaControlTextTrackContainerElement(document));
    element->hide();
    return element.release();
}

RenderObject* MediaControlTextTrackContainerElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderTextTrackContainerElement(this);
}

const AtomicString& MediaControlTextTrackContainerElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-text-track-container"));
    return id;
}

static const float mimimumFontSize = 16;
static const float videoHeightFontSizePercentage = .05;
static const float trackBottomMultiplier = .9;
    
void MediaControlTextTrackContainerElement::updateSizes()
{
    HTMLMediaElement* mediaElement = toParentMediaElement(this);
    if (!mediaElement || !mediaElement->renderer() || !mediaElement->renderer()->isVideo())
        return;
    
    IntRect videoBox = toRenderVideo(mediaElement->renderer())->videoBox();
    if (m_videoDisplaySize == videoBox)
        return;
    m_videoDisplaySize = videoBox;

    float fontSize = m_videoDisplaySize.size().height() * videoHeightFontSizePercentage;
    if (fontSize != m_fontSize) {
        m_fontSize = fontSize;
        ensureInlineStyleDecl()->setProperty(CSSPropertyFontSize, String::number(fontSize) + "px");
    }

    LayoutUnit bottom = static_cast<LayoutUnit>(m_videoDisplaySize.y() + m_videoDisplaySize.height() - (m_videoDisplaySize.height() * trackBottomMultiplier));
    if (bottom != m_bottom) {
        m_bottom = bottom;
        ensureInlineStyleDecl()->setProperty(CSSPropertyBottom, String::number(bottom) + "px");
    }
}

// ----------------------------

MediaControlTextTrackDisplayElement::MediaControlTextTrackDisplayElement(Document* document)
    : MediaControlElement(document)
{
}

PassRefPtr<MediaControlTextTrackDisplayElement> MediaControlTextTrackDisplayElement::create(Document* document)
{
    return adoptRef(new MediaControlTextTrackDisplayElement(document));
}

const AtomicString& MediaControlTextTrackDisplayElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-text-track-display"));
    return id;
}

#endif

// ----------------------------

} // namespace WebCore

#endif // ENABLE(VIDEO)
