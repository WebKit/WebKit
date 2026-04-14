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

#include <wtf/SIMDHelpers.h>
#include <wtf/text/EscapedFormsForJSON.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilderInternals.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template<typename OutputCharacterType, typename InputCharacterType>
ALWAYS_INLINE static bool appendEscapedJSONStringContent(std::span<OutputCharacterType>& output, std::span<const InputCharacterType> input)
{
    while (!input.empty()) {
        size_t scalarBudget = std::numeric_limits<size_t>::max();
#if (CPU(ARM64) || CPU(X86_64)) && COMPILER(CLANG)
        if constexpr (sizeof(OutputCharacterType) >= sizeof(InputCharacterType)) {
            using UnsignedType = SameSizeUnsignedInteger<InputCharacterType>;
            constexpr size_t stride = SIMD::stride<InputCharacterType>;
            constexpr auto quoteMask = SIMD::splat<UnsignedType>('"');
            constexpr auto escapeMask = SIMD::splat<UnsignedType>('\\');
            constexpr auto controlMask = SIMD::splat<UnsignedType>(' ');
            while (input.size() >= stride) {
                auto chunk = SIMD::load(std::bit_cast<const UnsignedType*>(input.data()));
                auto mask = SIMD::bitOr(SIMD::equal(chunk, quoteMask), SIMD::equal(chunk, escapeMask), SIMD::lessThan(chunk, controlMask));
                if constexpr (sizeof(InputCharacterType) != 1) {
                    constexpr auto surrogateMask = SIMD::splat<UnsignedType>(0xf800);
                    constexpr auto surrogateCheckMask = SIMD::splat<UnsignedType>(0xd800);
                    mask = SIMD::bitOr(mask, SIMD::equal(SIMD::bitAnd(chunk, surrogateMask), surrogateCheckMask));
                }
                if (auto index = SIMD::findFirstNonZeroIndex(mask)) {
                    unsigned cleanCount = index.value();
                    for (unsigned i = 0; i < cleanCount; ++i)
                        output[i] = input[i];
                    skip(output, cleanCount);
                    skip(input, cleanCount);
                    break;
                }
                if constexpr (sizeof(OutputCharacterType) == sizeof(InputCharacterType))
                    SIMD::store(chunk, std::bit_cast<UnsignedType*>(output.data()));
                else {
                    constexpr auto zeros = SIMD::splat<UnsignedType>(0);
                    simde_vst2q_u8(std::bit_cast<uint8_t*>(output.data()), (simde_uint8x16x2_t { chunk, zeros }));
                }
                skip(input, stride);
                skip(output, stride);
            }
            if (input.empty())
                return true;
            scalarBudget = stride;
        }
#endif
        for (; scalarBudget && !input.empty(); --scalarBudget) {
            auto character = input.front();
            skip(input, 1);
            if (character <= 0xFF) [[likely]] {
                auto escaped = escapedFormsForJSON[character];
                if (!escaped) [[likely]] {
                    consume(output) = character;
                    continue;
                }

                output[0] = '\\';
                output[1] = escaped;
                skip(output, 2);
                if (escaped == 'u') [[unlikely]] {
                    output[0] = '0';
                    output[1] = '0';
                    output[2] = upperNibbleToLowercaseASCIIHexDigit(character);
                    output[3] = lowerNibbleToLowercaseASCIIHexDigit(character);
                    skip(output, 4);
                }
                continue;
            }

            // We can end up calling appendEscapedJSONStringContent if we've already proven the string has only Latin1 characters when stringifying JSONs.
            // This optimization prevents us from bailing out mid-stream just because we saw e.g. a UTF-16 substring that was actually Latin1.
            if constexpr (std::same_as<OutputCharacterType, Latin1Character>)
                return false;

            if (!U16_IS_SURROGATE(character)) [[likely]] {
                consume(output) = character;
                continue;
            }

            if (!input.empty()) {
                auto next = input.front();
                bool isValidSurrogatePair = U16_IS_SURROGATE_LEAD(character) && U16_IS_TRAIL(next);
                if (isValidSurrogatePair) {
                    output[0] = character;
                    output[1] = next;
                    skip(output, 2);
                    skip(input, 1);
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
            skip(output, 6);
        }
    }

    return true;
}

} // namespace WTF
