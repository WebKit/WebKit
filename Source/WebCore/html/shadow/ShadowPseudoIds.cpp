/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShadowPseudoIds.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

namespace ShadowPseudoIds {

const AtomString& cue()
{
    static MainThreadNeverDestroyed<const AtomString> cue("cue"_s);
    return cue;
}

const AtomString& fileSelectorButton()
{
    static MainThreadNeverDestroyed<const AtomString> fileSelectorButton("file-selector-button"_s);
    return fileSelectorButton;
}

const AtomString& placeholder()
{
    static MainThreadNeverDestroyed<const AtomString> placeholder("placeholder"_s);
    return placeholder;
}

const AtomString& webkitContactsAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitContactsAutoFillButton("-webkit-contacts-auto-fill-button"_s);
    return webkitContactsAutoFillButton;
}

const AtomString& webkitCredentialsAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitCredentialsAutoFillButton("-webkit-credentials-auto-fill-button"_s);
    return webkitCredentialsAutoFillButton;
}

const AtomString& webkitCreditCardAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitCreditCardAutoFillButton("-webkit-credit-card-auto-fill-button"_s);
    return webkitCreditCardAutoFillButton;
}

const AtomString& webkitLoadingAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitLoadingAutoFillButton("-webkit-loading-auto-fill-button"_s);
    return webkitLoadingAutoFillButton;
}

const AtomString& webkitStrongPasswordAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitStrongPasswordAutoFillButton("-webkit-strong-password-auto-fill-button"_s);
    return webkitStrongPasswordAutoFillButton;
}

const AtomString& webkitCapsLockIndicator()
{
    static MainThreadNeverDestroyed<const AtomString> webkitCapsLockIndicator("-webkit-caps-lock-indicator"_s);
    return webkitCapsLockIndicator;
}

const AtomString& webkitColorSwatch()
{
    static MainThreadNeverDestroyed<const AtomString> webkitColorSwatch("-webkit-color-swatch"_s);
    return webkitColorSwatch;
}

const AtomString& webkitColorSwatchWrapper()
{
    static MainThreadNeverDestroyed<const AtomString> webkitColorSwatchWrapper("-webkit-color-swatch-wrapper"_s);
    return webkitColorSwatchWrapper;
}

const AtomString& webkitDatetimeEdit()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDatetimeEdit("-webkit-datetime-edit"_s);
    return webkitDatetimeEdit;
}

const AtomString& webkitDatetimeEditText()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDatetimeEditText("-webkit-datetime-edit-text"_s);
    return webkitDatetimeEditText;
}

const AtomString& webkitDatetimeEditFieldsWrapper()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDatetimeEditFieldsWrapper("-webkit-datetime-edit-fields-wrapper"_s);
    return webkitDatetimeEditFieldsWrapper;
}

const AtomString& webkitDateAndTimeValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDateAndTimeValue("-webkit-date-and-time-value"_s);
    return webkitDateAndTimeValue;
}

const AtomString& webkitDetailsMarker()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDetailsMarker("-webkit-details-marker"_s);
    return webkitDetailsMarker;
}

const AtomString& webkitGenericCueRoot()
{
    static MainThreadNeverDestroyed<const AtomString> webkitGenericCueRoot("-webkit-generic-cue-root"_s);
    return webkitGenericCueRoot;
}

const AtomString& webkitInnerSpinButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitInnerSpinButton("-webkit-inner-spin-button"_s);
    return webkitInnerSpinButton;
}

const AtomString& webkitListButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitListButton("-webkit-list-button"_s);
    return webkitListButton;
}

const AtomString& webkitMediaTextTrackContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackContainer("-webkit-media-text-track-container"_s);
    return webkitMediaTextTrackContainer;
}

const AtomString& webkitMediaTextTrackDisplay()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackDisplay("-webkit-media-text-track-display"_s);
    return webkitMediaTextTrackDisplay;
}

const AtomString& webkitMediaTextTrackDisplayBackdrop()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackDisplay("-webkit-media-text-track-display-backdrop"_s);
    return webkitMediaTextTrackDisplay;
}

const AtomString& webkitMediaTextTrackRegion()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackRegion("-webkit-media-text-track-region"_s);
    return webkitMediaTextTrackRegion;
}

const AtomString& webkitMediaTextTrackRegionContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackRegionContainer("-webkit-media-text-track-region-container"_s);
    return webkitMediaTextTrackRegionContainer;
}

