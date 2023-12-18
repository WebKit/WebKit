/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * License header from dragonbox
 *    https://github.com/jk-jeon/dragonbox/blob/master/LICENSE-Boost
 *    https://github.com/jk-jeon/dragonbox/blob/master/LICENSE-Apache2-LLVM
 */

#include "config.h"
#include <wtf/dragonbox/dragonbox_to_chars.h>

namespace WTF {

namespace dragonbox {

static constexpr char radix_100_table[] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', //
    '0', '5', '0', '6', '0', '7', '0', '8', '0', '9', //
    '1', '0', '1', '1', '1', '2', '1', '3', '1', '4', //
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', //
    '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', //
    '2', '5', '2', '6', '2', '7', '2', '8', '2', '9', //
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', //
    '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', //
    '4', '0', '4', '1', '4', '2', '4', '3', '4', '4', //
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', //
    '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', //
    '5', '5', '5', '6', '5', '7', '5', '8', '5', '9', //
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', //
    '6', '5', '6', '6', '6', '7', '6', '8', '6', '9', //
    '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', //
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', //
    '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', //
    '8', '5', '8', '6', '8', '7', '8', '8', '8', '9', //
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', //
    '9', '5', '9', '6', '9', '7', '9', '8', '9', '9' //
};

static constexpr char radix_100_head_table[] = {
    '0', '.', '1', '.', '2', '.', '3', '.', '4', '.', //
    '5', '.', '6', '.', '7', '.', '8', '.', '9', '.', //
    '1', '.', '1', '.', '1', '.', '1', '.', '1', '.', //
    '1', '.', '1', '.', '1', '.', '1', '.', '1', '.', //
    '2', '.', '2', '.', '2', '.', '2', '.', '2', '.', //
    '2', '.', '2', '.', '2', '.', '2', '.', '2', '.', //
    '3', '.', '3', '.', '3', '.', '3', '.', '3', '.', //
    '3', '.', '3', '.', '3', '.', '3', '.', '3', '.', //
    '4', '.', '4', '.', '4', '.', '4', '.', '4', '.', //
    '4', '.', '4', '.', '4', '.', '4', '.', '4', '.', //
    '5', '.', '5', '.', '5', '.', '5', '.', '5', '.', //
    '5', '.', '5', '.', '5', '.', '5', '.', '5', '.', //
    '6', '.', '6', '.', '6', '.', '6', '.', '6', '.', //
    '6', '.', '6', '.', '6', '.', '6', '.', '6', '.', //
    '7', '.', '7', '.', '7', '.', '7', '.', '7', '.', //
    '7', '.', '7', '.', '7', '.', '7', '.', '7', '.', //
    '8', '.', '8', '.', '8', '.', '8', '.', '8', '.', //
    '8', '.', '8', '.', '8', '.', '8', '.', '8', '.', //
    '9', '.', '9', '.', '9', '.', '9', '.', '9', '.', //
    '9', '.', '9', '.', '9', '.', '9', '.', '9', '.' //
};

ALWAYS_INLINE void print_1_digit(uint32_t n, char* buffer) noexcept
{
    static_assert(!('0' & 0xf));
    *buffer = static_cast<char>('0' | n);
}

ALWAYS_INLINE void print_2_digits(uint32_t n, char* buffer) noexcept
{
    memcpy(buffer, radix_100_table + n * 2, 2);
}

// These digit generation routines are inspired by James Anhalt's itoa algorithm:
// https://github.com/jeaiii/itoa
// The main idea is for given n, find y such that floor(10^k * y / 2^32) = n holds,
// where k is an appropriate integer depending on the length of n.
// For example, if n = 1234567, we set k = 6. In this case, we have
// floor(y / 2^32) = 1,
// floor(10^2 * ((10^0 * y) mod 2^32) / 2^32) = 23,
// floor(10^2 * ((10^2 * y) mod 2^32) / 2^32) = 45, and
// floor(10^2 * ((10^4 * y) mod 2^32) / 2^32) = 67.
// See https://jk-jeon.github.io/posts/2022/02/jeaiii-algorithm/ for more explanation.
// Note that this fuction ignores trailing zeros.

template <Mode mode>
ALWAYS_INLINE void print_9_digits(uint32_t s32, int32_t& exponent, char*& buffer) noexcept
{
    ASSERT(s32);

    // If ToExponential mode then print "d." else just print "d".
    constexpr uint32_t first_head_digit_chars_count = static_cast<uint32_t>(mode);

    // -- IEEE-754 binary32
    // Since we do not cut trailing zeros in advance, s32 must be of 6~9 digits
    // unless the original input was subnormal.
    // In particular, when it is of 9 digits it shouldn't have any trailing zeros.
    // -- IEEE-754 binary64
    // In this case, s32 must be of 7~9 digits unless the input is subnormal,
    // and it shouldn't have any trailing zeros if it is of 9 digits.
    if (s32 >= 1'0000'0000) {
        // 9 digits.
        // 1441151882 = ceil(2^57 / 1'0000'0000) + 1
        auto prod = s32 * static_cast<uint64_t>(1441151882);
        prod >>= 25;
        memcpy(buffer, radix_100_head_table + static_cast<uint32_t>(prod >> 32) * 2, first_head_digit_chars_count);

        prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
        print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count);
        prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
        print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 2);
        prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
        print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 4);
        prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
        print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 6);

        exponent += 8;
        buffer += 8 + first_head_digit_chars_count;
    } else if (s32 >= 100'0000) {
        // 7 or 8 digits which consist of the head digits (one or two digits) and the remaining 6 digits (d1, d2, d3, d4, d5, d6).

        // 281474978 = ceil(2^48 / 100'0000) + 1
        auto prod = s32 * static_cast<uint64_t>(281474978);
        prod >>= 16;
        auto const head_digits = static_cast<uint32_t>(prod >> 32);
        // Has 2nd head digit also means it's of 8 digits.
        uint32_t has_second_head_digit = static_cast<uint32_t>(head_digits >= 10);

        // If s32 is of 8 digits, increase the exponent by 7. Otherwise, increase it by 6.
        exponent += 6 + has_second_head_digit;

        uint32_t first_head_digit_index = head_digits * 2;
        // Write the first head digit and the decimal point if needed.
        memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
        // Write the second head digit. This character may be overwritten later but we don't care.
        buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

        if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 100'0000)) {
            // The remaining 6 digits (d1, d2, d3, d4, d5, d6) are all zero. Then, the number of characters actually need to be written is:
            //   1. Only the first digit is nonzero, which means that either s32 is of 7 digits or it is of 8 digits but the second digit is zero.
            //      Then, we only need the first digit in the buffer.
            //   2. Otherwise, we need first_head_digit_chars_count + 1 digits in the buffer.
            // Note that the first digit is never '0' if s32 is of 7 digits, because the input is never zero.
            uint32_t has_non_zero_second_head_digit = has_second_head_digit & static_cast<uint32_t>(buffer[first_head_digit_chars_count] > '0');
            buffer += 1 + has_non_zero_second_head_digit * first_head_digit_chars_count;
        } else {
            // At least one of the remaining 6 digits are nonzero.

            // After this adjustment, now the first destination becomes buffer + first_head_digit_chars_count.
            buffer += has_second_head_digit;

            // Obtain the next two digits (d1, d2).
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            uint32_t d1_index = first_head_digit_chars_count;
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + d1_index);

            if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 1'0000)) {
                // The remaining 4 digits (d3, d4, d5, d6) are all zero.
                uint32_t d2_index = d1_index + 1;
                uint32_t has_non_zero_d2 = static_cast<uint32_t>(buffer[d2_index] > '0');
                buffer += d2_index + has_non_zero_d2;
            } else {
                // At least one of the remaining 4 digits are nonzero.
                // Obtain the next two digits (d3, d4).
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                uint32_t d3_index = d1_index + 2;
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + d3_index);

                if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 100)) {
                    // The remaining 2 digits (d5, d6) are all zero.
                    uint32_t d4_index = d3_index + 1;
                    uint32_t has_non_zero_d4 = static_cast<uint32_t>(buffer[d4_index] > '0');
                    buffer += d4_index + has_non_zero_d4;
                } else {
                    // Obtain the last two digits (d5, d6).
                    prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                    uint32_t d5_index = d3_index + 2;
                    print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + d5_index);

                    uint32_t d6_index = d5_index + 1;
                    uint32_t has_non_zero_d6 = static_cast<uint32_t>(buffer[d6_index] > '0');
                    buffer += d6_index + has_non_zero_d6;
                }
            }
        }
    } else if (s32 >= 1'0000) {
        // 5 or 6 digits which consist of the head digits (one or two digits) and the remaining 4 digits (d1, d2, d3, d4).

        // 429497 = ceil(2^32 / 1'0000)
        auto prod = s32 * static_cast<uint64_t>(429497);
        auto const head_digits = static_cast<uint32_t>(prod >> 32);
        // Has 2nd head digit also means it's of 6 digits.
        uint32_t has_second_head_digit = static_cast<uint32_t>(head_digits >= 10);

        // If s32 is of 6 digits, increase the exponent by 5. Otherwise, increase it by 4.
        exponent += 4 + has_second_head_digit;

        uint32_t first_head_digit_index = head_digits * 2;
        // Write the first head digit and the decimal point if needed.
        memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
        // Write the second head digit. This character may be overwritten later but we don't care.
        buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

        if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 1'0000)) {
            // The remaining 4 digits (d1, d2, d3, d4) are all zero.
            // The number of characters actually written is 1 or first_head_digit_chars_count + 1, similarly to the case of 7 or 8 digits.
            uint32_t has_non_zero_second_head_digit = has_second_head_digit & static_cast<uint32_t>(buffer[first_head_digit_chars_count] > '0');
            buffer += 1 + has_non_zero_second_head_digit * first_head_digit_chars_count;
        } else {
            // At least one of the remaining 4 digits are nonzero.

            // After this adjustment, now the first destination becomes buffer + first_head_digit_chars_count.
            buffer += has_second_head_digit;

            // Obtain the next two digits (d1, d2).
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            uint32_t d1_index = first_head_digit_chars_count;
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + d1_index);

            if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 100)) {
                // The remaining 2 digits (d3, d4) are all zero.
                uint32_t d2_index = d1_index + 1;
                uint32_t has_non_zero_d2 = static_cast<uint32_t>(buffer[d2_index] > '0');
                buffer += d2_index + has_non_zero_d2;
            } else {
                // Obtain the last two digits (d3, d4).
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                uint32_t d3_index = d1_index + 2;
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + d3_index);

                uint32_t d4_index = d3_index + 1;
                uint32_t has_non_zero_d4 = static_cast<uint32_t>(buffer[d4_index] > '0');
                buffer += d4_index + has_non_zero_d4;
            }
        }
    } else if (s32 >= 100) {
        // 3 or 4 digits which consist of the head digits (one or two digits) and the remaining 2 digits (d1, d2).

        // 42949673 = ceil(2^32 / 100)
        auto prod = s32 * static_cast<uint64_t>(42949673);
        auto const head_digits = static_cast<uint32_t>(prod >> 32);
        // Has 2nd head digit also means it's of 4 digits.
        uint32_t has_second_head_digit = static_cast<uint32_t>(head_digits >= 10);

        // If s32 is of 4 digits, increase the exponent by 3. Otherwise, increase it by 2.
        exponent += 2 + has_second_head_digit;

        uint32_t first_head_digit_index = head_digits * 2;
        // Write the first head digit and the decimal point if needed.
        memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
        // Write the second head digit. This character may be overwritten later but we don't care.
        buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

        if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 100)) {
            // The remaining 2 digits (d1, d2) are all zero.
            // The number of characters actually written is 1 or first_head_digit_chars_count + 1, similarly to the case of 7 or 8 digits.
            uint32_t has_non_zero_second_head_digit = has_second_head_digit & static_cast<uint32_t>(buffer[first_head_digit_chars_count] > '0');
            buffer += 1 + has_non_zero_second_head_digit * first_head_digit_chars_count;
        } else {
            // At least one of the remaining 2 digits (d1, d2) are nonzero.

            // After this adjustment, now the first destination becomes buffer + first_head_digit_chars_count.
            buffer += has_second_head_digit;

            // Obtain the last two digits.
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            uint32_t d1_index = first_head_digit_chars_count;
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + d1_index);

            uint32_t d2_index = d1_index + 1;
            uint32_t has_non_zero_d2 = static_cast<uint32_t>(buffer[d2_index] > '0');
            buffer += d2_index + has_non_zero_d2;
        }
    } else {
        // 1 or 2 digits which consist of the head digits (one or two digits).

        // Has 2nd head digit also means it's of 2 digits.
        uint32_t has_second_head_digit = static_cast<uint32_t>(s32 >= 10);
        // If s32 is of 2 digits, increase the exponent by 1.
        exponent += has_second_head_digit;

        uint32_t first_head_digit_index = s32 * 2;
        // Write the first head digit and the decimal point if needed.
        memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
        // Write the second head digit. This character may be overwritten later but we don't care.
        buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

        // The number of characters actually written is 1 or first_head_digit_chars_count + 1, similarly to the case of 7 or 8 digits.
        uint32_t has_non_zero_second_head_digit = has_second_head_digit & static_cast<uint32_t>(buffer[first_head_digit_chars_count] > '0');
        buffer += 1 + has_non_zero_second_head_digit * first_head_digit_chars_count;
    }
}

