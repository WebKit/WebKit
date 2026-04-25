/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/RenderStyleConstants.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/TextStream.h>

namespace WebCore::Style {

struct PseudoElementIdentifier {
    PseudoElementType type;

    // Highlight name for ::highlight, view transition name for view transition pseudo-elements,
    // or user agent part name for UserAgentPartFallback (e.g., "file-selector-button", "picker(select)").
    AtomString nameOrPart { nullAtom() };

    friend bool operator==(const PseudoElementIdentifier& a, const PseudoElementIdentifier& b) = default;
};

inline void add(Hasher& hasher, const PseudoElementIdentifier& pseudoElementIdentifier)
{
    add(hasher, pseudoElementIdentifier.type, pseudoElementIdentifier.nameOrPart);
}

inline WTF::TextStream& operator<<(WTF::TextStream& ts, const PseudoElementIdentifier& pseudoElementIdentifier)
{
    ts << "::"_s << pseudoElementIdentifier.type;
    if (!pseudoElementIdentifier.nameOrPart.isNull())
        ts << '(' << pseudoElementIdentifier.nameOrPart << ')';
    return ts;
}

inline bool isNamedViewTransitionPseudoElement(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    if (!pseudoElementIdentifier)
        return false;

    switch (pseudoElementIdentifier->type) {
    case PseudoElementType::ViewTransitionGroup:
    case PseudoElementType::ViewTransitionImagePair:
    case PseudoElementType::ViewTransitionOld:
    case PseudoElementType::ViewTransitionNew:
        return true;
    default:
        return false;
    }
};

} // namespace WebCore

namespace WTF {

template<>
struct HashTraits<WebCore::Style::PseudoElementIdentifier> : GenericHashTraits<WebCore::Style::PseudoElementIdentifier> {
    typedef WebCore::Style::PseudoElementIdentifier EmptyValueType;

    static constexpr bool emptyValueIsZero = false;
    static EmptyValueType emptyValue() { return WebCore::Style::PseudoElementIdentifier { { }, emptyAtom() }; }

    static void constructDeletedValue(WebCore::Style::PseudoElementIdentifier& identifer) { new (NotNull, &identifer.nameOrPart) AtomString { HashTableDeletedValue }; }
    static bool isDeletedValue(const WebCore::Style::PseudoElementIdentifier& identifer) { return identifer.nameOrPart.isHashTableDeletedValue(); }
};

template<>
struct HashTraits<std::optional<WebCore::Style::PseudoElementIdentifier>> : GenericHashTraits<std::optional<WebCore::Style::PseudoElementIdentifier>> {
    typedef std::optional<WebCore::Style::PseudoElementIdentifier> EmptyValueType;

    static constexpr bool emptyValueIsZero = false;
    static EmptyValueType emptyValue() { return HashTraits<WebCore::Style::PseudoElementIdentifier>::emptyValue(); }

    static void constructDeletedValue(std::optional<WebCore::Style::PseudoElementIdentifier>& identifer)
    {
        HashTraits<WebCore::Style::PseudoElementIdentifier>::constructDeletedValue(identifer.emplace());
    }
    static bool isDeletedValue(const std::optional<WebCore::Style::PseudoElementIdentifier>& identifer)
    {
        return identifer && HashTraits<WebCore::Style::PseudoElementIdentifier>::isDeletedValue(*identifer);
    }
};

} // namespace WTF
