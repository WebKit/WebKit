/*
 * Copyright (C) 2019-2022 Apple Inc. All Rights Reserved.
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
#include "ControlPartType.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ControlPartType type)
{
    switch (type) {
    case ControlPartType::NoControl:
        ts << "no-control-part";
        break;
    case ControlPartType::Auto:
        ts << "auto-part";
        break;
    case ControlPartType::Checkbox:
        ts << "checkbox-part";
        break;
    case ControlPartType::Radio:
        ts << "radio-part";
        break;
    case ControlPartType::PushButton:
        ts << "push-button-part";
        break;
    case ControlPartType::SquareButton:
        ts << "square-button-part";
        break;
    case ControlPartType::Button:
        ts << "button-part";
        break;
    case ControlPartType::DefaultButton:
        ts << "default-button-part";
        break;
    case ControlPartType::Listbox:
        ts << "listbox-part";
        break;
    case ControlPartType::Menulist:
        ts << "menulist-part";
        break;
    case ControlPartType::MenulistButton:
        ts << "menulist-button-part";
        break;
    case ControlPartType::Meter:
        ts << "meter-part";
        break;
    case ControlPartType::ProgressBar:
        ts << "progress-bar-part";
        break;
    case ControlPartType::SliderHorizontal:
        ts << "slider-horizontal-part";
        break;
    case ControlPartType::SliderVertical:
        ts << "slider-vertical-part";
        break;
    case ControlPartType::SearchField:
        ts << "search-field-part";
        break;
#if ENABLE(APPLE_PAY)
    case ControlPartType::ApplePayButton:
        ts << "apple-pay-button-part";
        break;
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case ControlPartType::Attachment:
        ts << "attachment-part";
        break;
    case ControlPartType::BorderlessAttachment:
        ts << "borderless-attachment-part";
        break;
#endif
    case ControlPartType::TextArea:
        ts << "text-area-part";
        break;
    case ControlPartType::TextField:
        ts << "text-field-part";
        break;
    case ControlPartType::CapsLockIndicator:
        ts << "caps-lock-indicator-part";
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
        ts << "color-well-part";
        break;
#endif
#if ENABLE(SERVICE_CONTROLS)
    case ControlPartType::ImageControlsButton:
        ts << "image-controls-button-part";
        break;
#endif
    case ControlPartType::InnerSpinButton:
        ts << "inner-spin-button-part";
        break;
#if ENABLE(DATALIST_ELEMENT)
    case ControlPartType::ListButton:
        ts << "list-button-part";
        break;
#endif
    case ControlPartType::SearchFieldDecoration:
        ts << "search-field-decoration-part";
        break;
    case ControlPartType::SearchFieldResultsDecoration:
        ts << "search-field-results-decoration-part";
        break;
    case ControlPartType::SearchFieldResultsButton:
        ts << "search-field-results-button-part";
        break;
    case ControlPartType::SearchFieldCancelButton:
        ts << "search-field-cancel-button-part";
        break;
    case ControlPartType::SliderThumbHorizontal:
        ts << "slider-thumb-horizontal-part";
        break;
    case ControlPartType::SliderThumbVertical:
        ts << "slider-thumb-vertical-part";
        break;
    }
    return ts;
}

} // namespace WebCore
