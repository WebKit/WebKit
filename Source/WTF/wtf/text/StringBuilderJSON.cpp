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

#include <wtf/text/StringBuilderInternals.h>
#include <wtf/text/WTFString.h>

namespace WTF {

// This table driven escaping is ported from SpiderMonkey.
static const constexpr LChar escapedFormsForJSON[0x100] = {
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    'b', 't', 'n', 'u', 'f', 'r', 'u', 'u',
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    0,   0,  '\"', 0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,  '\\', 0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
};

template<typename OutputCharacterType, typename InputCharacterType>
ALWAYS_INLINE static void appendQuotedJSONStringInternal(OutputCharacterType*& output, const InputCharacterType* input, unsigned length)
{
    for (auto* end = input + length; input != end; ++input) {
        auto character = *input;
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

        auto next = input + 1;
        bool isValidSurrogatePair = U16_IS_SURROGATE_LEAD(character) && next != end && U16_IS_TRAIL(*next);
        if (isValidSurrogatePair) {
            *output++ = character;
            *output++ = *next;
            ++input;
            continue;
        }

        uint8_t upper = static_cast<uint32_t>(character) >> 8;
        uint8_t lower = static_cast<uint8_t>(character);
        *output++ = '\\';
        *output++ = 'u';
        *output++ = upperNibbleToLowercaseASCIIHexDigit(upper);
        *output++ = lowerNibbleToLowercaseASCIIHexDigit(upper);
        *output++ = upperNibbleToLowercaseASCIIHexDigit(lower);
        *output++ = lowerNibbleToLowercaseASCIIHexDigit(lower);
        continue;
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
    auto maximumCapacityRequired = m_length + 2 + stringLength * 6;
    if (maximumCapacityRequired.hasOverflowed()) {
        didOverflow();
        return;
    }

    // We need to check maximum length before calling roundUpPowerOfTwo because that function returns 0 for values in the range [2^31, 2^32-2].
    // FIXME: Instead, roundUpToPowerOfTwo should be fixed to do something more useful in those cases, perhaps using checked or saturated arithmetic.
    // https://bugs.webkit.org/show_bug.cgi?id=176086
    auto allocationSize = maximumCapacityRequired.value();
    if (allocationSize > String::MaxLength) {
        didOverflow();
        return;
    }
    allocationSize = roundUpToPowerOfTwo(allocationSize);
    if (allocationSize > String::MaxLength) {
        didOverflow();
        return;
    }

    // FIXME: Consider switching to extendBufferForAppending/shrink instead to share more code with the rest of StringBuilder.
    if (is8Bit() && !string.is8Bit())
        allocateBuffer<UChar>(characters<LChar>(), allocationSize);
    else
        reserveCapacity(allocationSize);
    if (UNLIKELY(hasOverflowed()))
        return;

    if (m_buffer->is8Bit()) {
        auto characters = const_cast<LChar*>(m_buffer->characters<LChar>());
        auto output = characters + m_length;
        *output++ = '"';
        appendQuotedJSONStringInternal(output, string.characters8(), string.length());
        *output++ = '"';
        m_length = output - characters;
    } else {
        auto characters = const_cast<UChar*>(m_buffer->characters<UChar>());
        auto output = characters + m_length;
        *output++ = '"';
        if (string.is8Bit())
            appendQuotedJSONStringInternal(output, string.characters8(), string.length());
        else
            appendQuotedJSONStringInternal(output, string.characters16(), string.length());
        *output++ = '"';
        m_length = output - characters;
    }
    ASSERT(m_buffer->length() >= m_length);
}

} // namespace WTF
