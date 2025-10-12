/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleAppearance.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, StyleAppearance appearance)
{
    switch (appearance) {
    case StyleAppearance::None:
        ts << "none"_s;
        break;
    case StyleAppearance::Auto:
        ts << "auto"_s;
        break;
    case StyleAppearance::Base:
        ts << "base"_s;
        break;
    case StyleAppearance::Checkbox:
        ts << "checkbox"_s;
        break;
    case StyleAppearance::Radio:
        ts << "radio"_s;
        break;
    case StyleAppearance::PushButton:
        ts << "push-button"_s;
        break;
    case StyleAppearance::SquareButton:
        ts << "square-button"_s;
        break;
    case StyleAppearance::Button:
        ts << "button"_s;
        break;
    case StyleAppearance::DefaultButton:
        ts << "default-button"_s;
        break;
    case StyleAppearance::Listbox:
        ts << "listbox"_s;
        break;
    case StyleAppearance::Menulist:
        ts << "menulist"_s;
        break;
    case StyleAppearance::MenulistButton:
        ts << "menulist-button"_s;
        break;
    case StyleAppearance::Meter:
        ts << "meter"_s;
        break;
    case StyleAppearance::ProgressBar:
        ts << "progress-bar"_s;
        break;
    case StyleAppearance::SliderHorizontal:
        ts << "slider-horizontal"_s;
        break;
    case StyleAppearance::SliderVertical:
        ts << "slider-vertical"_s;
        break;
    case StyleAppearance::SearchField:
        ts << "searchfield"_s;
        break;
#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        ts << "apple-pay-button"_s;
        break;
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case StyleAppearance::Attachment:
        ts << "attachment"_s;
        break;
    case StyleAppearance::BorderlessAttachment:
        ts << "borderless-attachment"_s;
        break;
#endif
    case StyleAppearance::TextArea:
        ts << "textarea"_s;
        break;
    case StyleAppearance::TextField:
        ts << "textfield"_s;
        break;
    case StyleAppearance::ColorWell:
        ts << "color-well"_s;
        break;
    case StyleAppearance::ColorWellSwatch:
        ts << "color-well-swatch"_s;
        break;
    case StyleAppearance::ColorWellSwatchOverlay:
        ts << "color-well-swatch-overlay"_s;
        break;
    case StyleAppearance::ColorWellSwatchWrapper:
        ts << "color-well-swatch-wrapper"_s;
        break;
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        ts << "image-controls-button"_s;
        break;
#endif
    case StyleAppearance::InnerSpinButton:
        ts << "inner-spin-button"_s;
        break;
    case StyleAppearance::ListButton:
        ts << "list-button"_s;
        break;
    case StyleAppearance::SearchFieldDecoration:
        ts << "searchfield-decoration"_s;
        break;
    case StyleAppearance::SearchFieldResultsDecoration:
        ts << "searchfield-results-decoration"_s;
        break;
    case StyleAppearance::SearchFieldResultsButton:
        ts << "searchfield-results-button"_s;
        break;
    case StyleAppearance::SearchFieldCancelButton:
        ts << "searchfield-cancel-button"_s;
        break;
    case StyleAppearance::SliderThumbHorizontal:
        ts << "sliderthumb-horizontal"_s;
        break;
    case StyleAppearance::SliderThumbVertical:
        ts << "sliderthumb-vertical"_s;
        break;
    case StyleAppearance::Switch:
        ts << "switch"_s;
        break;
    case StyleAppearance::SwitchThumb:
        ts << "switch-thumb"_s;
        break;
    case StyleAppearance::SwitchTrack:
        ts << "switch-track"_s;
        break;
    }
    return ts;
}

} // namespace WebCore
