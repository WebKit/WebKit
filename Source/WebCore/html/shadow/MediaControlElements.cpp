/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "MediaControlElements.h"

#if ENABLE(VIDEO)

#include "DOMTokenList.h"
#include "ElementChildIterator.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FullscreenManager.h"
#include "GraphicsContext.h"
#include "HTMLHeadingElement.h"
#include "HTMLLIElement.h"
#include "HTMLUListElement.h"
#include "HTMLVideoElement.h"
#include "ImageBuffer.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MediaControls.h"
#include "MouseEvent.h"
#include "PODInterval.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderLayer.h"
#include "RenderMediaControlElements.h"
#include "RenderSlider.h"
#include "RenderTheme.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TextTrackCueGeneric.h"
#include "TextTrackList.h"
#include "VTTRegionList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlPanelElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlPanelEnclosureElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlOverlayEnclosureElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlTimelineContainerElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlVolumeSliderContainerElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlStatusDisplayElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlPanelMuteButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlVolumeSliderMuteButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlPlayButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlOverlayPlayButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlSeekForwardButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlSeekBackButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlRewindButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlReturnToRealtimeButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlToggleClosedCaptionsButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlClosedCaptionsContainerElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlClosedCaptionsTrackListElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlTimelineElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlFullscreenButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlPanelVolumeSliderElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlFullscreenVolumeSliderElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlFullscreenVolumeMinButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlFullscreenVolumeMaxButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlTimeRemainingDisplayElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlCurrentTimeDisplayElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(MediaControlTextTrackContainerElement);

using namespace HTMLNames;

static const AtomString& getMediaControlCurrentTimeDisplayElementShadowPseudoId();
static const AtomString& getMediaControlTimeRemainingDisplayElementShadowPseudoId();

MediaControlPanelElement::MediaControlPanelElement(Document& document)
    : MediaControlDivElement(document, MediaControlsPanel)
    , m_canBeDragged(false)
    , m_isBeingDragged(false)
    , m_isDisplayed(false)
    , m_opaque(true)
    , m_transitionTimer(*this, &MediaControlPanelElement::transitionTimerFired)
{
    setPseudo(AtomString("-webkit-media-controls-panel", AtomString::ConstructFromLiteral));
}

Ref<MediaControlPanelElement> MediaControlPanelElement::create(Document& document)
{
    return adoptRef(*new MediaControlPanelElement(document));
}

void MediaControlPanelElement::startDrag(const LayoutPoint& eventLocation)
{
    if (!m_canBeDragged)
        return;

    if (m_isBeingDragged)
        return;

    auto renderer = this->renderer();
    if (!renderer || !renderer->isBox())
        return;

    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return;

    m_lastDragEventLocation = eventLocation;

    frame->eventHandler().setCapturingMouseEventsElement(this);

    m_isBeingDragged = true;
}

void MediaControlPanelElement::continueDrag(const LayoutPoint& eventLocation)
{
    if (!m_isBeingDragged)
        return;

    LayoutSize distanceDragged = eventLocation - m_lastDragEventLocation;
    m_cumulativeDragOffset.move(distanceDragged);
    m_lastDragEventLocation = eventLocation;
    setPosition(m_cumulativeDragOffset);
}

void MediaControlPanelElement::endDrag()
{
    if (!m_isBeingDragged)
        return;

    m_isBeingDragged = false;

    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler().setCapturingMouseEventsElement(nullptr);
}

void MediaControlPanelElement::startTimer()
{
    stopTimer();

    // The timer is required to set the property display:'none' on the panel,
    // such that captions are correctly displayed at the bottom of the video
    // at the end of the fadeout transition.
    Seconds duration = RenderTheme::singleton().mediaControlsFadeOutDuration();
    m_transitionTimer.startOneShot(duration);
}

void MediaControlPanelElement::stopTimer()
{
    if (m_transitionTimer.isActive())
        m_transitionTimer.stop();
}

void MediaControlPanelElement::transitionTimerFired()
{
    if (!m_opaque)
        hide();

    stopTimer();
}

void MediaControlPanelElement::setPosition(const LayoutPoint& position)
{
    double left = position.x();
    double top = position.y();

    // Set the left and top to control the panel's position; this depends on it being absolute positioned.
    // Set the margin to zero since the position passed in will already include the effect of the margin.
    setInlineStyleProperty(CSSPropertyLeft, left, CSSUnitType::CSS_PX);
    setInlineStyleProperty(CSSPropertyTop, top, CSSUnitType::CSS_PX);
    setInlineStyleProperty(CSSPropertyMarginLeft, 0.0, CSSUnitType::CSS_PX);
    setInlineStyleProperty(CSSPropertyMarginTop, 0.0, CSSUnitType::CSS_PX);

    classList().add("dragged");
}

void MediaControlPanelElement::resetPosition()
{
    removeInlineStyleProperty(CSSPropertyLeft);
    removeInlineStyleProperty(CSSPropertyTop);
    removeInlineStyleProperty(CSSPropertyMarginLeft);
    removeInlineStyleProperty(CSSPropertyMarginTop);

    classList().remove("dragged");

    m_cumulativeDragOffset.setX(0);
    m_cumulativeDragOffset.setY(0);
}

void MediaControlPanelElement::makeOpaque()
{
    if (m_opaque)
        return;

    double duration = RenderTheme::singleton().mediaControlsFadeInDuration();

    setInlineStyleProperty(CSSPropertyTransitionProperty, CSSPropertyOpacity);
    setInlineStyleProperty(CSSPropertyTransitionDuration, duration, CSSUnitType::CSS_S);
    setInlineStyleProperty(CSSPropertyOpacity, 1.0, CSSUnitType::CSS_NUMBER);

    m_opaque = true;

    if (m_isDisplayed)
        show();
}

