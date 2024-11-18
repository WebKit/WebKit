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

#pragma once

#include <wtf/text/EscapedFormsForJSON.h>
#include <wtf/text/StringBuilderInternals.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template<typename OutputCharacterType, typename InputCharacterType>
ALWAYS_INLINE static void appendEscapedJSONStringContent(std::span<OutputCharacterType>& output, std::span<const InputCharacterType> input)
{
    for (; !input.empty(); input = input.subspan(1)) {
        auto character = input.front();
        if (LIKELY(character <= 0xFF)) {
            auto escaped = escapedFormsForJSON[character];
            if (LIKELY(!escaped)) {
                output[0] = character;
                output = output.subspan(1);
                continue;
            }

            output[0] = '\\';
            output[1] = escaped;
            output = output.subspan(2);
            if (UNLIKELY(escaped == 'u')) {
                output[0] = '0';
                output[1] = '0';
                output[2] = upperNibbleToLowercaseASCIIHexDigit(character);
                output[3] = lowerNibbleToLowercaseASCIIHexDigit(character);
                output = output.subspan(4);
            }
            continue;
        }

        if (LIKELY(!U16_IS_SURROGATE(character))) {
            output[0] = character;
            output = output.subspan(1);
            continue;
        }

        if (input.size() > 1) {
            auto next = input[1];
            bool isValidSurrogatePair = U16_IS_SURROGATE_LEAD(character) && U16_IS_TRAIL(next);
            if (isValidSurrogatePair) {
                output[0] = character;
                output[1] = next;
                output = output.subspan(2);
                input = input.subspan(1);
                continue;
            }
        }

        uint8_t upper = static_cast<uint32_t>(character) >> 8;
        uint8_t lower = static_cast<uint8_t>(character);
        output[0] = '\\';
        output[1] = 'u';
        output[2] = upperNibbleToLowercaseASCIIHexDigit(upper);
        output[3] = lowerNibbleToLowercaseASCIIHexDigit(upper);
        output[4] = upperNibbleToLowercaseASCIIHexDigit(lower);
        output[5] = lowerNibbleToLowercaseASCIIHexDigit(lower);
        output = output.subspan(6);
    }
}

} // namespace WTF
