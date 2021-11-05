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

#include "config.h"
#include "ContentExtensionStringSerialization.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore::ContentExtensions {

String deserializeString(Span<const uint8_t> span)
{
    const auto* actions = span.data();
    const auto actionsLength = span.size();
    auto prefixLength = sizeof(uint32_t) + sizeof(bool);
    auto stringStartIndex = prefixLength;
    RELEASE_ASSERT(actionsLength >= stringStartIndex);
    uint32_t stringLength = *reinterpret_cast<const uint32_t*>(actions);
    bool wideCharacters = actions[sizeof(uint32_t)];

    if (wideCharacters) {
        RELEASE_ASSERT(actionsLength >= stringStartIndex + stringLength * sizeof(UChar));
        return String(reinterpret_cast<const UChar*>(&actions[stringStartIndex]), stringLength);
    }
    RELEASE_ASSERT(actionsLength >= stringStartIndex + stringLength * sizeof(LChar));
    return String(reinterpret_cast<const LChar*>(&actions[stringStartIndex]), stringLength);
}

void serializeString(Vector<uint8_t>& actions, const String& string)
{
    // Append Selector length (4 bytes).
    uint32_t stringLength = string.length();
    actions.grow(actions.size() + sizeof(uint32_t));
    *reinterpret_cast<uint32_t*>(&actions[actions.size() - sizeof(uint32_t)]) = stringLength;
    bool wideCharacters = !string.is8Bit();
    actions.append(wideCharacters);
    // Append Selector.
    if (wideCharacters) {
        uint32_t startIndex = actions.size();
        actions.grow(actions.size() + sizeof(UChar) * stringLength);
        for (uint32_t i = 0; i < stringLength; ++i)
            *reinterpret_cast<UChar*>(&actions[startIndex + i * sizeof(UChar)]) = string[i];
    } else {
        for (uint32_t i = 0; i < stringLength; ++i)
            actions.append(string[i]);
    }
}

size_t stringSerializedLength(Span<const uint8_t> span)
{
    constexpr auto prefixLength = sizeof(uint32_t) + sizeof(bool);
    RELEASE_ASSERT(span.size() >= prefixLength);
    auto stringLength = *reinterpret_cast<const uint32_t*>(span.data());
    bool wideCharacters = span[sizeof(uint32_t)];
    if (wideCharacters)
        return prefixLength + stringLength * sizeof(UChar);
    return prefixLength + stringLength * sizeof(LChar);
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