const AtomString& webkitMeterBar()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterBar("-webkit-meter-bar"_s);
    return webkitMeterBar;
}

const AtomString& webkitMeterInnerElement()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterInnerElement("-webkit-meter-inner-element"_s);
    return webkitMeterInnerElement;
}

const AtomString& webkitMeterOptimumValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterOptimumValue("-webkit-meter-optimum-value"_s);
    return webkitMeterOptimumValue;
}

const AtomString& webkitMeterSuboptimumValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterSuboptimumValue("-webkit-meter-suboptimum-value"_s);
    return webkitMeterSuboptimumValue;
}

const AtomString& webkitMeterEvenLessGoodValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterEvenLessGoodValue("-webkit-meter-even-less-good-value"_s);
    return webkitMeterEvenLessGoodValue;
}

const AtomString& webkitPluginReplacement()
{
    static MainThreadNeverDestroyed<const AtomString> webkitPluginReplacement("-webkit-plugin-replacement"_s);
    return webkitPluginReplacement;
}

const AtomString& webkitProgressBar()
{
    static MainThreadNeverDestroyed<const AtomString> webkitProgressBar("-webkit-progress-bar"_s);
    return webkitProgressBar;
}

const AtomString& webkitProgressValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitProgressValue("-webkit-progress-value"_s);
    return webkitProgressValue;
}

const AtomString& webkitProgressInnerElement()
{
    static MainThreadNeverDestroyed<const AtomString> webkitProgressInnerElement("-webkit-progress-inner-element"_s);
    return webkitProgressInnerElement;
}

const AtomString& webkitSearchDecoration()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchDecoration("-webkit-search-decoration"_s);
    return webkitSearchDecoration;
}

const AtomString& webkitSearchResultsButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchResultsButton("-webkit-search-results-button"_s);
    return webkitSearchResultsButton;
}

const AtomString& webkitSearchResultsDecoration()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchResultsDecoration("-webkit-search-results-decoration"_s);
    return webkitSearchResultsDecoration;
}

const AtomString& webkitSearchCancelButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchCancelButton("-webkit-search-cancel-button"_s);
    return webkitSearchCancelButton;
}

const AtomString& webkitSliderRunnableTrack()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSliderRunnableTrack("-webkit-slider-runnable-track"_s);
    return webkitSliderRunnableTrack;
}

const AtomString& webkitSliderThumb()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSliderThumb("-webkit-slider-thumb"_s);
    return webkitSliderThumb;
}

const AtomString& webkitSliderContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSliderContainer("-webkit-slider-container"_s);
    return webkitSliderContainer;
}

const AtomString& webkitTextfieldDecorationContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitTextfieldDecorationContainer("-webkit-textfield-decoration-container"_s);
    return webkitTextfieldDecorationContainer;
}

const AtomString& webkitValidationBubble()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubble("-webkit-validation-bubble"_s);
    return webkitValidationBubble;
}

const AtomString& webkitValidationBubbleArrowClipper()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleArrowClipper("-webkit-validation-bubble-arrow-clipper"_s);
    return webkitValidationBubbleArrowClipper;
}

const AtomString& webkitValidationBubbleArrow()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleArrow("-webkit-validation-bubble-arrow"_s);
    return webkitValidationBubbleArrow;
}

const AtomString& webkitValidationBubbleMessage()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleMessage("-webkit-validation-bubble-message"_s);
    return webkitValidationBubbleMessage;
}

const AtomString& webkitValidationBubbleIcon()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleIcon("-webkit-validation-bubble-icon"_s);
    return webkitValidationBubbleIcon;
}

const AtomString& webkitValidationBubbleTextBlock()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleTextBlock("-webkit-validation-bubble-text-block"_s);
    return webkitValidationBubbleTextBlock;
}

const AtomString& webkitValidationBubbleHeading()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleHeading("-webkit-validation-bubble-heading"_s);
    return webkitValidationBubbleHeading;
}

const AtomString& webkitValidationBubbleBody()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleBody("-webkit-validation-bubble-body"_s);
    return webkitValidationBubbleBody;
}

const AtomString& appleAttachmentControlsContainer()
{
    static MainThreadNeverDestroyed<const AtomString> appleAttachmentControlsContainer("-apple-attachment-controls-container"_s);
    return appleAttachmentControlsContainer;
}

} // namespace ShadowPseudoId

} // namespace WebCore
