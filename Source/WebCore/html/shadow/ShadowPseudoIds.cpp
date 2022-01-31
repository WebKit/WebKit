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
    static MainThreadNeverDestroyed<const AtomString> cue("cue", AtomString::ConstructFromLiteral);
    return cue;
}

const AtomString& fileSelectorButton()
{
    static MainThreadNeverDestroyed<const AtomString> fileSelectorButton("file-selector-button", AtomString::ConstructFromLiteral);
    return fileSelectorButton;
}

const AtomString& placeholder()
{
    static MainThreadNeverDestroyed<const AtomString> placeholder("placeholder", AtomString::ConstructFromLiteral);
    return placeholder;
}

const AtomString& webkitContactsAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitContactsAutoFillButton("-webkit-contacts-auto-fill-button", AtomString::ConstructFromLiteral);
    return webkitContactsAutoFillButton;
}

const AtomString& webkitCredentialsAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitCredentialsAutoFillButton("-webkit-credentials-auto-fill-button", AtomString::ConstructFromLiteral);
    return webkitCredentialsAutoFillButton;
}

const AtomString& webkitCreditCardAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitCreditCardAutoFillButton("-webkit-credit-card-auto-fill-button", AtomString::ConstructFromLiteral);
    return webkitCreditCardAutoFillButton;
}

const AtomString& webkitStrongPasswordAutoFillButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitStrongPasswordAutoFillButton("-webkit-strong-password-auto-fill-button", AtomString::ConstructFromLiteral);
    return webkitStrongPasswordAutoFillButton;
}

const AtomString& webkitCapsLockIndicator()
{
    static MainThreadNeverDestroyed<const AtomString> webkitCapsLockIndicator("-webkit-caps-lock-indicator", AtomString::ConstructFromLiteral);
    return webkitCapsLockIndicator;
}

const AtomString& webkitColorSwatch()
{
    static MainThreadNeverDestroyed<const AtomString> webkitColorSwatch("-webkit-color-swatch", AtomString::ConstructFromLiteral);
    return webkitColorSwatch;
}

const AtomString& webkitColorSwatchWrapper()
{
    static MainThreadNeverDestroyed<const AtomString> webkitColorSwatchWrapper("-webkit-color-swatch-wrapper", AtomString::ConstructFromLiteral);
    return webkitColorSwatchWrapper;
}

const AtomString& webkitDatetimeEdit()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDatetimeEdit("-webkit-datetime-edit", AtomString::ConstructFromLiteral);
    return webkitDatetimeEdit;
}

const AtomString& webkitDatetimeEditText()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDatetimeEditText("-webkit-datetime-edit-text", AtomString::ConstructFromLiteral);
    return webkitDatetimeEditText;
}

const AtomString& webkitDatetimeEditFieldsWrapper()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDatetimeEditFieldsWrapper("-webkit-datetime-edit-fields-wrapper", AtomString::ConstructFromLiteral);
    return webkitDatetimeEditFieldsWrapper;
}

const AtomString& webkitDateAndTimeValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDateAndTimeValue("-webkit-date-and-time-value", AtomString::ConstructFromLiteral);
    return webkitDateAndTimeValue;
}

const AtomString& webkitDetailsMarker()
{
    static MainThreadNeverDestroyed<const AtomString> webkitDetailsMarker("-webkit-details-marker", AtomString::ConstructFromLiteral);
    return webkitDetailsMarker;
}

const AtomString& webkitGenericCueRoot()
{
    static MainThreadNeverDestroyed<const AtomString> webkitGenericCueRoot("-webkit-generic-cue-root", AtomString::ConstructFromLiteral);
    return webkitGenericCueRoot;
}

const AtomString& webkitInnerSpinButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitInnerSpinButton("-webkit-inner-spin-button", AtomString::ConstructFromLiteral);
    return webkitInnerSpinButton;
}

const AtomString& webkitListButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitListButton("-webkit-list-button", AtomString::ConstructFromLiteral);
    return webkitListButton;
}

const AtomString& webkitMediaSliderThumb()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaSliderThumb("-webkit-media-slider-thumb", AtomString::ConstructFromLiteral);
    return webkitMediaSliderThumb;
}

const AtomString& webkitMediaSliderContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaSliderContainer("-webkit-media-slider-container", AtomString::ConstructFromLiteral);
    return webkitMediaSliderContainer;
}

const AtomString& webkitMediaTextTrackContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackContainer("-webkit-media-text-track-container", AtomString::ConstructFromLiteral);
    return webkitMediaTextTrackContainer;
}

const AtomString& webkitMediaTextTrackDisplay()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackDisplay("-webkit-media-text-track-display", AtomString::ConstructFromLiteral);
    return webkitMediaTextTrackDisplay;
}

const AtomString& webkitMediaTextTrackDisplayBackdrop()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackDisplay("-webkit-media-text-track-display-backdrop", AtomString::ConstructFromLiteral);
    return webkitMediaTextTrackDisplay;
}