namespace detail {

ALWAYS_INLINE static char* float_to_chars_impl(const uint32_t significand, int32_t exponent, char* buffer)
{
    // Print significand.
    print_9_digits<Mode::ToExponential>(significand, exponent, buffer);

    // Print exponent and return
    if (exponent < 0) {
        memcpy(buffer, "e-", 2);
        buffer += 2;
        exponent = -exponent;
    } else {
        memcpy(buffer, "e+", 2);
        buffer += 2;
    }

    if (exponent >= 10) {
        print_2_digits(static_cast<uint32_t>(exponent), buffer);
        buffer += 2;
    } else {
        print_1_digit(static_cast<uint32_t>(exponent), buffer);
        buffer += 1;
    }

    return buffer;
}

template <>
char* to_chars_impl<float, default_float_traits<float>, Mode::ToExponential, PrintTrailingZero::No>(uint32_t significand, int32_t exponent, char* buffer)
{
    return float_to_chars_impl(significand, exponent, buffer);
}

template <Mode mode, PrintTrailingZero print_trailing_zero>
ALWAYS_INLINE static char* double_to_chars_impl(const uint64_t significand, int32_t exponent, char* buffer)
{
    // Print significand by decomposing it into a 9-digit block and a 8-digit block.
    uint32_t first_block = 0;
    uint32_t second_block = 0;
    bool has_second_block = true;

    if (significand >= 1'0000'0000) {
        first_block = static_cast<uint32_t>(significand / 1'0000'0000);
        second_block = static_cast<uint32_t>(significand) - first_block * 1'0000'0000;
        exponent += 8;
    } else {
        first_block = static_cast<uint32_t>(significand);
        has_second_block = false;
    }

    if (!second_block && print_trailing_zero == PrintTrailingZero::No)
        print_9_digits<mode>(first_block, exponent, buffer);
    else {
        // If need to print the decimal point then print "d." else just print "d".
        constexpr uint32_t first_head_digit_chars_count = static_cast<uint32_t>(mode);

        if (first_block >= 1'0000'0000) {
            // We proceed similarly to print_9_digits(), but since we do not need to remove
            // trailing zeros, the procedure is a bit simpler.

            // The input is of 17 digits, thus there should be no trailing zero at all.
            // The first block is of 9 digits.
            // 1441151882 = ceil(2^57 / 1'0000'0000) + 1
            auto prod = first_block * static_cast<uint64_t>(1441151882);
            prod >>= 25;
            memcpy(buffer, radix_100_head_table + static_cast<uint32_t>(prod >> 32) * 2, first_head_digit_chars_count);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 2);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 4);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 6);

