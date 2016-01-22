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

#include <wtf/NeverDestroyed.h>

namespace WebCore {

WebAutocapitalizeType autocapitalizeTypeForAttributeValue(const AtomicString& attributeValue)
{
    // Omitted / missing values are the Default state.
    if (attributeValue.isEmpty())
        return WebAutocapitalizeTypeDefault;

    if (equalLettersIgnoringASCIICase(attributeValue, "on") || equalLettersIgnoringASCIICase(attributeValue, "sentences"))
        return WebAutocapitalizeTypeSentences;
    if (equalLettersIgnoringASCIICase(attributeValue, "off") || equalLettersIgnoringASCIICase(attributeValue, "none"))
        return WebAutocapitalizeTypeNone;
    if (equalLettersIgnoringASCIICase(attributeValue, "words"))
        return WebAutocapitalizeTypeWords;
    if (equalLettersIgnoringASCIICase(attributeValue, "characters"))
        return WebAutocapitalizeTypeAllCharacters;

    // Unrecognized values fall back to "on".
    return WebAutocapitalizeTypeSentences;
}

const AtomicString& stringForAutocapitalizeType(WebAutocapitalizeType type)
{
    switch (type) {
    case WebAutocapitalizeTypeDefault:
        return nullAtom;
    case WebAutocapitalizeTypeNone: {
        static NeverDestroyed<const AtomicString> valueNone("none", AtomicString::ConstructFromLiteral);
        return valueNone;
    }
    case WebAutocapitalizeTypeSentences: {
        static NeverDestroyed<const AtomicString> valueSentences("sentences", AtomicString::ConstructFromLiteral);
        return valueSentences;
    }
    case WebAutocapitalizeTypeWords: {
        static NeverDestroyed<const AtomicString> valueWords("words", AtomicString::ConstructFromLiteral);
        return valueWords;
    }
    case WebAutocapitalizeTypeAllCharacters: {
        static NeverDestroyed<const AtomicString> valueAllCharacters("characters", AtomicString::ConstructFromLiteral);
        return valueAllCharacters;
    }
    }

    ASSERT_NOT_REACHED();
    return nullAtom;
}

} // namespace WebCore
