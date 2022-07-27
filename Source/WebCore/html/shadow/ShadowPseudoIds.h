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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

namespace ShadowPseudoIds {

const AtomString& cue();

const AtomString& fileSelectorButton();

const AtomString& placeholder();

const AtomString& webkitContactsAutoFillButton();
const AtomString& webkitCredentialsAutoFillButton();
const AtomString& webkitCreditCardAutoFillButton();
const AtomString& webkitLoadingAutoFillButton();
const AtomString& webkitStrongPasswordAutoFillButton();

const AtomString& webkitCapsLockIndicator();

const AtomString& webkitColorSwatch();
const AtomString& webkitColorSwatchWrapper();

const AtomString& webkitDatetimeEdit();
const AtomString& webkitDatetimeEditText();
const AtomString& webkitDatetimeEditFieldsWrapper();
const AtomString& webkitDateAndTimeValue();

const AtomString& webkitDetailsMarker();

const AtomString& webkitGenericCueRoot();

const AtomString& webkitInnerSpinButton();

const AtomString& webkitListButton();

const AtomString& webkitMediaTextTrackContainer();

const AtomString& webkitMediaTextTrackDisplay();
const AtomString& webkitMediaTextTrackDisplayBackdrop();

const AtomString& webkitMediaTextTrackRegion();
const AtomString& webkitMediaTextTrackRegionContainer();

const AtomString& webkitMeterBar();
const AtomString& webkitMeterInnerElement();
const AtomString& webkitMeterOptimumValue();
const AtomString& webkitMeterSuboptimumValue();
const AtomString& webkitMeterEvenLessGoodValue();

const AtomString& webkitPluginReplacement();

const AtomString& webkitProgressBar();
const AtomString& webkitProgressValue();
const AtomString& webkitProgressInnerElement();

const AtomString& webkitSearchDecoration();
const AtomString& webkitSearchResultsButton();
const AtomString& webkitSearchResultsDecoration();

const AtomString& webkitSearchCancelButton();

const AtomString& webkitSliderRunnableTrack();

const AtomString& webkitSliderThumb();
const AtomString& webkitSliderContainer();

const AtomString& webkitTextfieldDecorationContainer();

const AtomString& webkitValidationBubble();
const AtomString& webkitValidationBubbleArrowClipper();
const AtomString& webkitValidationBubbleArrow();
const AtomString& webkitValidationBubbleMessage();
const AtomString& webkitValidationBubbleIcon();
const AtomString& webkitValidationBubbleTextBlock();
const AtomString& webkitValidationBubbleHeading();
const AtomString& webkitValidationBubbleBody();

const AtomString& appleAttachmentControlsContainer();

} // namespace ShadowPseudoId

} // namespace WebCore
