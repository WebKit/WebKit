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
#include <wtf/EnumTraits.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HTMLInputElement;
}

namespace WebKit {

struct InputMethodState {
    enum class Purpose {
        FreeForm,
        Digits,
        Number,
        Phone,
        Url,
        Email,
        Password
    };

    enum class Hint {
        None = 0,
        Spellcheck = 1 << 0,
        Lowercase = 1 << 1,
        UppercaseChars = 1 << 2,
        UppercaseWords = 1 << 3,
        UppercaseSentences = 1 << 4,
        InhibitOnScreenKeyboard = 1 << 5
    };

    void setPurposeOrHintForInputMode(WebCore::InputMode);
    void setPurposeForInputElement(WebCore::HTMLInputElement&);
    void addHintsForAutocapitalizeType(WebCore::AutocapitalizeType);

    void encode(IPC::Encoder&) const;
    static Optional<InputMethodState> decode(IPC::Decoder&);

    Purpose purpose { Purpose::FreeForm };
    OptionSet<Hint> hints;
};

inline bool operator==(const InputMethodState& a, const InputMethodState& b)
{
    return a.purpose == b.purpose && a.hints == b.hints;
}

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::InputMethodState::Hint> {
    using values = EnumValues<
        WebKit::InputMethodState::Hint,
        WebKit::InputMethodState::Hint::None,
        WebKit::InputMethodState::Hint::Spellcheck,
        WebKit::InputMethodState::Hint::Lowercase,
        WebKit::InputMethodState::Hint::UppercaseChars,
        WebKit::InputMethodState::Hint::UppercaseWords,
        WebKit::InputMethodState::Hint::UppercaseSentences,
        WebKit::InputMethodState::Hint::InhibitOnScreenKeyboard
    >;
};

template<> struct EnumTraits<WebKit::InputMethodState::Purpose> {
    using values = EnumValues<
        WebKit::InputMethodState::Purpose,
        WebKit::InputMethodState::Purpose::FreeForm,
        WebKit::InputMethodState::Purpose::Digits,
        WebKit::InputMethodState::Purpose::Number,
        WebKit::InputMethodState::Purpose::Phone,
        WebKit::InputMethodState::Purpose::Url,
        WebKit::InputMethodState::Purpose::Email,
        WebKit::InputMethodState::Purpose::Password
    >;
};

} // namespace WTF
