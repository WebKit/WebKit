/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSSelector.h"

#include "CSSOMUtils.h"
#include "HTMLNames.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;

class CSSSelectorBag : public Noncopyable {
public:
    ~CSSSelectorBag()
    {
        ASSERT(isEmpty());
    }

    bool isEmpty() const
    {
        return m_stack.isEmpty();
    }

    void add(PassOwnPtr<CSSSelector> selector)
    {
        if (selector)
            m_stack.append(selector.leakPtr());
    }

    PassOwnPtr<CSSSelector> takeAny()
    {
        ASSERT(!isEmpty());
        OwnPtr<CSSSelector> selector = adoptPtr(m_stack.last());
        m_stack.removeLast();
        return selector.release();
    }

private:
    Vector<CSSSelector*, 16> m_stack;
};

unsigned CSSSelector::specificity() const
{
    // make sure the result doesn't overflow
    static const unsigned maxValueMask = 0xffffff;
    unsigned total = 0;
    for (const CSSSelector* selector = this; selector; selector = selector->tagHistory()) {
        if (selector->m_isForPage)
            return (total + selector->specificityForPage()) & maxValueMask;
        total = (total + selector->specificityForOneSelector()) & maxValueMask;
    }
    return total;
}

inline unsigned CSSSelector::specificityForOneSelector() const
{
    // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity. This function
    // isn't quite correct.
    unsigned s = (m_tag.localName() == starAtom ? 0 : 1);
    switch (m_match) {
    case Id:
        s += 0x10000;
        break;
    case Exact:
    case Class:
    case Set:
    case List:
    case Hyphen:
    case PseudoClass:
    case PseudoElement:
    case Contain:
    case Begin:
    case End:
        s += 0x100;
    case None:
        break;
    }
    return s;
}

