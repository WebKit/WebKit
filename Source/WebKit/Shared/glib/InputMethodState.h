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

#pragma once

#include <WebCore/AutocapitalizeTypes.h>
#include <WebCore/InputMode.h>
#include <wtf/OptionSet.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HTMLInputElement;
}

namespace WebKit {

enum class InputMethodStatePurpose : uint8_t {
    FreeForm,
    Digits,
    Number,
    Phone,
    Url,
    Email,
    Password
};

enum class InputMethodStateHint : uint8_t {
    None = 0,
    Spellcheck = 1 << 0,
    Lowercase = 1 << 1,
    UppercaseChars = 1 << 2,
    UppercaseWords = 1 << 3,
    UppercaseSentences = 1 << 4,
    InhibitOnScreenKeyboard = 1 << 5
};

struct InputMethodState {
    using Purpose = InputMethodStatePurpose;
    using Hint = InputMethodStateHint;

    void setPurposeOrHintForInputMode(WebCore::InputMode);
    void setPurposeForInputElement(WebCore::HTMLInputElement&);
    void addHintsForAutocapitalizeType(WebCore::AutocapitalizeType);

    void encode(IPC::Encoder&) const;
    static std::optional<InputMethodState> decode(IPC::Decoder&);

    friend bool operator==(const InputMethodState&, const InputMethodState&) = default;

    Purpose purpose { Purpose::FreeForm };
    OptionSet<Hint> hints;
};

} // namespace WebKit
