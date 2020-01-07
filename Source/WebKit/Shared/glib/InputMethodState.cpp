/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "InputMethodState.h"

#include "ArgumentCoders.h"
#include <WebCore/HTMLInputElement.h>

namespace WebKit {

void InputMethodState::setPurposeOrHintForInputMode(WebCore::InputMode inputMode)
{
    switch (inputMode) {
    case WebCore::InputMode::None:
        hints.add(InputMethodState::Hint::InhibitOnScreenKeyboard);
        break;
    case WebCore::InputMode::Unspecified:
    case WebCore::InputMode::Text:
        purpose = Purpose::FreeForm;
        break;
    case WebCore::InputMode::Telephone:
        purpose = Purpose::Phone;
        break;
    case WebCore::InputMode::Url:
        purpose = Purpose::Url;
        break;
    case WebCore::InputMode::Email:
        purpose = Purpose::Email;
        break;
    case WebCore::InputMode::Numeric:
        purpose = Purpose::Digits;
        break;
    case WebCore::InputMode::Decimal:
        purpose = Purpose::Number;
        break;
    case WebCore::InputMode::Search:
        break;
    }
}

static bool inputElementHasDigitsPattern(WebCore::HTMLInputElement& element)
{
    const auto& pattern = element.attributeWithoutSynchronization(WebCore::HTMLNames::patternAttr);
    return pattern == "\\d*" || pattern == "[0-9]*";
}

void InputMethodState::setPurposeForInputElement(WebCore::HTMLInputElement& element)
{
    if (element.isPasswordField())
        purpose = Purpose::Password;
    else if (element.isEmailField())
        purpose = Purpose::Email;
    else if (element.isTelephoneField())
        purpose = Purpose::Phone;
    else if (element.isNumberField())
        purpose = inputElementHasDigitsPattern(element) ? Purpose::Digits : Purpose::Number;
    else if (element.isURLField())
        purpose = Purpose::Url;
    else if (element.isText() && inputElementHasDigitsPattern(element))
        purpose = Purpose::Digits;
}

void InputMethodState::addHintsForAutocapitalizeType(AutocapitalizeType autocapitalizeType)
{
    switch (autocapitalizeType) {
    case AutocapitalizeTypeDefault:
        break;
    case AutocapitalizeTypeNone:
        hints.add(InputMethodState::Hint::Lowercase);
        break;
    case AutocapitalizeTypeWords:
        hints.add(InputMethodState::Hint::UppercaseWords);
        break;
    case AutocapitalizeTypeSentences:
        hints.add(InputMethodState::Hint::UppercaseSentences);
        break;
    case AutocapitalizeTypeAllCharacters:
        hints.add(InputMethodState::Hint::UppercaseChars);
        break;
    }
}

void InputMethodState::encode(IPC::Encoder& encoder) const
{
    encoder.encodeEnum(purpose);
    encoder << hints;
}

Optional<InputMethodState> InputMethodState::decode(IPC::Decoder& decoder)
{
    InputMethodState state;
    if (!decoder.decodeEnum(state.purpose))
        return WTF::nullopt;
    if (!decoder.decode(state.hints))
        return WTF::nullopt;
    return state;
}

} // namespace WebKit
