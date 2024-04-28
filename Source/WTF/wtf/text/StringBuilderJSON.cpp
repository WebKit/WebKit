/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>. All rights reserved.
 * Copyright (C) 2017 Mozilla Foundation. All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"
#include <wtf/text/StringBuilder.h>

#include <wtf/text/EscapedFormsForJSON.h>
#include <wtf/text/StringBuilderInternals.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template<typename OutputCharacterType, typename InputCharacterType>
ALWAYS_INLINE static void appendQuotedJSONStringInternal(OutputCharacterType*& output, std::span<const InputCharacterType> input)
{
    for (; !input.empty(); input = input.subspan(1)) {
        auto character = input.front();
        if (LIKELY(character <= 0xFF)) {
            auto escaped = escapedFormsForJSON[character];
            if (LIKELY(!escaped)) {
                *output++ = character;
                continue;
            }

            *output++ = '\\';
            *output++ = escaped;
            if (UNLIKELY(escaped == 'u')) {
                *output++ = '0';
                *output++ = '0';
                *output++ = upperNibbleToLowercaseASCIIHexDigit(character);
                *output++ = lowerNibbleToLowercaseASCIIHexDigit(character);
            }
            continue;
        }

        if (LIKELY(!U16_IS_SURROGATE(character))) {
            *output++ = character;
            continue;
        }

        if (input.size() > 1) {
            auto next = input[1];
            bool isValidSurrogatePair = U16_IS_SURROGATE_LEAD(character) && U16_IS_TRAIL(next);
            if (isValidSurrogatePair) {
                *output++ = character;
                *output++ = next;
                input = input.subspan(1);
                continue;
            }
        }

        uint8_t upper = static_cast<uint32_t>(character) >> 8;
        uint8_t lower = static_cast<uint8_t>(character);
        *output++ = '\\';
        *output++ = 'u';
        *output++ = upperNibbleToLowercaseASCIIHexDigit(upper);
        *output++ = lowerNibbleToLowercaseASCIIHexDigit(upper);
        *output++ = upperNibbleToLowercaseASCIIHexDigit(lower);
        *output++ = lowerNibbleToLowercaseASCIIHexDigit(lower);
    }
}

void StringBuilder::appendQuotedJSONString(const String& string)
{
    if (hasOverflowed())
        return;

    // Make sure we have enough buffer space to append this string for worst case without reallocating.
    // The 2 is for the '"' quotes on each end.
    // The 6 is the worst case for a single code unit that could be encoded as \uNNNN.
    CheckedUint32 stringLength = string.length();
    stringLength *= 6;
    stringLength += 2;
    if (stringLength.hasOverflowed()) {
        didOverflow();
        return;
    }

    auto stringLengthValue = stringLength.value();

    if (is8Bit() && string.is8Bit()) {
        if (auto* output = extendBufferForAppending<LChar>(saturatedSum<uint32_t>(m_length, stringLengthValue))) {
            auto* end = output + stringLengthValue;
            *output++ = '"';
            appendQuotedJSONStringInternal(output, string.span8());
            *output++ = '"';
            if (output < end)
                shrink(m_length - (end - output));
        }
    } else {
        if (auto* output = extendBufferForAppendingWithUpconvert(saturatedSum<uint32_t>(m_length, stringLengthValue))) {
            auto* end = output + stringLengthValue;
            *output++ = '"';
            if (string.is8Bit())
                appendQuotedJSONStringInternal(output, string.span8());
            else
                appendQuotedJSONStringInternal(output, string.span16());
            *output++ = '"';
            if (output < end)
                shrink(m_length - (end - output));
        }
    }
}

} // namespace WTF