unsigned CSSSelector::specificityForPage() const
{
    // See http://dev.w3.org/csswg/css3-page/#cascading-and-page-context
    unsigned s = (m_tag.localName() == starAtom ? 0 : 4);

    switch (pseudoType()) {
    case PseudoFirstPage:
        s += 2;
        break;
    case PseudoLeftPage:
    case PseudoRightPage:
        s += 1;
        break;
    case PseudoNotParsed:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return s;
}

PseudoId CSSSelector::pseudoId(PseudoType type)
{
    switch (type) {
    case PseudoFirstLine:
        return FIRST_LINE;
    case PseudoFirstLetter:
        return FIRST_LETTER;
    case PseudoSelection:
        return SELECTION;
    case PseudoBefore:
        return BEFORE;
    case PseudoAfter:
        return AFTER;
    case PseudoFileUploadButton:
        return FILE_UPLOAD_BUTTON;
    case PseudoInputPlaceholder:
        return INPUT_PLACEHOLDER;
#if ENABLE(INPUT_SPEECH)
    case PseudoInputSpeechButton:
        return INPUT_SPEECH_BUTTON;
#endif
    case PseudoSliderThumb:
        return SLIDER_THUMB;
    case PseudoSearchCancelButton:
        return SEARCH_CANCEL_BUTTON;
    case PseudoSearchDecoration:
        return SEARCH_DECORATION;
    case PseudoSearchResultsDecoration:
        return SEARCH_RESULTS_DECORATION;
    case PseudoSearchResultsButton:
        return SEARCH_RESULTS_BUTTON;
    case PseudoMediaControlsPanel:
        return MEDIA_CONTROLS_PANEL;
    case PseudoMediaControlsMuteButton:
        return MEDIA_CONTROLS_MUTE_BUTTON;
    case PseudoMediaControlsPlayButton:
        return MEDIA_CONTROLS_PLAY_BUTTON;
    case PseudoMediaControlsTimelineContainer:
        return MEDIA_CONTROLS_TIMELINE_CONTAINER;
    case PseudoMediaControlsVolumeSliderContainer:
        return MEDIA_CONTROLS_VOLUME_SLIDER_CONTAINER;
    case PseudoMediaControlsCurrentTimeDisplay:
        return MEDIA_CONTROLS_CURRENT_TIME_DISPLAY;
    case PseudoMediaControlsTimeRemainingDisplay:
        return MEDIA_CONTROLS_TIME_REMAINING_DISPLAY;
    case PseudoMediaControlsTimeline:
        return MEDIA_CONTROLS_TIMELINE;
    case PseudoMediaControlsVolumeSlider:
        return MEDIA_CONTROLS_VOLUME_SLIDER;
    case PseudoMediaControlsVolumeSliderMuteButton:
        return MEDIA_CONTROLS_VOLUME_SLIDER_MUTE_BUTTON;
    case PseudoMediaControlsSeekBackButton:
        return MEDIA_CONTROLS_SEEK_BACK_BUTTON;
    case PseudoMediaControlsSeekForwardButton:
        return MEDIA_CONTROLS_SEEK_FORWARD_BUTTON;
    case PseudoMediaControlsRewindButton:
        return MEDIA_CONTROLS_REWIND_BUTTON;
    case PseudoMediaControlsReturnToRealtimeButton:
        return MEDIA_CONTROLS_RETURN_TO_REALTIME_BUTTON;
    case PseudoMediaControlsToggleClosedCaptions:
        return MEDIA_CONTROLS_TOGGLE_CLOSED_CAPTIONS_BUTTON;
    case PseudoMediaControlsStatusDisplay:
        return MEDIA_CONTROLS_STATUS_DISPLAY;
    case PseudoMediaControlsFullscreenButton:
        return MEDIA_CONTROLS_FULLSCREEN_BUTTON;
    case PseudoScrollbar:
        return SCROLLBAR;
    case PseudoScrollbarButton:
        return SCROLLBAR_BUTTON;
    case PseudoScrollbarCorner:
        return SCROLLBAR_CORNER;
    case PseudoScrollbarThumb:
        return SCROLLBAR_THUMB;
    case PseudoScrollbarTrack:
        return SCROLLBAR_TRACK;
    case PseudoScrollbarTrackPiece:
        return SCROLLBAR_TRACK_PIECE;
    case PseudoResizer:
        return RESIZER;
    case PseudoInnerSpinButton:
        return INNER_SPIN_BUTTON;
    case PseudoOuterSpinButton:
        return OUTER_SPIN_BUTTON;
    case PseudoProgressBarValue:
#if ENABLE(PROGRESS_TAG)
        return PROGRESS_BAR_VALUE;
#else
        ASSERT_NOT_REACHED();
        return NOPSEUDO;
#endif

#if ENABLE(METER_TAG)
    case PseudoMeterHorizontalBar:
        return METER_HORIZONTAL_BAR;
    case PseudoMeterHorizontalOptimum:
        return METER_HORIZONTAL_OPTIMUM;
    case PseudoMeterHorizontalSuboptimal:
        return METER_HORIZONTAL_SUBOPTIMAL;
    case PseudoMeterHorizontalEvenLessGood:
        return METER_HORIZONTAL_EVEN_LESS_GOOD;
    case PseudoMeterVerticalBar:
        return METER_VERTICAL_BAR;
    case PseudoMeterVerticalOptimum:
        return METER_VERTICAL_OPTIMUM;
    case PseudoMeterVerticalSuboptimal:
        return METER_VERTICAL_SUBOPTIMAL;
    case PseudoMeterVerticalEvenLessGood:
        return METER_VERTICAL_EVEN_LESS_GOOD;
#else
    case PseudoMeterHorizontalBar:
    case PseudoMeterHorizontalOptimum:
    case PseudoMeterHorizontalSuboptimal:
    case PseudoMeterHorizontalEvenLessGood:
    case PseudoMeterVerticalBar:
    case PseudoMeterVerticalOptimum:
    case PseudoMeterVerticalSuboptimal:
    case PseudoMeterVerticalEvenLessGood:
        ASSERT_NOT_REACHED();
        return NOPSEUDO;
#endif

#if ENABLE(FULLSCREEN_API)
    case PseudoFullScreen:
        return FULL_SCREEN;
    case PseudoFullScreenDocument:
        return FULL_SCREEN_DOCUMENT;
#endif
            
    case PseudoInputListButton:
#if ENABLE(DATALIST)
        return INPUT_LIST_BUTTON;
#endif
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
        return NOPSEUDO;
    case PseudoNotParsed:
        ASSERT_NOT_REACHED();
        return NOPSEUDO;
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

static HashMap<AtomicStringImpl*, CSSSelector::PseudoType>* nameToPseudoTypeMap()
{
    DEFINE_STATIC_LOCAL(AtomicString, active, ("active"));
    DEFINE_STATIC_LOCAL(AtomicString, after, ("after"));
    DEFINE_STATIC_LOCAL(AtomicString, anyLink, ("-webkit-any-link"));
    DEFINE_STATIC_LOCAL(AtomicString, autofill, ("-webkit-autofill"));
    DEFINE_STATIC_LOCAL(AtomicString, before, ("before"));
    DEFINE_STATIC_LOCAL(AtomicString, checked, ("checked"));
    DEFINE_STATIC_LOCAL(AtomicString, fileUploadButton, ("-webkit-file-upload-button"));
#if ENABLE(INPUT_SPEECH)
    DEFINE_STATIC_LOCAL(AtomicString, inputSpeechButton, ("-webkit-input-speech-button"));
#endif
    DEFINE_STATIC_LOCAL(AtomicString, defaultString, ("default"));
    DEFINE_STATIC_LOCAL(AtomicString, disabled, ("disabled"));
    DEFINE_STATIC_LOCAL(AtomicString, readOnly, ("read-only"));
    DEFINE_STATIC_LOCAL(AtomicString, readWrite, ("read-write"));
    DEFINE_STATIC_LOCAL(AtomicString, valid, ("valid"));
    DEFINE_STATIC_LOCAL(AtomicString, invalid, ("invalid"));
    DEFINE_STATIC_LOCAL(AtomicString, drag, ("-webkit-drag"));
    DEFINE_STATIC_LOCAL(AtomicString, dragAlias, ("-khtml-drag")); // was documented with this name in Apple documentation, so keep an alia
    DEFINE_STATIC_LOCAL(AtomicString, empty, ("empty"));
    DEFINE_STATIC_LOCAL(AtomicString, enabled, ("enabled"));
    DEFINE_STATIC_LOCAL(AtomicString, firstChild, ("first-child"));
    DEFINE_STATIC_LOCAL(AtomicString, firstLetter, ("first-letter"));
    DEFINE_STATIC_LOCAL(AtomicString, firstLine, ("first-line"));
    DEFINE_STATIC_LOCAL(AtomicString, firstOfType, ("first-of-type"));
    DEFINE_STATIC_LOCAL(AtomicString, fullPageMedia, ("-webkit-full-page-media"));
    DEFINE_STATIC_LOCAL(AtomicString, nthChild, ("nth-child("));
    DEFINE_STATIC_LOCAL(AtomicString, nthOfType, ("nth-of-type("));
    DEFINE_STATIC_LOCAL(AtomicString, nthLastChild, ("nth-last-child("));
    DEFINE_STATIC_LOCAL(AtomicString, nthLastOfType, ("nth-last-of-type("));
    DEFINE_STATIC_LOCAL(AtomicString, focus, ("focus"));
    DEFINE_STATIC_LOCAL(AtomicString, hover, ("hover"));
    DEFINE_STATIC_LOCAL(AtomicString, indeterminate, ("indeterminate"));
    DEFINE_STATIC_LOCAL(AtomicString, innerSpinButton, ("-webkit-inner-spin-button"));
#if ENABLE(DATALIST)
    DEFINE_STATIC_LOCAL(AtomicString, inputListButton, ("-webkit-input-list-button"));
#endif
    DEFINE_STATIC_LOCAL(AtomicString, inputPlaceholder, ("-webkit-input-placeholder"));
    DEFINE_STATIC_LOCAL(AtomicString, lastChild, ("last-child"));
    DEFINE_STATIC_LOCAL(AtomicString, lastOfType, ("last-of-type"));
    DEFINE_STATIC_LOCAL(AtomicString, link, ("link"));
    DEFINE_STATIC_LOCAL(AtomicString, lang, ("lang("));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsPanel, ("-webkit-media-controls-panel"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsMuteButton, ("-webkit-media-controls-mute-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsPlayButton, ("-webkit-media-controls-play-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsTimeline, ("-webkit-media-controls-timeline"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsVolumeSlider, ("-webkit-media-controls-volume-slider"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsVolumeSliderMuteButton, ("-webkit-media-controls-volume-slider-mute-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsSeekBackButton, ("-webkit-media-controls-seek-back-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsSeekForwardButton, ("-webkit-media-controls-seek-forward-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsRewindButton, ("-webkit-media-controls-rewind-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsReturnToRealtimeButton, ("-webkit-media-controls-return-to-realtime-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsToggleClosedCaptionsButton, ("-webkit-media-controls-toggle-closed-captions-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsStatusDisplay, ("-webkit-media-controls-status-display"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsFullscreenButton, ("-webkit-media-controls-fullscreen-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsTimelineContainer, ("-webkit-media-controls-timeline-container"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsVolumeSliderContainer, ("-webkit-media-controls-volume-slider-container"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsCurrentTimeDisplay, ("-webkit-media-controls-current-time-display"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsTimeRemainingDisplay, ("-webkit-media-controls-time-remaining-display"));
    DEFINE_STATIC_LOCAL(AtomicString, notStr, ("not("));
    DEFINE_STATIC_LOCAL(AtomicString, onlyChild, ("only-child"));
    DEFINE_STATIC_LOCAL(AtomicString, onlyOfType, ("only-of-type"));
    DEFINE_STATIC_LOCAL(AtomicString, optional, ("optional"));
    DEFINE_STATIC_LOCAL(AtomicString, outerSpinButton, ("-webkit-outer-spin-button"));
#if ENABLE(PROGRESS_TAG)
    DEFINE_STATIC_LOCAL(AtomicString, progressBarValue, ("-webkit-progress-bar-value"));
#endif

#if ENABLE(METER_TAG)
    DEFINE_STATIC_LOCAL(AtomicString, meterHorizontalBar, ("-webkit-meter-horizontal-bar"));
    DEFINE_STATIC_LOCAL(AtomicString, meterHorizontalOptimumValue, ("-webkit-meter-horizontal-optimum-value"));
    DEFINE_STATIC_LOCAL(AtomicString, meterHorizontalSuboptimalValue, ("-webkit-meter-horizontal-suboptimal-value"));
    DEFINE_STATIC_LOCAL(AtomicString, meterHorizontalEvenLessGoodValue, ("-webkit-meter-horizontal-even-less-good-value"));
    DEFINE_STATIC_LOCAL(AtomicString, meterVerticalBar, ("-webkit-meter-vertical-bar"));
    DEFINE_STATIC_LOCAL(AtomicString, meterVerticalOptimumValue, ("-webkit-meter-vertical-optimum-value"));
    DEFINE_STATIC_LOCAL(AtomicString, meterVerticalSuboptimalValue, ("-webkit-meter-vertical-suboptimal-value"));
    DEFINE_STATIC_LOCAL(AtomicString, meterVerticalEvenLessGoodValue, ("-webkit-meter-vertical-even-less-good-value"));
#endif

    DEFINE_STATIC_LOCAL(AtomicString, required, ("required"));
    DEFINE_STATIC_LOCAL(AtomicString, resizer, ("-webkit-resizer"));
    DEFINE_STATIC_LOCAL(AtomicString, root, ("root"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbar, ("-webkit-scrollbar"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarButton, ("-webkit-scrollbar-button"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarCorner, ("-webkit-scrollbar-corner"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarThumb, ("-webkit-scrollbar-thumb"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarTrack, ("-webkit-scrollbar-track"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarTrackPiece, ("-webkit-scrollbar-track-piece"));
    DEFINE_STATIC_LOCAL(AtomicString, searchCancelButton, ("-webkit-search-cancel-button"));
    DEFINE_STATIC_LOCAL(AtomicString, searchDecoration, ("-webkit-search-decoration"));
    DEFINE_STATIC_LOCAL(AtomicString, searchResultsDecoration, ("-webkit-search-results-decoration"));
    DEFINE_STATIC_LOCAL(AtomicString, searchResultsButton, ("-webkit-search-results-button"));
    DEFINE_STATIC_LOCAL(AtomicString, selection, ("selection"));
    DEFINE_STATIC_LOCAL(AtomicString, sliderThumb, ("-webkit-slider-thumb"));
    DEFINE_STATIC_LOCAL(AtomicString, target, ("target"));
    DEFINE_STATIC_LOCAL(AtomicString, visited, ("visited"));
    DEFINE_STATIC_LOCAL(AtomicString, windowInactive, ("window-inactive"));
    DEFINE_STATIC_LOCAL(AtomicString, decrement, ("decrement"));
    DEFINE_STATIC_LOCAL(AtomicString, increment, ("increment"));
    DEFINE_STATIC_LOCAL(AtomicString, start, ("start"));
    DEFINE_STATIC_LOCAL(AtomicString, end, ("end"));
    DEFINE_STATIC_LOCAL(AtomicString, horizontal, ("horizontal"));
    DEFINE_STATIC_LOCAL(AtomicString, vertical, ("vertical"));
    DEFINE_STATIC_LOCAL(AtomicString, doubleButton, ("double-button"));
    DEFINE_STATIC_LOCAL(AtomicString, singleButton, ("single-button"));
    DEFINE_STATIC_LOCAL(AtomicString, noButton, ("no-button"));
    DEFINE_STATIC_LOCAL(AtomicString, cornerPresent, ("corner-present"));
    // Paged Media pseudo-classes
    DEFINE_STATIC_LOCAL(AtomicString, firstPage, ("first"));
    DEFINE_STATIC_LOCAL(AtomicString, leftPage, ("left"));
    DEFINE_STATIC_LOCAL(AtomicString, rightPage, ("right"));
#if ENABLE(FULLSCREEN_API)
    DEFINE_STATIC_LOCAL(AtomicString, fullScreen, ("-webkit-full-screen"));
    DEFINE_STATIC_LOCAL(AtomicString, fullScreenDocument, ("-webkit-full-screen-document"));
#endif

    static HashMap<AtomicStringImpl*, CSSSelector::PseudoType>* nameToPseudoType = 0;
    if (!nameToPseudoType) {
        nameToPseudoType = new HashMap<AtomicStringImpl*, CSSSelector::PseudoType>;
        nameToPseudoType->set(active.impl(), CSSSelector::PseudoActive);
        nameToPseudoType->set(after.impl(), CSSSelector::PseudoAfter);
        nameToPseudoType->set(anyLink.impl(), CSSSelector::PseudoAnyLink);
        nameToPseudoType->set(autofill.impl(), CSSSelector::PseudoAutofill);
        nameToPseudoType->set(before.impl(), CSSSelector::PseudoBefore);
        nameToPseudoType->set(checked.impl(), CSSSelector::PseudoChecked);
        nameToPseudoType->set(fileUploadButton.impl(), CSSSelector::PseudoFileUploadButton);
#if ENABLE(INPUT_SPEECH)
        nameToPseudoType->set(inputSpeechButton.impl(), CSSSelector::PseudoInputSpeechButton);
#endif
        nameToPseudoType->set(defaultString.impl(), CSSSelector::PseudoDefault);
        nameToPseudoType->set(disabled.impl(), CSSSelector::PseudoDisabled);
        nameToPseudoType->set(readOnly.impl(), CSSSelector::PseudoReadOnly);
        nameToPseudoType->set(readWrite.impl(), CSSSelector::PseudoReadWrite);
        nameToPseudoType->set(valid.impl(), CSSSelector::PseudoValid);
        nameToPseudoType->set(invalid.impl(), CSSSelector::PseudoInvalid);
        nameToPseudoType->set(drag.impl(), CSSSelector::PseudoDrag);
        nameToPseudoType->set(dragAlias.impl(), CSSSelector::PseudoDrag);
        nameToPseudoType->set(enabled.impl(), CSSSelector::PseudoEnabled);
        nameToPseudoType->set(empty.impl(), CSSSelector::PseudoEmpty);
        nameToPseudoType->set(firstChild.impl(), CSSSelector::PseudoFirstChild);
        nameToPseudoType->set(fullPageMedia.impl(), CSSSelector::PseudoFullPageMedia);
#if ENABLE(DATALIST)
        nameToPseudoType->set(inputListButton.impl(), CSSSelector::PseudoInputListButton);
#endif
        nameToPseudoType->set(inputPlaceholder.impl(), CSSSelector::PseudoInputPlaceholder);
        nameToPseudoType->set(lastChild.impl(), CSSSelector::PseudoLastChild);
        nameToPseudoType->set(lastOfType.impl(), CSSSelector::PseudoLastOfType);
        nameToPseudoType->set(onlyChild.impl(), CSSSelector::PseudoOnlyChild);
        nameToPseudoType->set(onlyOfType.impl(), CSSSelector::PseudoOnlyOfType);
        nameToPseudoType->set(firstLetter.impl(), CSSSelector::PseudoFirstLetter);
        nameToPseudoType->set(firstLine.impl(), CSSSelector::PseudoFirstLine);
        nameToPseudoType->set(firstOfType.impl(), CSSSelector::PseudoFirstOfType);
        nameToPseudoType->set(focus.impl(), CSSSelector::PseudoFocus);
        nameToPseudoType->set(hover.impl(), CSSSelector::PseudoHover);
        nameToPseudoType->set(indeterminate.impl(), CSSSelector::PseudoIndeterminate);
        nameToPseudoType->set(innerSpinButton.impl(), CSSSelector::PseudoInnerSpinButton);
        nameToPseudoType->set(link.impl(), CSSSelector::PseudoLink);
        nameToPseudoType->set(lang.impl(), CSSSelector::PseudoLang);
        nameToPseudoType->set(mediaControlsPanel.impl(), CSSSelector::PseudoMediaControlsPanel);
        nameToPseudoType->set(mediaControlsMuteButton.impl(), CSSSelector::PseudoMediaControlsMuteButton);
        nameToPseudoType->set(mediaControlsPlayButton.impl(), CSSSelector::PseudoMediaControlsPlayButton);
        nameToPseudoType->set(mediaControlsCurrentTimeDisplay.impl(), CSSSelector::PseudoMediaControlsCurrentTimeDisplay);
        nameToPseudoType->set(mediaControlsTimeRemainingDisplay.impl(), CSSSelector::PseudoMediaControlsTimeRemainingDisplay);
        nameToPseudoType->set(mediaControlsTimeline.impl(), CSSSelector::PseudoMediaControlsTimeline);
        nameToPseudoType->set(mediaControlsVolumeSlider.impl(), CSSSelector::PseudoMediaControlsVolumeSlider);
        nameToPseudoType->set(mediaControlsVolumeSliderMuteButton.impl(), CSSSelector::PseudoMediaControlsVolumeSliderMuteButton);
        nameToPseudoType->set(mediaControlsSeekBackButton.impl(), CSSSelector::PseudoMediaControlsSeekBackButton);
        nameToPseudoType->set(mediaControlsSeekForwardButton.impl(), CSSSelector::PseudoMediaControlsSeekForwardButton);
        nameToPseudoType->set(mediaControlsRewindButton.impl(), CSSSelector::PseudoMediaControlsRewindButton);
        nameToPseudoType->set(mediaControlsReturnToRealtimeButton.impl(), CSSSelector::PseudoMediaControlsReturnToRealtimeButton);
        nameToPseudoType->set(mediaControlsToggleClosedCaptionsButton.impl(), CSSSelector::PseudoMediaControlsToggleClosedCaptions);
        nameToPseudoType->set(mediaControlsStatusDisplay.impl(), CSSSelector::PseudoMediaControlsStatusDisplay);
        nameToPseudoType->set(mediaControlsFullscreenButton.impl(), CSSSelector::PseudoMediaControlsFullscreenButton);
        nameToPseudoType->set(mediaControlsTimelineContainer.impl(), CSSSelector::PseudoMediaControlsTimelineContainer);
        nameToPseudoType->set(mediaControlsVolumeSliderContainer.impl(), CSSSelector::PseudoMediaControlsVolumeSliderContainer);
        nameToPseudoType->set(notStr.impl(), CSSSelector::PseudoNot);
        nameToPseudoType->set(nthChild.impl(), CSSSelector::PseudoNthChild);
        nameToPseudoType->set(nthOfType.impl(), CSSSelector::PseudoNthOfType);
        nameToPseudoType->set(nthLastChild.impl(), CSSSelector::PseudoNthLastChild);
        nameToPseudoType->set(nthLastOfType.impl(), CSSSelector::PseudoNthLastOfType);
        nameToPseudoType->set(outerSpinButton.impl(), CSSSelector::PseudoOuterSpinButton);
#if ENABLE(PROGRESS_TAG)
        nameToPseudoType->set(progressBarValue.impl(), CSSSelector::PseudoProgressBarValue);
#endif
#if ENABLE(METER_TAG)
        nameToPseudoType->set(meterHorizontalBar.impl(), CSSSelector::PseudoMeterHorizontalBar);
        nameToPseudoType->set(meterHorizontalOptimumValue.impl(), CSSSelector::PseudoMeterHorizontalOptimum);
        nameToPseudoType->set(meterHorizontalSuboptimalValue.impl(), CSSSelector::PseudoMeterHorizontalSuboptimal);
        nameToPseudoType->set(meterHorizontalEvenLessGoodValue.impl(), CSSSelector::PseudoMeterHorizontalEvenLessGood);
        nameToPseudoType->set(meterVerticalBar.impl(), CSSSelector::PseudoMeterVerticalBar);
        nameToPseudoType->set(meterVerticalOptimumValue.impl(), CSSSelector::PseudoMeterVerticalOptimum);
        nameToPseudoType->set(meterVerticalSuboptimalValue.impl(), CSSSelector::PseudoMeterVerticalSuboptimal);
        nameToPseudoType->set(meterVerticalEvenLessGoodValue.impl(), CSSSelector::PseudoMeterVerticalEvenLessGood);
#endif
        nameToPseudoType->set(root.impl(), CSSSelector::PseudoRoot);
        nameToPseudoType->set(windowInactive.impl(), CSSSelector::PseudoWindowInactive);
        nameToPseudoType->set(decrement.impl(), CSSSelector::PseudoDecrement);
        nameToPseudoType->set(increment.impl(), CSSSelector::PseudoIncrement);
        nameToPseudoType->set(start.impl(), CSSSelector::PseudoStart);
        nameToPseudoType->set(end.impl(), CSSSelector::PseudoEnd);
        nameToPseudoType->set(horizontal.impl(), CSSSelector::PseudoHorizontal);
        nameToPseudoType->set(vertical.impl(), CSSSelector::PseudoVertical);
        nameToPseudoType->set(doubleButton.impl(), CSSSelector::PseudoDoubleButton);
        nameToPseudoType->set(singleButton.impl(), CSSSelector::PseudoSingleButton);
        nameToPseudoType->set(noButton.impl(), CSSSelector::PseudoNoButton);
        nameToPseudoType->set(optional.impl(), CSSSelector::PseudoOptional);
        nameToPseudoType->set(required.impl(), CSSSelector::PseudoRequired);
        nameToPseudoType->set(resizer.impl(), CSSSelector::PseudoResizer);
        nameToPseudoType->set(scrollbar.impl(), CSSSelector::PseudoScrollbar);
        nameToPseudoType->set(scrollbarButton.impl(), CSSSelector::PseudoScrollbarButton);
        nameToPseudoType->set(scrollbarCorner.impl(), CSSSelector::PseudoScrollbarCorner);
        nameToPseudoType->set(scrollbarThumb.impl(), CSSSelector::PseudoScrollbarThumb);
        nameToPseudoType->set(scrollbarTrack.impl(), CSSSelector::PseudoScrollbarTrack);
        nameToPseudoType->set(scrollbarTrackPiece.impl(), CSSSelector::PseudoScrollbarTrackPiece);
        nameToPseudoType->set(cornerPresent.impl(), CSSSelector::PseudoCornerPresent);
        nameToPseudoType->set(searchCancelButton.impl(), CSSSelector::PseudoSearchCancelButton);
        nameToPseudoType->set(searchDecoration.impl(), CSSSelector::PseudoSearchDecoration);
        nameToPseudoType->set(searchResultsDecoration.impl(), CSSSelector::PseudoSearchResultsDecoration);
        nameToPseudoType->set(searchResultsButton.impl(), CSSSelector::PseudoSearchResultsButton);
        nameToPseudoType->set(selection.impl(), CSSSelector::PseudoSelection);
        nameToPseudoType->set(sliderThumb.impl(), CSSSelector::PseudoSliderThumb);
        nameToPseudoType->set(target.impl(), CSSSelector::PseudoTarget);
        nameToPseudoType->set(visited.impl(), CSSSelector::PseudoVisited);
        nameToPseudoType->set(firstPage.impl(), CSSSelector::PseudoFirstPage);
        nameToPseudoType->set(leftPage.impl(), CSSSelector::PseudoLeftPage);
        nameToPseudoType->set(rightPage.impl(), CSSSelector::PseudoRightPage);
#if ENABLE(FULLSCREEN_API)
        nameToPseudoType->set(fullScreen.impl(), CSSSelector::PseudoFullScreen);
        nameToPseudoType->set(fullScreenDocument.impl(), CSSSelector::PseudoFullScreenDocument);
#endif
    }
    return nameToPseudoType;
}

CSSSelector::PseudoType CSSSelector::parsePseudoType(const AtomicString& name)
{
    if (name.isNull())
        return PseudoUnknown;
    HashMap<AtomicStringImpl*, CSSSelector::PseudoType>* nameToPseudoType = nameToPseudoTypeMap();
    HashMap<AtomicStringImpl*, CSSSelector::PseudoType>::iterator slot = nameToPseudoType->find(name.impl());
    return slot == nameToPseudoType->end() ? PseudoUnknown : slot->second;
}

void CSSSelector::extractPseudoType() const
{
    if (m_match != PseudoClass && m_match != PseudoElement && m_match != PagePseudoClass)
        return;

    m_pseudoType = parsePseudoType(m_value);

    bool element = false; // pseudo-element
    bool compat = false; // single colon compatbility mode
    bool isPagePseudoClass = false; // Page pseudo-class

    switch (m_pseudoType) {
    case PseudoAfter:
    case PseudoBefore:
    case PseudoFirstLetter:
    case PseudoFirstLine:
        compat = true;
    case PseudoFileUploadButton:
    case PseudoInputListButton:
    case PseudoInputPlaceholder:
#if ENABLE(INPUT_SPEECH)
    case PseudoInputSpeechButton:
#endif
    case PseudoInnerSpinButton:
    case PseudoMediaControlsPanel:
    case PseudoMediaControlsMuteButton:
    case PseudoMediaControlsPlayButton:
    case PseudoMediaControlsCurrentTimeDisplay:
    case PseudoMediaControlsTimeRemainingDisplay:
    case PseudoMediaControlsTimeline:
    case PseudoMediaControlsVolumeSlider:
    case PseudoMediaControlsVolumeSliderMuteButton:
    case PseudoMediaControlsSeekBackButton:
    case PseudoMediaControlsSeekForwardButton:
    case PseudoMediaControlsRewindButton:
    case PseudoMediaControlsReturnToRealtimeButton:
    case PseudoMediaControlsToggleClosedCaptions:
    case PseudoMediaControlsStatusDisplay:
    case PseudoMediaControlsFullscreenButton:
    case PseudoMediaControlsTimelineContainer:
    case PseudoMediaControlsVolumeSliderContainer:
    case PseudoMeterHorizontalBar:
    case PseudoMeterHorizontalOptimum:
    case PseudoMeterHorizontalSuboptimal:
    case PseudoMeterHorizontalEvenLessGood:
    case PseudoMeterVerticalBar:
    case PseudoMeterVerticalOptimum:
    case PseudoMeterVerticalSuboptimal:
    case PseudoMeterVerticalEvenLessGood:
    case PseudoOuterSpinButton:
    case PseudoProgressBarValue:
    case PseudoResizer:
    case PseudoScrollbar:
    case PseudoScrollbarCorner:
    case PseudoScrollbarButton:
    case PseudoScrollbarThumb:
    case PseudoScrollbarTrack:
    case PseudoScrollbarTrackPiece:
    case PseudoSearchCancelButton:
    case PseudoSearchDecoration:
    case PseudoSearchResultsDecoration:
    case PseudoSearchResultsButton:
    case PseudoSelection:
    case PseudoSliderThumb:
        element = true;
        break;
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoNotParsed:
#if ENABLE(FULLSCREEN_API)
    case PseudoFullScreen:
    case PseudoFullScreenDocument:
#endif
        break;
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
        isPagePseudoClass = true;
        break;
    }

    bool matchPagePseudoClass = (m_match == PagePseudoClass);
    if (matchPagePseudoClass != isPagePseudoClass)
        m_pseudoType = PseudoUnknown;
    else if (m_match == PseudoClass && element) {
        if (!compat)
            m_pseudoType = PseudoUnknown;
        else
           m_match = PseudoElement;
    } else if (m_match == PseudoElement && !element)
        m_pseudoType = PseudoUnknown;
}

bool CSSSelector::operator==(const CSSSelector& other)
{
    const CSSSelector* sel1 = this;
    const CSSSelector* sel2 = &other;

    while (sel1 && sel2) {
        if (sel1->m_tag != sel2->m_tag || sel1->attribute() != sel2->attribute() ||
             sel1->relation() != sel2->relation() || sel1->m_match != sel2->m_match ||
             sel1->m_value != sel2->m_value ||
             sel1->pseudoType() != sel2->pseudoType() ||
             sel1->argument() != sel2->argument())
            return false;
        sel1 = sel1->tagHistory();
        sel2 = sel2->tagHistory();
    }

    if (sel1 || sel2)
        return false;

    return true;
}

String CSSSelector::selectorText() const
{
    String str = "";

    const AtomicString& prefix = m_tag.prefix();
    const AtomicString& localName = m_tag.localName();
    if (m_match == CSSSelector::None || !prefix.isNull() || localName != starAtom) {
        if (prefix.isNull())
            str = localName;
        else {
            str = prefix.string();
            str.append("|");
            str.append(localName);
        }
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->m_match == CSSSelector::Id) {
            str += "#";
            serializeIdentifier(cs->m_value, str);
        } else if (cs->m_match == CSSSelector::Class) {
            str += ".";
            serializeIdentifier(cs->m_value, str);
        } else if (cs->m_match == CSSSelector::PseudoClass || cs->m_match == CSSSelector::PagePseudoClass) {
            str += ":";
            str += cs->m_value;
            if (cs->pseudoType() == PseudoNot) {
                if (CSSSelector* subSel = cs->simpleSelector())
                    str += subSel->selectorText();
                str += ")";
            } else if (cs->pseudoType() == PseudoLang
                    || cs->pseudoType() == PseudoNthChild
                    || cs->pseudoType() == PseudoNthLastChild
                    || cs->pseudoType() == PseudoNthOfType
                    || cs->pseudoType() == PseudoNthLastOfType) {
                str += cs->argument();
                str += ")";
            }
        } else if (cs->m_match == CSSSelector::PseudoElement) {
            str += "::";
            str += cs->m_value;
        } else if (cs->hasAttribute()) {
            str += "[";
            const AtomicString& prefix = cs->attribute().prefix();
            if (!prefix.isNull()) {
                str.append(prefix);
                str.append("|");
            }
            str += cs->attribute().localName();
            switch (cs->m_match) {
                case CSSSelector::Exact:
                    str += "=";
                    break;
                case CSSSelector::Set:
                    // set has no operator or value, just the attrName
                    str += "]";
                    break;
                case CSSSelector::List:
                    str += "~=";
                    break;
                case CSSSelector::Hyphen:
                    str += "|=";
                    break;
                case CSSSelector::Begin:
                    str += "^=";
                    break;
                case CSSSelector::End:
                    str += "$=";
                    break;
                case CSSSelector::Contain:
                    str += "*=";
                    break;
                default:
                    break;
            }
            if (cs->m_match != CSSSelector::Set) {
                serializeString(cs->m_value, str);
                str += "]";
            }
        }
        if (cs->relation() != CSSSelector::SubSelector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    if (CSSSelector* tagHistory = cs->tagHistory()) {
        String tagHistoryText = tagHistory->selectorText();
        if (cs->relation() == CSSSelector::DirectAdjacent)
            str = tagHistoryText + " + " + str;
        else if (cs->relation() == CSSSelector::IndirectAdjacent)
            str = tagHistoryText + " ~ " + str;
        else if (cs->relation() == CSSSelector::Child)
            str = tagHistoryText + " > " + str;
        else
            // Descendant
            str = tagHistoryText + " " + str;
    }

    return str;
}
    
void CSSSelector::setTagHistory(CSSSelector* tagHistory) 
{ 
    if (m_hasRareData) 
        m_data.m_rareData->m_tagHistory.set(tagHistory); 
    else 
        m_data.m_tagHistory = tagHistory; 
}

const QualifiedName& CSSSelector::attribute() const
{ 
    switch (m_match) {
    case Id:
        return idAttr;
    case Class:
        return classAttr;
    default:
        return m_hasRareData ? m_data.m_rareData->m_attribute : anyQName();
    }
}

void CSSSelector::setAttribute(const QualifiedName& value) 
{ 
    createRareData(); 
    m_data.m_rareData->m_attribute = value; 
}
    
void CSSSelector::setArgument(const AtomicString& value) 
{ 
    createRareData(); 
    m_data.m_rareData->m_argument = value; 
}

void CSSSelector::setSimpleSelector(CSSSelector* value)
{
    createRareData(); 
    m_data.m_rareData->m_simpleSelector.set(value); 
}

bool CSSSelector::parseNth()
{
    if (!m_hasRareData)
        return false;
    if (m_parsedNth)
        return true;
    m_parsedNth = m_data.m_rareData->parseNth();
    return m_parsedNth;
}

bool CSSSelector::matchNth(int count)
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->matchNth(count);
}

bool CSSSelector::isSimple() const
{
    if (simpleSelector() || tagHistory() || matchesPseudoElement())
        return false;

    int numConditions = 0;

    // hasTag() cannot be be used here because namespace may not be nullAtom.
    // Example:
    //     @namespace "http://www.w3.org/2000/svg";
    //     svg:not(:root) { ...
    if (m_tag != starAtom)
        numConditions++;

    if (m_match == Id || m_match == Class || m_match == PseudoClass)
        numConditions++;

    if (m_hasRareData && m_data.m_rareData->m_attribute != anyQName())
        numConditions++;

    // numConditions is 0 for a universal selector.
    // numConditions is 1 for other simple selectors.
    return numConditions <= 1;
}

// a helper function for parsing nth-arguments
bool CSSSelector::RareData::parseNth()
{
    String argument = m_argument.lower();
    
    if (argument.isEmpty())
        return false;
    
    m_a = 0;
    m_b = 0;
    if (argument == "odd") {
        m_a = 2;
        m_b = 1;
    } else if (argument == "even") {
        m_a = 2;
        m_b = 0;
    } else {
        size_t n = argument.find('n');
        if (n != notFound) {
            if (argument[0] == '-') {
                if (n == 1)
                    m_a = -1; // -n == -1n
                else
                    m_a = argument.substring(0, n).toInt();
            } else if (!n)
                m_a = 1; // n == 1n
            else
                m_a = argument.substring(0, n).toInt();
            
            size_t p = argument.find('+', n);
            if (p != notFound)
                m_b = argument.substring(p + 1, argument.length() - p - 1).toInt();
            else {
                p = argument.find('-', n);
                if (p != notFound)
                    m_b = -argument.substring(p + 1, argument.length() - p - 1).toInt();
            }
        } else
            m_b = argument.toInt();
    }
    return true;
}

// a helper function for checking nth-arguments
bool CSSSelector::RareData::matchNth(int count)
{
    if (!m_a)
        return count == m_b;
    else if (m_a > 0) {
        if (count < m_b)
            return false;
        return (count - m_b) % m_a == 0;
    } else {
        if (count > m_b)
            return false;
        return (m_b - count) % (-m_a) == 0;
    }
}

inline void CSSSelector::releaseOwnedSelectorsToBag(CSSSelectorBag& bag)
{
    if (m_hasRareData) {
        ASSERT(m_data.m_rareData);
        bag.add(m_data.m_rareData->m_tagHistory.release());
        bag.add(m_data.m_rareData->m_simpleSelector.release());
        delete m_data.m_rareData;
        // Clear the pointer so that a destructor of this selector will not
        // traverse this chain.
        m_data.m_rareData = 0;
    } else {
        bag.add(adoptPtr(m_data.m_tagHistory));
        // Clear the pointer for the same reason.
        m_data.m_tagHistory = 0;
    }
}

void CSSSelector::deleteReachableSelectors()
{
    // Traverse the chain of selectors and delete each iteratively.
    CSSSelectorBag selectorsToBeDeleted;
    releaseOwnedSelectorsToBag(selectorsToBeDeleted);
    while (!selectorsToBeDeleted.isEmpty()) {
        OwnPtr<CSSSelector> selector(selectorsToBeDeleted.takeAny());
        ASSERT(selector);
        selector->releaseOwnedSelectorsToBag(selectorsToBeDeleted);
    }
}

} // namespace WebCore