const AtomString& webkitMediaTextTrackRegion()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackRegion("-webkit-media-text-track-region", AtomString::ConstructFromLiteral);
    return webkitMediaTextTrackRegion;
}

const AtomString& webkitMediaTextTrackRegionContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMediaTextTrackRegionContainer("-webkit-media-text-track-region-container", AtomString::ConstructFromLiteral);
    return webkitMediaTextTrackRegionContainer;
}

const AtomString& webkitMeterBar()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterBar("-webkit-meter-bar", AtomString::ConstructFromLiteral);
    return webkitMeterBar;
}

const AtomString& webkitMeterInnerElement()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterInnerElement("-webkit-meter-inner-element", AtomString::ConstructFromLiteral);
    return webkitMeterInnerElement;
}

const AtomString& webkitMeterOptimumValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterOptimumValue("-webkit-meter-optimum-value", AtomString::ConstructFromLiteral);
    return webkitMeterOptimumValue;
}

const AtomString& webkitMeterSuboptimumValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterSuboptimumValue("-webkit-meter-suboptimum-value", AtomString::ConstructFromLiteral);
    return webkitMeterSuboptimumValue;
}

const AtomString& webkitMeterEvenLessGoodValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitMeterEvenLessGoodValue("-webkit-meter-even-less-good-value", AtomString::ConstructFromLiteral);
    return webkitMeterEvenLessGoodValue;
}

const AtomString& webkitProgressBar()
{
    static MainThreadNeverDestroyed<const AtomString> webkitProgressBar("-webkit-progress-bar", AtomString::ConstructFromLiteral);
    return webkitProgressBar;
}

const AtomString& webkitProgressValue()
{
    static MainThreadNeverDestroyed<const AtomString> webkitProgressValue("-webkit-progress-value", AtomString::ConstructFromLiteral);
    return webkitProgressValue;
}

const AtomString& webkitProgressInnerElement()
{
    static MainThreadNeverDestroyed<const AtomString> webkitProgressInnerElement("-webkit-progress-inner-element", AtomString::ConstructFromLiteral);
    return webkitProgressInnerElement;
}

const AtomString& webkitSearchDecoration()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchDecoration("-webkit-search-decoration", AtomString::ConstructFromLiteral);
    return webkitSearchDecoration;
}

const AtomString& webkitSearchResultsButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchResultsButton("-webkit-search-results-button", AtomString::ConstructFromLiteral);
    return webkitSearchResultsButton;
}

const AtomString& webkitSearchResultsDecoration()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchResultsDecoration("-webkit-search-results-decoration", AtomString::ConstructFromLiteral);
    return webkitSearchResultsDecoration;
}

const AtomString& webkitSearchCancelButton()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSearchCancelButton("-webkit-search-cancel-button", AtomString::ConstructFromLiteral);
    return webkitSearchCancelButton;
}

const AtomString& webkitSliderRunnableTrack()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSliderRunnableTrack("-webkit-slider-runnable-track", AtomString::ConstructFromLiteral);
    return webkitSliderRunnableTrack;
}

const AtomString& webkitSliderThumb()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSliderThumb("-webkit-slider-thumb", AtomString::ConstructFromLiteral);
    return webkitSliderThumb;
}

const AtomString& webkitSliderContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitSliderContainer("-webkit-slider-container", AtomString::ConstructFromLiteral);
    return webkitSliderContainer;
}

const AtomString& webkitTextfieldDecorationContainer()
{
    static MainThreadNeverDestroyed<const AtomString> webkitTextfieldDecorationContainer("-webkit-textfield-decoration-container", AtomString::ConstructFromLiteral);
    return webkitTextfieldDecorationContainer;
}

const AtomString& webkitValidationBubble()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubble("-webkit-validation-bubble", AtomString::ConstructFromLiteral);
    return webkitValidationBubble;
}

const AtomString& webkitValidationBubbleArrowClipper()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleArrowClipper("-webkit-validation-bubble-arrow-clipper", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleArrowClipper;
}

const AtomString& webkitValidationBubbleArrow()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleArrow("-webkit-validation-bubble-arrow", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleArrow;
}

const AtomString& webkitValidationBubbleMessage()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleMessage("-webkit-validation-bubble-message", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleMessage;
}

const AtomString& webkitValidationBubbleIcon()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleIcon("-webkit-validation-bubble-icon", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleIcon;
}

const AtomString& webkitValidationBubbleTextBlock()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleTextBlock("-webkit-validation-bubble-text-block", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleTextBlock;
}

const AtomString& webkitValidationBubbleHeading()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleHeading("-webkit-validation-bubble-heading", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleHeading;
}

const AtomString& webkitValidationBubbleBody()
{
    static MainThreadNeverDestroyed<const AtomString> webkitValidationBubbleBody("-webkit-validation-bubble-body", AtomString::ConstructFromLiteral);
    return webkitValidationBubbleBody;
}

} // namespace ShadowPseudoId

} // namespace WebCore