void MediaControlPanelElement::makeTransparent()
{
    if (!m_opaque)
        return;

    Seconds duration = RenderTheme::singleton().mediaControlsFadeOutDuration();

    setInlineStyleProperty(CSSPropertyTransitionProperty, CSSPropertyOpacity);
    setInlineStyleProperty(CSSPropertyTransitionDuration, duration.value(), CSSUnitType::CSS_S);
    setInlineStyleProperty(CSSPropertyOpacity, 0.0, CSSUnitType::CSS_NUMBER);

    m_opaque = false;
    startTimer();
}

void MediaControlPanelElement::defaultEventHandler(Event& event)
{
    MediaControlDivElement::defaultEventHandler(event);

    if (is<MouseEvent>(event)) {
        LayoutPoint location = downcast<MouseEvent>(event).absoluteLocation();
        if (event.type() == eventNames().mousedownEvent && event.target() == this) {
            startDrag(location);
            event.setDefaultHandled();
        } else if (event.type() == eventNames().mousemoveEvent && m_isBeingDragged)
            continueDrag(location);
        else if (event.type() == eventNames().mouseupEvent && m_isBeingDragged) {
            continueDrag(location);
            endDrag();
            event.setDefaultHandled();
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

void MediaControlPanelElement::setIsDisplayed(bool isDisplayed)
{
    m_isDisplayed = isDisplayed;
}

// ----------------------------

MediaControlPanelEnclosureElement::MediaControlPanelEnclosureElement(Document& document)
    // Mapping onto same MediaControlElementType as panel element, since it has similar properties.
    : MediaControlDivElement(document, MediaControlsPanel)
{
    setPseudo(AtomString("-webkit-media-controls-enclosure", AtomString::ConstructFromLiteral));
}

Ref<MediaControlPanelEnclosureElement> MediaControlPanelEnclosureElement::create(Document& document)
{
    return adoptRef(*new MediaControlPanelEnclosureElement(document));
}

// ----------------------------

MediaControlOverlayEnclosureElement::MediaControlOverlayEnclosureElement(Document& document)
    // Mapping onto same MediaControlElementType as panel element, since it has similar properties.
    : MediaControlDivElement(document, MediaControlsPanel)
{
    setPseudo(AtomString("-webkit-media-controls-overlay-enclosure", AtomString::ConstructFromLiteral));
}

Ref<MediaControlOverlayEnclosureElement> MediaControlOverlayEnclosureElement::create(Document& document)
{
    return adoptRef(*new MediaControlOverlayEnclosureElement(document));
}

// ----------------------------

MediaControlTimelineContainerElement::MediaControlTimelineContainerElement(Document& document)
    : MediaControlDivElement(document, MediaTimelineContainer)
{
    setPseudo(AtomString("-webkit-media-controls-timeline-container", AtomString::ConstructFromLiteral));
}

Ref<MediaControlTimelineContainerElement> MediaControlTimelineContainerElement::create(Document& document)
{
    Ref<MediaControlTimelineContainerElement> element = adoptRef(*new MediaControlTimelineContainerElement(document));
    element->hide();
    return element;
}

void MediaControlTimelineContainerElement::setTimeDisplaysHidden(bool hidden)
{
    for (auto& element : childrenOfType<Element>(*this)) {
        if (element.shadowPseudoId() != getMediaControlTimeRemainingDisplayElementShadowPseudoId()
            && element.shadowPseudoId() != getMediaControlCurrentTimeDisplayElementShadowPseudoId())
            continue;

        MediaControlTimeDisplayElement& timeDisplay = static_cast<MediaControlTimeDisplayElement&>(element);
        if (hidden)
            timeDisplay.hide();
        else
            timeDisplay.show();
    }
}

RenderPtr<RenderElement> MediaControlTimelineContainerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMediaControlTimelineContainer>(*this, WTFMove(style));
}

// ----------------------------

MediaControlVolumeSliderContainerElement::MediaControlVolumeSliderContainerElement(Document& document)
    : MediaControlDivElement(document, MediaVolumeSliderContainer)
{
    setPseudo(AtomString("-webkit-media-controls-volume-slider-container", AtomString::ConstructFromLiteral));
}

Ref<MediaControlVolumeSliderContainerElement> MediaControlVolumeSliderContainerElement::create(Document& document)
{
    Ref<MediaControlVolumeSliderContainerElement> element = adoptRef(*new MediaControlVolumeSliderContainerElement(document));
    element->hide();
    return element;
}

RenderPtr<RenderElement> MediaControlVolumeSliderContainerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMediaVolumeSliderContainer>(*this, WTFMove(style));
}

void MediaControlVolumeSliderContainerElement::defaultEventHandler(Event& event)
{
    // Poor man's mouseleave event detection.

    if (!is<MouseEvent>(event) || event.type() != eventNames().mouseoutEvent)
        return;

    if (!is<Node>(downcast<MouseEvent>(event).relatedTarget()))
        return;

    if (containsIncludingShadowDOM(&downcast<Node>(*downcast<MouseEvent>(event).relatedTarget())))
        return;

    hide();
}

// ----------------------------

MediaControlStatusDisplayElement::MediaControlStatusDisplayElement(Document& document)
    : MediaControlDivElement(document, MediaStatusDisplay)
    , m_stateBeingDisplayed(Nothing)
{
    setPseudo(AtomString("-webkit-media-controls-status-display", AtomString::ConstructFromLiteral));
}

Ref<MediaControlStatusDisplayElement> MediaControlStatusDisplayElement::create(Document& document)
{
    Ref<MediaControlStatusDisplayElement> element = adoptRef(*new MediaControlStatusDisplayElement(document));
    element->hide();
    return element;
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

    if (m_stateBeingDisplayed == Nothing)
        show();
    else if (newStateToDisplay == Nothing)
        hide();

    m_stateBeingDisplayed = newStateToDisplay;

    switch (m_stateBeingDisplayed) {
    case Nothing:
        setInnerText(emptyString());
        break;
    case Loading:
        setInnerText(mediaElementLoadingStateText());
        break;
    case LiveBroadcast:
        setInnerText(mediaElementLiveBroadcastStateText());
        break;
    }
}

// ----------------------------

MediaControlPanelMuteButtonElement::MediaControlPanelMuteButtonElement(Document& document, MediaControls* controls)
    : MediaControlMuteButtonElement(document, MediaMuteButton)
    , m_controls(controls)
{
    setPseudo(AtomString("-webkit-media-controls-mute-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlPanelMuteButtonElement> MediaControlPanelMuteButtonElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);

    Ref<MediaControlPanelMuteButtonElement> button = adoptRef(*new MediaControlPanelMuteButtonElement(document, controls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

void MediaControlPanelMuteButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().mouseoverEvent)
        m_controls->showVolumeSlider();

    MediaControlMuteButtonElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlVolumeSliderMuteButtonElement::MediaControlVolumeSliderMuteButtonElement(Document& document)
    : MediaControlMuteButtonElement(document, MediaMuteButton)
{
    setPseudo(AtomString("-webkit-media-controls-volume-slider-mute-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlVolumeSliderMuteButtonElement> MediaControlVolumeSliderMuteButtonElement::create(Document& document)
{
    Ref<MediaControlVolumeSliderMuteButtonElement> button = adoptRef(*new MediaControlVolumeSliderMuteButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

// ----------------------------

MediaControlPlayButtonElement::MediaControlPlayButtonElement(Document& document)
    : MediaControlInputElement(document, MediaPlayButton)
{
    setPseudo(AtomString("-webkit-media-controls-play-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlPlayButtonElement> MediaControlPlayButtonElement::create(Document& document)
{
    Ref<MediaControlPlayButtonElement> button = adoptRef(*new MediaControlPlayButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

void MediaControlPlayButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        if (mediaController()->canPlay())
            mediaController()->play();
        else
            mediaController()->pause();
        updateDisplayType();
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlPlayButtonElement::updateDisplayType()
{
    setDisplayType(mediaController()->canPlay() ? MediaPlayButton : MediaPauseButton);
}

// ----------------------------

MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(Document& document)
    : MediaControlInputElement(document, MediaOverlayPlayButton)
{
    setPseudo(AtomString("-webkit-media-controls-overlay-play-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlOverlayPlayButtonElement> MediaControlOverlayPlayButtonElement::create(Document& document)
{
    Ref<MediaControlOverlayPlayButtonElement> button = adoptRef(*new MediaControlOverlayPlayButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

void MediaControlOverlayPlayButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent && mediaController()->canPlay()) {
        mediaController()->play();
        updateDisplayType();
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlOverlayPlayButtonElement::updateDisplayType()
{
    if (mediaController()->canPlay()) {
        show();
    } else
        hide();
}

// ----------------------------

MediaControlSeekForwardButtonElement::MediaControlSeekForwardButtonElement(Document& document)
    : MediaControlSeekButtonElement(document, MediaSeekForwardButton)
{
    setPseudo(AtomString("-webkit-media-controls-seek-forward-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlSeekForwardButtonElement> MediaControlSeekForwardButtonElement::create(Document& document)
{
    Ref<MediaControlSeekForwardButtonElement> button = adoptRef(*new MediaControlSeekForwardButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

// ----------------------------

MediaControlSeekBackButtonElement::MediaControlSeekBackButtonElement(Document& document)
    : MediaControlSeekButtonElement(document, MediaSeekBackButton)
{
    setPseudo(AtomString("-webkit-media-controls-seek-back-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlSeekBackButtonElement> MediaControlSeekBackButtonElement::create(Document& document)
{
    Ref<MediaControlSeekBackButtonElement> button = adoptRef(*new MediaControlSeekBackButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

// ----------------------------

MediaControlRewindButtonElement::MediaControlRewindButtonElement(Document& document)
    : MediaControlInputElement(document, MediaRewindButton)
{
    setPseudo(AtomString("-webkit-media-controls-rewind-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlRewindButtonElement> MediaControlRewindButtonElement::create(Document& document)
{
    Ref<MediaControlRewindButtonElement> button = adoptRef(*new MediaControlRewindButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

void MediaControlRewindButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        mediaController()->setCurrentTime(std::max<double>(0, mediaController()->currentTime() - 30));
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlReturnToRealtimeButtonElement::MediaControlReturnToRealtimeButtonElement(Document& document)
    : MediaControlInputElement(document, MediaReturnToRealtimeButton)
{
    setPseudo(AtomString("-webkit-media-controls-return-to-realtime-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlReturnToRealtimeButtonElement> MediaControlReturnToRealtimeButtonElement::create(Document& document)
{
    Ref<MediaControlReturnToRealtimeButtonElement> button = adoptRef(*new MediaControlReturnToRealtimeButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button;
}

void MediaControlReturnToRealtimeButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        mediaController()->returnToRealtime();
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlToggleClosedCaptionsButtonElement::MediaControlToggleClosedCaptionsButtonElement(Document& document, MediaControls* controls)
    : MediaControlInputElement(document, MediaShowClosedCaptionsButton)
#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK)
    , m_controls(controls)
#endif
{
#if !PLATFORM(COCOA) && !PLATFORM(WIN) || !PLATFORM(GTK)
    UNUSED_PARAM(controls);
#endif
    setPseudo(AtomString("-webkit-media-controls-toggle-closed-captions-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlToggleClosedCaptionsButtonElement> MediaControlToggleClosedCaptionsButtonElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);

    Ref<MediaControlToggleClosedCaptionsButtonElement> button = adoptRef(*new MediaControlToggleClosedCaptionsButtonElement(document, controls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button;
}

void MediaControlToggleClosedCaptionsButtonElement::updateDisplayType()
{
    bool captionsVisible = mediaController()->closedCaptionsVisible();
    setDisplayType(captionsVisible ? MediaHideClosedCaptionsButton : MediaShowClosedCaptionsButton);
    setChecked(captionsVisible);
}

void MediaControlToggleClosedCaptionsButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        // FIXME: It's not great that the shared code is dictating behavior of platform-specific
        // UI. Not all ports may want the closed captions button to toggle a list of tracks, so
        // we have to use #if.
        // https://bugs.webkit.org/show_bug.cgi?id=101877
#if !PLATFORM(COCOA) && !PLATFORM(WIN) && !PLATFORM(GTK)
        mediaController()->setClosedCaptionsVisible(!mediaController()->closedCaptionsVisible());
        setChecked(mediaController()->closedCaptionsVisible());
        updateDisplayType();
#else
        m_controls->toggleClosedCaptionTrackList();
#endif
        event.setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlClosedCaptionsContainerElement::MediaControlClosedCaptionsContainerElement(Document& document)
    : MediaControlDivElement(document, MediaClosedCaptionsContainer)
{
    setPseudo(AtomString("-webkit-media-controls-closed-captions-container", AtomString::ConstructFromLiteral));
}

Ref<MediaControlClosedCaptionsContainerElement> MediaControlClosedCaptionsContainerElement::create(Document& document)
{
    Ref<MediaControlClosedCaptionsContainerElement> element = adoptRef(*new MediaControlClosedCaptionsContainerElement(document));
    element->setAttributeWithoutSynchronization(dirAttr, AtomString("auto", AtomString::ConstructFromLiteral));
    element->hide();
    return element;
}

// ----------------------------

MediaControlClosedCaptionsTrackListElement::MediaControlClosedCaptionsTrackListElement(Document& document, MediaControls* controls)
    : MediaControlDivElement(document, MediaClosedCaptionsTrackList)
#if ENABLE(VIDEO_TRACK)
    , m_controls(controls)
#endif
{
#if !ENABLE(VIDEO_TRACK)
    UNUSED_PARAM(controls);
#endif
    setPseudo(AtomString("-webkit-media-controls-closed-captions-track-list", AtomString::ConstructFromLiteral));
}

Ref<MediaControlClosedCaptionsTrackListElement> MediaControlClosedCaptionsTrackListElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);
    Ref<MediaControlClosedCaptionsTrackListElement> element = adoptRef(*new MediaControlClosedCaptionsTrackListElement(document, controls));
    return element;
}

void MediaControlClosedCaptionsTrackListElement::defaultEventHandler(Event& event)
{
#if ENABLE(VIDEO_TRACK)
    if (event.type() == eventNames().clickEvent) {
        if (!is<Element>(event.target()))
            return;

        // When we created the elements in the track list, we gave them a custom
        // attribute representing the index in the HTMLMediaElement's list of tracks.
        // Check if the event target has such a custom element and, if so,
        // tell the HTMLMediaElement to enable that track.

        auto textTrack = makeRefPtr(m_menuToTrackMap.get(&downcast<Element>(*event.target())));
        m_menuToTrackMap.clear();
        m_controls->toggleClosedCaptionTrackList();
        if (!textTrack)
            return;

        auto mediaElement = parentMediaElement(this);
        if (!mediaElement)
            return;

        mediaElement->setSelectedTextTrack(textTrack.get());

        updateDisplay();
    }

    MediaControlDivElement::defaultEventHandler(event);
#else
    UNUSED_PARAM(event);
#endif
}

void MediaControlClosedCaptionsTrackListElement::updateDisplay()
{
#if ENABLE(VIDEO_TRACK)
    static NeverDestroyed<AtomString> selectedClassValue("selected", AtomString::ConstructFromLiteral);

    if (!mediaController()->hasClosedCaptions())
        return;

    if (!document().page())
        return;
    CaptionUserPreferences::CaptionDisplayMode displayMode = document().page()->group().captionPreferences().captionDisplayMode();

    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;

    if (!mediaElement->textTracks() || !mediaElement->textTracks()->length())
        return;

    rebuildTrackListMenu();

    RefPtr<Element> offMenuItem;
    bool trackMenuItemSelected = false;

    for (auto& trackItem : m_menuItems) {
        RefPtr<TextTrack> textTrack;
        MenuItemToTrackMap::iterator iter = m_menuToTrackMap.find(trackItem.get());
        if (iter == m_menuToTrackMap.end())
            continue;
        textTrack = iter->value;
        if (!textTrack)
            continue;

        if (textTrack == TextTrack::captionMenuOffItem()) {
            offMenuItem = trackItem;
            continue;
        }

        if (textTrack == TextTrack::captionMenuAutomaticItem()) {
            if (displayMode == CaptionUserPreferences::Automatic)
                trackItem->classList().add(selectedClassValue);
            else
                trackItem->classList().remove(selectedClassValue);
            continue;
        }

        if (displayMode != CaptionUserPreferences::Automatic && textTrack->mode() == TextTrack::Mode::Showing) {
            trackMenuItemSelected = true;
            trackItem->classList().add(selectedClassValue);
        } else
            trackItem->classList().remove(selectedClassValue);
    }

    if (offMenuItem) {
        if (displayMode == CaptionUserPreferences::ForcedOnly && !trackMenuItemSelected)
            offMenuItem->classList().add(selectedClassValue);
        else
            offMenuItem->classList().remove(selectedClassValue);
    }
#endif
}

void MediaControlClosedCaptionsTrackListElement::rebuildTrackListMenu()
{
#if ENABLE(VIDEO_TRACK)
    // Remove any existing content.
    removeChildren();
    m_menuItems.clear();
    m_menuToTrackMap.clear();

    if (!mediaController()->hasClosedCaptions())
        return;

    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;

    auto* trackList = mediaElement->textTracks();
    if (!trackList || !trackList->length())
        return;

    if (!document().page())
        return;
    auto& captionPreferences = document().page()->group().captionPreferences();
    Vector<RefPtr<TextTrack>> tracksForMenu = captionPreferences.sortedTrackListForMenu(trackList);

    auto captionsHeader = HTMLHeadingElement::create(h3Tag, document());
    captionsHeader->appendChild(document().createTextNode(textTrackSubtitlesText()));
    appendChild(captionsHeader);
    auto captionsMenuList = HTMLUListElement::create(document());

    for (auto& textTrack : tracksForMenu) {
        auto menuItem = HTMLLIElement::create(document());
        menuItem->appendChild(document().createTextNode(captionPreferences.displayNameForTrack(textTrack.get())));
        captionsMenuList->appendChild(menuItem);
        m_menuItems.append(menuItem.ptr());
        m_menuToTrackMap.add(menuItem.ptr(), textTrack);
    }

    appendChild(captionsMenuList);
#endif
}

// ----------------------------

MediaControlTimelineElement::MediaControlTimelineElement(Document& document, MediaControls* controls)
    : MediaControlInputElement(document, MediaSlider)
    , m_controls(controls)
{
    setPseudo(AtomString("-webkit-media-controls-timeline", AtomString::ConstructFromLiteral));
}

Ref<MediaControlTimelineElement> MediaControlTimelineElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);

    Ref<MediaControlTimelineElement> timeline = adoptRef(*new MediaControlTimelineElement(document, controls));
    timeline->ensureUserAgentShadowRoot();
    timeline->setType("range");
    timeline->setAttributeWithoutSynchronization(precisionAttr, AtomString("float", AtomString::ConstructFromLiteral));
    return timeline;
}

void MediaControlTimelineElement::defaultEventHandler(Event& event)
{
    // Left button is 0. Rejects mouse events not from left button.
    if (is<MouseEvent>(event) && downcast<MouseEvent>(event).button())
        return;

    if (!renderer())
        return;

    if (event.type() == eventNames().mousedownEvent)
        mediaController()->beginScrubbing();

    if (event.type() == eventNames().mouseupEvent)
        mediaController()->endScrubbing();

    MediaControlInputElement::defaultEventHandler(event);

    if (event.type() == eventNames().mouseoverEvent || event.type() == eventNames().mouseoutEvent || event.type() == eventNames().mousemoveEvent)
        return;

    double time = value().toDouble();
    if ((event.isInputEvent() || event.type() == eventNames().inputEvent) && time != mediaController()->currentTime())
        mediaController()->setCurrentTime(time);

    RenderSlider& slider = downcast<RenderSlider>(*renderer());
    if (slider.inDragMode())
        m_controls->updateCurrentTimeDisplay();
}

#if !PLATFORM(IOS_FAMILY)
bool MediaControlTimelineElement::willRespondToMouseClickEvents()
{
    if (!renderer())
        return false;

    return true;
}
#endif // !PLATFORM(IOS_FAMILY)

void MediaControlTimelineElement::setPosition(double currentTime)
{
    setValue(String::number(currentTime));
}

void MediaControlTimelineElement::setDuration(double duration)
{
    setAttribute(maxAttr, AtomString::number(duration));
}

// ----------------------------

MediaControlPanelVolumeSliderElement::MediaControlPanelVolumeSliderElement(Document& document)
    : MediaControlVolumeSliderElement(document)
{
    setPseudo(AtomString("-webkit-media-controls-volume-slider", AtomString::ConstructFromLiteral));
}

Ref<MediaControlPanelVolumeSliderElement> MediaControlPanelVolumeSliderElement::create(Document& document)
{
    Ref<MediaControlPanelVolumeSliderElement> slider = adoptRef(*new MediaControlPanelVolumeSliderElement(document));
    slider->ensureUserAgentShadowRoot();
    slider->setType("range");
    slider->setAttributeWithoutSynchronization(precisionAttr, AtomString("float", AtomString::ConstructFromLiteral));
    slider->setAttributeWithoutSynchronization(maxAttr, AtomString("1", AtomString::ConstructFromLiteral));
    return slider;
}

// ----------------------------

MediaControlFullscreenVolumeSliderElement::MediaControlFullscreenVolumeSliderElement(Document& document)
    : MediaControlVolumeSliderElement(document)
{
    setPseudo(AtomString("-webkit-media-controls-fullscreen-volume-slider", AtomString::ConstructFromLiteral));
}

Ref<MediaControlFullscreenVolumeSliderElement> MediaControlFullscreenVolumeSliderElement::create(Document& document)
{
    Ref<MediaControlFullscreenVolumeSliderElement> slider = adoptRef(*new MediaControlFullscreenVolumeSliderElement(document));
    slider->ensureUserAgentShadowRoot();
    slider->setType("range");
    slider->setAttributeWithoutSynchronization(precisionAttr, AtomString("float", AtomString::ConstructFromLiteral));
    slider->setAttributeWithoutSynchronization(maxAttr, AtomString("1", AtomString::ConstructFromLiteral));
    return slider;
}

// ----------------------------

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(Document& document)
    : MediaControlInputElement(document, MediaEnterFullscreenButton)
{
    setPseudo(AtomString("-webkit-media-controls-fullscreen-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlFullscreenButtonElement> MediaControlFullscreenButtonElement::create(Document& document)
{
    Ref<MediaControlFullscreenButtonElement> button = adoptRef(*new MediaControlFullscreenButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button;
}

void MediaControlFullscreenButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
#if ENABLE(FULLSCREEN_API)
        // Only use the new full screen API if the fullScreenEnabled setting has
        // been explicitly enabled. Otherwise, use the old fullscreen API. This
        // allows apps which embed a WebView to retain the existing full screen
        // video implementation without requiring them to implement their own full
        // screen behavior.
        if (document().settings().fullScreenEnabled()) {
            if (document().fullscreenManager().isFullscreen() && document().fullscreenManager().currentFullscreenElement() == parentMediaElement(this))
                document().fullscreenManager().cancelFullscreen();
            else
                document().fullscreenManager().requestFullscreenForElement(parentMediaElement(this).get(), FullscreenManager::ExemptIFrameAllowFullscreenRequirement);
        } else
#endif
            mediaController()->enterFullscreen();
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlFullscreenButtonElement::setIsFullscreen(bool isFullscreen)
{
    setDisplayType(isFullscreen ? MediaExitFullscreenButton : MediaEnterFullscreenButton);
}

// ----------------------------

MediaControlFullscreenVolumeMinButtonElement::MediaControlFullscreenVolumeMinButtonElement(Document& document)
    : MediaControlInputElement(document, MediaUnMuteButton)
{
    setPseudo(AtomString("-webkit-media-controls-fullscreen-volume-min-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlFullscreenVolumeMinButtonElement> MediaControlFullscreenVolumeMinButtonElement::create(Document& document)
{
    Ref<MediaControlFullscreenVolumeMinButtonElement> button = adoptRef(*new MediaControlFullscreenVolumeMinButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

void MediaControlFullscreenVolumeMinButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        mediaController()->setVolume(0);
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlFullscreenVolumeMaxButtonElement::MediaControlFullscreenVolumeMaxButtonElement(Document& document)
: MediaControlInputElement(document, MediaMuteButton)
{
    setPseudo(AtomString("-webkit-media-controls-fullscreen-volume-max-button", AtomString::ConstructFromLiteral));
}

Ref<MediaControlFullscreenVolumeMaxButtonElement> MediaControlFullscreenVolumeMaxButtonElement::create(Document& document)
{
    Ref<MediaControlFullscreenVolumeMaxButtonElement> button = adoptRef(*new MediaControlFullscreenVolumeMaxButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button;
}

void MediaControlFullscreenVolumeMaxButtonElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        mediaController()->setVolume(1);
        event.setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

// ----------------------------

MediaControlTimeRemainingDisplayElement::MediaControlTimeRemainingDisplayElement(Document& document)
    : MediaControlTimeDisplayElement(document, MediaTimeRemainingDisplay)
{
    setPseudo(getMediaControlTimeRemainingDisplayElementShadowPseudoId());
}

Ref<MediaControlTimeRemainingDisplayElement> MediaControlTimeRemainingDisplayElement::create(Document& document)
{
    return adoptRef(*new MediaControlTimeRemainingDisplayElement(document));
}

static const AtomString& getMediaControlTimeRemainingDisplayElementShadowPseudoId()
{
    static NeverDestroyed<AtomString> id("-webkit-media-controls-time-remaining-display", AtomString::ConstructFromLiteral);
    return id;
}

// ----------------------------

MediaControlCurrentTimeDisplayElement::MediaControlCurrentTimeDisplayElement(Document& document)
    : MediaControlTimeDisplayElement(document, MediaCurrentTimeDisplay)
{
    setPseudo(getMediaControlCurrentTimeDisplayElementShadowPseudoId());
}

Ref<MediaControlCurrentTimeDisplayElement> MediaControlCurrentTimeDisplayElement::create(Document& document)
{
    return adoptRef(*new MediaControlCurrentTimeDisplayElement(document));
}

static const AtomString& getMediaControlCurrentTimeDisplayElementShadowPseudoId()
{
    static NeverDestroyed<AtomString> id("-webkit-media-controls-current-time-display", AtomString::ConstructFromLiteral);
    return id;
}

// ----------------------------

#if ENABLE(VIDEO_TRACK)

MediaControlTextTrackContainerElement::MediaControlTextTrackContainerElement(Document& document)
    : MediaControlDivElement(document, MediaTextTrackDisplayContainer)
    , m_updateTimer(*this, &MediaControlTextTrackContainerElement::updateTimerFired)
    , m_fontSize(0)
    , m_fontSizeIsImportant(false)
    , m_updateTextTrackRepresentationStyle(false)
{
    setPseudo(AtomString("-webkit-media-text-track-container", AtomString::ConstructFromLiteral));
}

Ref<MediaControlTextTrackContainerElement> MediaControlTextTrackContainerElement::create(Document& document)
{
    auto element = adoptRef(*new MediaControlTextTrackContainerElement(document));
    element->hide();
    return element;
}

RenderPtr<RenderElement> MediaControlTextTrackContainerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextTrackContainerElement>(*this, WTFMove(style));
}

static bool compareCueIntervalForDisplay(const CueInterval& one, const CueInterval& two)
{
    return one.data()->isPositionedAbove(two.data());
};

void MediaControlTextTrackContainerElement::updateDisplay()
{
    if (!mediaController()->closedCaptionsVisible())
        removeChildren();

    auto mediaElement = parentMediaElement(this);
    // 1. If the media element is an audio element, or is another playback
    // mechanism with no rendering area, abort these steps. There is nothing to
    // render.
    if (!mediaElement || !mediaElement->isVideo() || m_videoDisplaySize.size().isEmpty())
        return;

    // 2. Let video be the media element or other playback mechanism.
    HTMLVideoElement& video = downcast<HTMLVideoElement>(*mediaElement);

    // 3. Let output be an empty list of absolutely positioned CSS block boxes.

    // 4. If the user agent is exposing a user interface for video, add to
    // output one or more completely transparent positioned CSS block boxes that
    // cover the same region as the user interface.

    // 5. If the last time these rules were run, the user agent was not exposing
    // a user interface for video, but now it is, let reset be true. Otherwise,
    // let reset be false.

    // There is nothing to be done explicitly for 4th and 5th steps, as
    // everything is handled through CSS. The caption box is on top of the
    // controls box, in a container set with the -webkit-box display property.

    // 6. Let tracks be the subset of video's list of text tracks that have as
    // their rules for updating the text track rendering these rules for
    // updating the display of WebVTT text tracks, and whose text track mode is
    // showing or showing by default.
    // 7. Let cues be an empty list of text track cues.
    // 8. For each track track in tracks, append to cues all the cues from
    // track's list of cues that have their text track cue active flag set.
    CueList activeCues = video.currentlyActiveCues();

    // 9. If reset is false, then, for each text track cue cue in cues: if cue's
    // text track cue display state has a set of CSS boxes, then add those boxes
    // to output, and remove cue from cues.

    // There is nothing explicitly to be done here, as all the caching occurs
    // within the TextTrackCue instance itself. If parameters of the cue change,
    // the display tree is cleared.

    // If the number of CSS boxes in the output is less than the number of cues
    // we wish to render (e.g., we are adding another cue in a set of roll-up
    // cues), remove all the existing CSS boxes representing the cues and re-add
    // them so that the new cue is at the bottom.
    // FIXME: Calling countChildNodes() here is inefficient. We don't need to
    // traverse all children just to check if there are less children than cues.
    if (countChildNodes() < activeCues.size())
        removeChildren();

    activeCues.removeAllMatching([] (CueInterval& cueInterval) {
        RefPtr<TextTrackCue> cue = cueInterval.data();
        return !cue->track()
            || !cue->track()->isRendered()
            || cue->track()->mode() == TextTrack::Mode::Disabled
            || !cue->isActive()
            || !cue->isRenderable();
    });

    // Sort the active cues for the appropriate display order. For example, for roll-up
    // or paint-on captions, we need to add the cues in reverse chronological order,
    // so that the newest captions appear at the bottom.
    std::sort(activeCues.begin(), activeCues.end(), &compareCueIntervalForDisplay);

    if (mediaController()->closedCaptionsVisible()) {
        // 10. For each text track cue cue in cues that has not yet had
        // corresponding CSS boxes added to output, in text track cue order, run the
        // following substeps:
        for (auto& interval : activeCues) {
            auto cue = interval.data();
            cue->setFontSize(m_fontSize, m_videoDisplaySize.size(), m_fontSizeIsImportant);
            if (is<VTTCue>(cue) || is<TextTrackCueGeneric>(cue))
                processActiveVTTCue(*toVTTCue(cue));
            else {
                auto displayBox = cue->getDisplayTree(m_videoDisplaySize.size(), m_fontSize);
                if (displayBox->hasChildNodes() && !contains(displayBox.get()))
                    appendChild(*displayBox);
            }
        }
    }

    // 11. Return output.
    if (hasChildNodes()) {
        show();
        updateTextTrackRepresentation();
    } else {
        hide();
        clearTextTrackRepresentation();
    }
}

void MediaControlTextTrackContainerElement::processActiveVTTCue(VTTCue& cue)
{
    ASSERT(is<VTTCue>(cue) || is<TextTrackCueGeneric>(cue));

    DEBUG_LOG(LOGIDENTIFIER, "adding and positioning cue: \"", cue.text(), "\", start=", cue.startTime(), ", end=", cue.endTime(), ", line=", cue.line());
    Ref<TextTrackCueBox> displayBox = *cue.getDisplayTree(m_videoDisplaySize.size(), m_fontSize);

    if (auto region = cue.track()->regions()->getRegionById(cue.regionId())) {
        // Let region be the WebVTT region whose region identifier
        // matches the text track cue region identifier of cue.
        Ref<HTMLDivElement> regionNode = region->getDisplayTree();

        if (!contains(regionNode.ptr()))
            appendChild(region->getDisplayTree());

        region->appendTextTrackCueBox(WTFMove(displayBox));
    } else {
        // If cue has an empty text track cue region identifier or there is no
        // WebVTT region whose region identifier is identical to cue's text
        // track cue region identifier, run the following substeps:
        if (displayBox->hasChildNodes() && !contains(displayBox.ptr())) {
            // Note: the display tree of a cue is removed when the active flag of the cue is unset.
            appendChild(displayBox);
        }
    }
}

void MediaControlTextTrackContainerElement::updateActiveCuesFontSize()
{
    if (!document().page())
        return;

    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;

    float smallestDimension = std::min(m_videoDisplaySize.size().height(), m_videoDisplaySize.size().width());
    float fontScale = document().page()->group().captionPreferences().captionFontSizeScaleAndImportance(m_fontSizeIsImportant);
    m_fontSize = lroundf(smallestDimension * fontScale);

    for (auto& activeCue : mediaElement->currentlyActiveCues()) {
        RefPtr<TextTrackCue> cue = activeCue.data();
        if (cue->isRenderable())
            cue->setFontSize(m_fontSize, m_videoDisplaySize.size(), m_fontSizeIsImportant);
    }
}

void MediaControlTextTrackContainerElement::updateTextStrokeStyle()
{
    if (!document().page())
        return;

    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;
    
    String language;

    // FIXME: Since it is possible to have more than one text track enabled, the following code may not find the correct language.
    // The default UI only allows a user to enable one track at a time, so it should be OK for now, but we should consider doing
    // this differently, see <https://bugs.webkit.org/show_bug.cgi?id=169875>.
    if (auto* tracks = mediaElement->textTracks()) {
        for (unsigned i = 0; i < tracks->length(); ++i) {
            auto track = tracks->item(i);
            if (track && track->mode() == TextTrack::Mode::Showing) {
                language = track->validBCP47Language();
                break;
            }
        }
    }

    float strokeWidth;
    bool important;

    // FIXME: find a way to set this property in the stylesheet like the other user style preferences, see <https://bugs.webkit.org/show_bug.cgi?id=169874>.
    if (document().page()->group().captionPreferences().captionStrokeWidthForFont(m_fontSize, language, strokeWidth, important))
        setInlineStyleProperty(CSSPropertyStrokeWidth, strokeWidth, CSSUnitType::CSS_PX, important);
}

void MediaControlTextTrackContainerElement::updateTimerFired()
{
    if (!document().page())
        return;

    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;

    for (auto& activeCue : mediaElement->currentlyActiveCues())
        activeCue.data()->recalculateStyles();

    if (m_textTrackRepresentation)
        updateStyleForTextTrackRepresentation();

    updateActiveCuesFontSize();
    updateDisplay();
    updateTextStrokeStyle();
}

void MediaControlTextTrackContainerElement::updateTextTrackRepresentation()
{
    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;

    if (!mediaElement->requiresTextTrackRepresentation()) {
        if (m_textTrackRepresentation) {
            clearTextTrackRepresentation();
            updateSizes(ForceUpdate::Yes);
        }
        return;
    }

    if (!m_textTrackRepresentation) {
        m_textTrackRepresentation = TextTrackRepresentation::create(*this);
        if (document().page())
            m_textTrackRepresentation->setContentScale(document().page()->deviceScaleFactor());
        m_updateTextTrackRepresentationStyle = true;
        mediaElement->setTextTrackRepresentation(m_textTrackRepresentation.get());
    }

    m_textTrackRepresentation->update();
    updateStyleForTextTrackRepresentation();
}

void MediaControlTextTrackContainerElement::clearTextTrackRepresentation()
{
    if (!m_textTrackRepresentation)
        return;

    m_textTrackRepresentation = nullptr;
    m_updateTextTrackRepresentationStyle = true;
    if (auto mediaElement = parentMediaElement(this))
        mediaElement->setTextTrackRepresentation(nullptr);
    updateStyleForTextTrackRepresentation();
    updateActiveCuesFontSize();
}

void MediaControlTextTrackContainerElement::updateStyleForTextTrackRepresentation()
{
    if (!m_updateTextTrackRepresentationStyle)
        return;

    m_updateTextTrackRepresentationStyle = false;

    if (m_textTrackRepresentation) {
        setInlineStyleProperty(CSSPropertyWidth, m_videoDisplaySize.size().width(), CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyHeight, m_videoDisplaySize.size().height(), CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
        setInlineStyleProperty(CSSPropertyLeft, 0, CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyTop, 0, CSSUnitType::CSS_PX);
        return;
    }

    removeInlineStyleProperty(CSSPropertyPosition);
    removeInlineStyleProperty(CSSPropertyWidth);
    removeInlineStyleProperty(CSSPropertyHeight);
    removeInlineStyleProperty(CSSPropertyLeft);
    removeInlineStyleProperty(CSSPropertyTop);
}

void MediaControlTextTrackContainerElement::enteredFullscreen()
{
    if (hasChildNodes())
        updateTextTrackRepresentation();
    updateSizes(ForceUpdate::Yes);
}

void MediaControlTextTrackContainerElement::exitedFullscreen()
{
    clearTextTrackRepresentation();
    updateSizes(ForceUpdate::Yes);
}

void MediaControlTextTrackContainerElement::updateSizes(ForceUpdate force)
{
    auto mediaElement = parentMediaElement(this);
    if (!mediaElement)
        return;

    if (!document().page())
        return;

    IntRect videoBox;
    if (m_textTrackRepresentation) {
        videoBox = m_textTrackRepresentation->bounds();
        float deviceScaleFactor = document().page()->deviceScaleFactor();
        videoBox.setWidth(videoBox.width() * deviceScaleFactor);
        videoBox.setHeight(videoBox.height() * deviceScaleFactor);
    } else {
        if (!is<RenderVideo>(mediaElement->renderer()))
            return;
        videoBox = downcast<RenderVideo>(*mediaElement->renderer()).videoBox();
    }

    if (force == ForceUpdate::No && m_videoDisplaySize == videoBox)
        return;

    m_videoDisplaySize = videoBox;
    m_updateTextTrackRepresentationStyle = true;
    mediaElement->syncTextTrackBounds();

    // FIXME (121170): This function is called during layout, and should lay out the text tracks immediately.
    m_updateTimer.startOneShot(0_s);
}

RefPtr<Image> MediaControlTextTrackContainerElement::createTextTrackRepresentationImage()
{
    if (!hasChildNodes())
        return nullptr;

    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return nullptr;

    document().updateLayout();

    auto* renderer = this->renderer();
    if (!renderer)
        return nullptr;

    if (!renderer->hasLayer())
        return nullptr;

    RenderLayer* layer = downcast<RenderLayerModelObject>(*renderer).layer();

    float deviceScaleFactor = 1;
    if (Page* page = document().page())
        deviceScaleFactor = page->deviceScaleFactor();

    IntRect paintingRect = IntRect(IntPoint(), layer->size());

    // FIXME (149422): This buffer should not be unconditionally unaccelerated.
    std::unique_ptr<ImageBuffer> buffer(ImageBuffer::create(paintingRect.size(), RenderingMode::Unaccelerated, deviceScaleFactor));
    if (!buffer)
        return nullptr;

    layer->paint(buffer->context(), paintingRect, LayoutSize(), { PaintBehavior::FlattenCompositingLayers, PaintBehavior::Snapshotting }, nullptr, RenderLayer::paintLayerPaintingCompositingAllPhasesFlags());

    return ImageBuffer::sinkIntoImage(WTFMove(buffer));
}

void MediaControlTextTrackContainerElement::textTrackRepresentationBoundsChanged(const IntRect&)
{
    if (hasChildNodes())
        updateTextTrackRepresentation();
    updateSizes();
}

#if !RELEASE_LOG_DISABLED
const Logger& MediaControlTextTrackContainerElement::logger() const
{
    return document().logger();
}

const void* MediaControlTextTrackContainerElement::logIdentifier() const
{
    if (auto mediaElement = parentMediaElement(this))
        return mediaElement->logIdentifier();
    return nullptr;
}

WTFLogChannel& MediaControlTextTrackContainerElement::logChannel() const
{
    return LogMedia;
}
#endif // !RELEASE_LOG_DISABLED

#endif // ENABLE(VIDEO_TRACK)

// ----------------------------

} // namespace WebCore

#endif // ENABLE(VIDEO)