            // The second block is of 8 digits.
            // 281474978 = ceil(2^48 / 100'0000) + 1
            prod = second_block * static_cast<uint64_t>(281474978);
            prod >>= 16;
            prod += 1;
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 8);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 10);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 12);
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 14);

            exponent += 8;
            buffer += first_head_digit_chars_count + 16;
        } else {
            if (first_block >= 100'0000) {
                // 7 or 8 digits which consist of the head digits (one or two digits) and the remaining 6 digits.
                // 281474978 = ceil(2^48 / 100'0000) + 1
                auto prod = first_block * static_cast<uint64_t>(281474978);
                prod >>= 16;
                auto const head_digits = static_cast<uint32_t>(prod >> 32);
                // Has 2nd head digit also means it's of 8 digits.
                uint32_t has_second_head_digit = static_cast<uint32_t>(head_digits >= 10);

                uint32_t first_head_digit_index = head_digits * 2;
                memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
                buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

                exponent += 6 + has_second_head_digit;
                buffer += has_second_head_digit;

                // Print remaining 6 digits.
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count);
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 2);
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 4);

                buffer += first_head_digit_chars_count + 6;
            } else if (first_block >= 1'0000) {
                // 5 or 6 digits which consist of the head digits (one or two digits) and the remaining 4 digits.

                // 429497 = ceil(2^32 / 1'0000)
                auto prod = first_block * static_cast<uint64_t>(429497);
                auto const head_digits = static_cast<uint32_t>(prod >> 32);
                // Has 2nd head digit also means it's of 6 digits.
                uint32_t has_second_head_digit = static_cast<uint32_t>(head_digits >= 10);

                uint32_t first_head_digit_index = head_digits * 2;
                memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
                buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

                exponent += 4 + has_second_head_digit;
                buffer += has_second_head_digit;

                // Print remaining 4 digits.
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count);
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count + 2);

                buffer += first_head_digit_chars_count + 4;
            } else if (first_block >= 100) {
                // 3 or 4 digits which consist of the head digits (one or two digits) and the remaining 2 digits.

                // 42949673 = ceil(2^32 / 100)
                auto prod = first_block * static_cast<uint64_t>(42949673);
                auto const head_digits = static_cast<uint32_t>(prod >> 32);
                // Has 2nd head digit also means it's of 4 digits.
                uint32_t has_second_head_digit = static_cast<uint32_t>(head_digits >= 10);

                uint32_t first_head_digit_index = head_digits * 2;
                memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
                buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

                exponent += 2 + has_second_head_digit;
                buffer += has_second_head_digit;

                // Print remaining 2 digits.
                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + first_head_digit_chars_count);

                buffer += first_head_digit_chars_count + 2;
            } else {
                // 1 or 2 digits which consist of the head digits (one or two digits).

                // Has 2nd head digit also means it's of 2 digits.
                uint32_t has_second_head_digit = static_cast<uint32_t>(first_block >= 10);

                uint32_t first_head_digit_index = first_block * 2;
                memcpy(buffer, radix_100_head_table + first_head_digit_index, first_head_digit_chars_count);
                buffer[first_head_digit_chars_count] = radix_100_table[first_head_digit_index + 1];

                exponent += has_second_head_digit;
                buffer += first_head_digit_chars_count + has_second_head_digit;
            }

            // Next, print the second block. The second block is of 8 digits, but we may have trailing zeros.
            if (has_second_block) {
                // 281474978 = ceil(2^48 / 100'0000) + 1
                auto prod = second_block * static_cast<uint64_t>(281474978);
                prod >>= 16;
                prod += 1;
                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer);

                if constexpr (print_trailing_zero == PrintTrailingZero::No) {
                    // Remaining 6 digits are all zero?
                    if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 100'0000))
                        buffer += (1 + unsigned(buffer[1] > '0'));
                    else {
                        // Obtain the next two digits.
                        prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                        print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + 2);

                        // Remaining 4 digits are all zero?
                        if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 1'0000))
                            buffer += (3 + unsigned(buffer[3] > '0'));
                        else {
                            // Obtain the next two digits.
                            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                            print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + 4);

                            // Remaining 2 digits are all zero?
                            if (static_cast<uint32_t>(prod) <= static_cast<uint32_t>((static_cast<uint64_t>(1) << 32) / 100))
                                buffer += (5 + unsigned(buffer[5] > '0'));
                            else {
                                // Obtain the last two digits.
                                prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                                print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + 6);
                                buffer += (7 + unsigned(buffer[7] > '0'));
                            }
                        }
                    }
                } else {
                    // Obtain the remaining 6 digits.
                    prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                    print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + 2);
                    prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                    print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + 4);
                    prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
                    print_2_digits(static_cast<uint32_t>(prod >> 32), buffer + 6);
                    buffer += 8;
                }
            }
        }
    }

    if constexpr (mode == Mode::ToShortest)
        return buffer;

    // Print exponent and return
    if (exponent < 0) {
        memcpy(buffer, "e-", 2);
        buffer += 2;
        exponent = -exponent;
    } else {
        memcpy(buffer, "e+", 2);
        buffer += 2;
    }

    if (exponent >= 100) {
        // d1 = exponent / 10; d2 = exponent % 10;
        // 6554 = ceil(2^16 / 10)
        auto prod = static_cast<uint32_t>(exponent) * static_cast<uint32_t>(6554);
        auto d1 = prod >> 16;
        prod = static_cast<uint16_t>(prod) * static_cast<uint32_t>(5); // * 10
        auto d2 = prod >> 15; // >> 16
        print_2_digits(d1, buffer);
        print_1_digit(d2, buffer + 2);
        buffer += 3;
    } else if (exponent >= 10) {
        print_2_digits(static_cast<uint32_t>(exponent), buffer);
        buffer += 2;
    } else {
        print_1_digit(static_cast<uint32_t>(exponent), buffer);
        buffer += 1;
    }

    return buffer;
}

