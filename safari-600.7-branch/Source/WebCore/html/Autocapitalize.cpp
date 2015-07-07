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

static const AtomicString& valueOn()
{
    static NeverDestroyed<const AtomicString> valueOn("on", AtomicString::ConstructFromLiteral);
    return valueOn;
}

static const AtomicString& valueOff()
{
    static NeverDestroyed<const AtomicString> valueOff("off", AtomicString::ConstructFromLiteral);
    return valueOff;
}

static const AtomicString& valueNone()
{
    static NeverDestroyed<const AtomicString> valueNone("none", AtomicString::ConstructFromLiteral);
    return valueNone;
}

static const AtomicString& valueWords()
{
    static NeverDestroyed<const AtomicString> valueWords("words", AtomicString::ConstructFromLiteral);
    return valueWords;
}

static const AtomicString& valueSentences()
{
    static NeverDestroyed<const AtomicString> valueSentences("sentences", AtomicString::ConstructFromLiteral);
    return valueSentences;
}

static const AtomicString& valueAllCharacters()
{
    static NeverDestroyed<const AtomicString> valueAllCharacters("characters", AtomicString::ConstructFromLiteral);
    return valueAllCharacters;
}

WebAutocapitalizeType autocapitalizeTypeForAttributeValue(const AtomicString& attributeValue)
{
    // Omitted / missing values are the Default state.
    if (attributeValue.isNull() || attributeValue.isEmpty())
        return WebAutocapitalizeTypeDefault;

    if (equalIgnoringCase(attributeValue, valueOn()) || equalIgnoringCase(attributeValue, valueSentences()))
        return WebAutocapitalizeTypeSentences;
    if (equalIgnoringCase(attributeValue, valueOff()) || equalIgnoringCase(attributeValue, valueNone()))
        return WebAutocapitalizeTypeNone;
    if (equalIgnoringCase(attributeValue, valueWords()))
        return WebAutocapitalizeTypeWords;
    if (equalIgnoringCase(attributeValue, valueAllCharacters()))
        return WebAutocapitalizeTypeAllCharacters;

    // Unrecognized values fall back to "on".
    return WebAutocapitalizeTypeSentences;
}

const AtomicString& stringForAutocapitalizeType(WebAutocapitalizeType type)
{
    switch (type) {
    case WebAutocapitalizeTypeDefault:
        return nullAtom;
    case WebAutocapitalizeTypeNone:
        return valueNone();
    case WebAutocapitalizeTypeSentences:
        return valueSentences();
    case WebAutocapitalizeTypeWords:
        return valueWords();
    case WebAutocapitalizeTypeAllCharacters:
        return valueAllCharacters();
    }

    ASSERT_NOT_REACHED();
    return nullAtom;
}

} // namespace WebCore
