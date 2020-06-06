/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
#include "Autocapitalize.h"

#if ENABLE(AUTOCAPITALIZE)

#include <wtf/NeverDestroyed.h>

namespace WebCore {

AutocapitalizeType autocapitalizeTypeForAttributeValue(const AtomString& attributeValue)
{
    // Omitted / missing values are the Default state.
    if (attributeValue.isEmpty())
        return AutocapitalizeType::Default;

    if (equalLettersIgnoringASCIICase(attributeValue, "on") || equalLettersIgnoringASCIICase(attributeValue, "sentences"))
        return AutocapitalizeType::Sentences;
    if (equalLettersIgnoringASCIICase(attributeValue, "off") || equalLettersIgnoringASCIICase(attributeValue, "none"))
        return AutocapitalizeType::None;
    if (equalLettersIgnoringASCIICase(attributeValue, "words"))
        return AutocapitalizeType::Words;
    if (equalLettersIgnoringASCIICase(attributeValue, "characters"))
        return AutocapitalizeType::AllCharacters;

    // Unrecognized values fall back to "on".
    return AutocapitalizeType::Sentences;
}

const AtomString& stringForAutocapitalizeType(AutocapitalizeType type)
{
    switch (type) {
    case AutocapitalizeType::Default:
        return nullAtom();
    case AutocapitalizeType::None: {
        static MainThreadNeverDestroyed<const AtomString> valueNone("none", AtomString::ConstructFromLiteral);
        return valueNone;
    }
    case AutocapitalizeType::Sentences: {
        static MainThreadNeverDestroyed<const AtomString> valueSentences("sentences", AtomString::ConstructFromLiteral);
        return valueSentences;
    }
    case AutocapitalizeType::Words: {
        static MainThreadNeverDestroyed<const AtomString> valueWords("words", AtomString::ConstructFromLiteral);
        return valueWords;
    }
    case AutocapitalizeType::AllCharacters: {
        static MainThreadNeverDestroyed<const AtomString> valueAllCharacters("characters", AtomString::ConstructFromLiteral);
        return valueAllCharacters;
    }
    }

    ASSERT_NOT_REACHED();
    return nullAtom();
}

} // namespace WebCore

#endif // ENABLE(AUTOCAPITALIZE)