template <>
char* to_chars_impl<double, default_float_traits<double>, Mode::ToExponential, PrintTrailingZero::No>(uint64_t significand, int32_t exponent, char* buffer)
{
    return double_to_chars_impl<Mode::ToExponential, PrintTrailingZero::No>(significand, exponent, buffer);
}

char* to_shortest(const uint64_t significand, int32_t exponent, char* buffer)
{
    ASSERT(significand);

    // significand = 12340, exponent = -2, result = 123.4
    // significand = 12345, exponent = -2, result = 123.45
    // significand = 12345, exponent =  2, result = 1234500
    int32_t significand_digits_count = count_digits_base10_with_max_17(significand);
    int32_t integral_digits_count = significand_digits_count + exponent;
    if (!valid_shortest_representation(integral_digits_count))
        return double_to_chars_impl<Mode::ToExponential, PrintTrailingZero::No>(significand, exponent, buffer);

    if (exponent >= 0) {
        buffer = double_to_chars_impl<Mode::ToShortest, PrintTrailingZero::Yes>(significand, exponent, buffer);
        while (exponent--)
            *buffer++ = '0';
        return buffer;
    }

    if (integral_digits_count > 0) {
        // significand = 12345, exponent = -2, significand_digits_count = 5, integral_digits_count = 3, fractional_digits_count = 2
        int32_t fractional_digits_count = significand_digits_count - integral_digits_count;
        ASSERT(0 < fractional_digits_count && fractional_digits_count < ieee754_binary64::decimal_digits && fractional_digits_count == -exponent);
        ASSERT(0 < integral_digits_count && integral_digits_count < ieee754_binary64::decimal_digits);
        uint64_t base = compute_power_with_max_16(10, fractional_digits_count);

        // Obtain and write the integral part.
        uint64_t integral = significand / base;
        buffer = double_to_chars_impl<Mode::ToShortest, PrintTrailingZero::Yes>(integral, exponent, buffer);

        // Obtain and write the fractional part if needed.
        uint64_t fractional = significand % base;
        if (fractional) {
            // Given this case:
            //     significand = 12305, exponent = -2, integral_digits_count = 3, fractional_digits_count = 2, integral = 123, fractional = 5
            // Write ".0" first and then write "5".
            int32_t actual_fractional_digits_count = count_digits_base10_with_max_17(fractional);
            *buffer++ = '.';
            while (actual_fractional_digits_count++ < fractional_digits_count)
                *buffer++ = '0';
            buffer = double_to_chars_impl<Mode::ToShortest, PrintTrailingZero::No>(fractional, exponent, buffer);
        }
    } else {
        // Given this case:
        //     significand = 12345, exponent = -7, integral_digits_count = -2, result = 0.0012345
        // Write "0.00" first and then write "12345".
        memcpy(buffer, "0.", 2);
        buffer += 2;
        while (integral_digits_count++ < 0)
            *buffer++ = '0';
        buffer = double_to_chars_impl<Mode::ToShortest, PrintTrailingZero::No>(significand, exponent, buffer);
    }
    return buffer;
}

} // namespace detail

} // namespace dragonbox

} // namespace WTF
