/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2011 the V8 project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Part of code is from https://github.com/Maratyszcza/FP16
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Facebook Inc.
 * Copyright (c) 2017 Georgia Institute of Technology
 * Copyright 2019 Google LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#include <bit>
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

/*
 * Convert a 16-bit floating-point number in IEEE half-precision format, in bit representation, to
 * a 32-bit floating-point number in IEEE single-precision format.
 *
 * @note The implementation relies on IEEE-like (no assumption about rounding mode and no operations on denormals)
 * floating-point operations and bitcasts between integer and floating-point variables.
 */
constexpr float convertFloat16ToFloat32(uint16_t h)
{
#if HAVE(FLOAT16)
    return static_cast<float>(std::bit_cast<_Float16>(h));
#else
    /*
     * Extend the half-precision floating-point number to 32 bits and shift to the upper part of the 32-bit word:
     *      +---+-----+------------+-------------------+
     *      | S |EEEEE|MM MMMM MMMM|0000 0000 0000 0000|
     *      +---+-----+------------+-------------------+
     * Bits  31  26-30    16-25            0-15
     *
     * S - sign bit, E - bits of the biased exponent, M - bits of the mantissa, 0 - zero bits.
     */
    const uint32_t w = (uint32_t) h << 16;
    /*
     * Extract the sign of the input number into the high bit of the 32-bit word:
     *
     *      +---+----------------------------------+
     *      | S |0000000 00000000 00000000 00000000|
     *      +---+----------------------------------+
     * Bits  31                 0-31
     */
    const uint32_t sign = w & UINT32_C(0x80000000);
    /*
     * Extract mantissa and biased exponent of the input number into the high bits of the 32-bit word:
     *
     *      +-----+------------+---------------------+
     *      |EEEEE|MM MMMM MMMM|0 0000 0000 0000 0000|
     *      +-----+------------+---------------------+
     * Bits  27-31    17-26            0-16
     */
    const uint32_t two_w = w + w;

    /*
     * Shift mantissa and exponent into bits 23-28 and bits 13-22 so they become mantissa and exponent
     * of a single-precision floating-point number:
     *
     *       S|Exponent |          Mantissa
     *      +-+---+-----+------------+----------------+
     *      |0|000|EEEEE|MM MMMM MMMM|0 0000 0000 0000|
     *      +-+---+-----+------------+----------------+
     * Bits   | 23-31   |           0-22
     *
     * Next, there are some adjustments to the exponent:
     * - The exponent needs to be corrected by the difference in exponent bias between single-precision and half-precision
     *   formats (0x7F - 0xF = 0x70)
     * - Inf and NaN values in the inputs should become Inf and NaN values after conversion to the single-precision number.
     *   Therefore, if the biased exponent of the half-precision input was 0x1F (max possible value), the biased exponent
     *   of the single-precision output must be 0xFF (max possible value). We do this correction in two steps:
     *   - First, we adjust the exponent by (0xFF - 0x1F) = 0xE0 (see exp_offset below) rather than by 0x70 suggested
     *     by the difference in the exponent bias (see above).
     *   - Then we multiply the single-precision result of exponent adjustment by 2**(-112) to reverse the effect of
     *     exponent adjustment by 0xE0 less the necessary exponent adjustment by 0x70 due to difference in exponent bias.
     *     The floating-point multiplication hardware would ensure than Inf and NaN would retain their value on at least
     *     partially IEEE754-compliant implementations.
     *
     * Note that the above operations do not handle denormal inputs (where biased exponent == 0). However, they also do not
     * operate on denormal inputs, and do not produce denormal results.
     */
    const uint32_t exp_offset = UINT32_C(0xE0) << 23;
    const float exp_scale = 0x1.0p-112f; // 0x7800000
    const float normalized_value = std::bit_cast<float>((two_w >> 4) + exp_offset) * exp_scale;

    /*
     * Convert denormalized half-precision inputs into single-precision results (always normalized).
     * Zero inputs are also handled here.
     *
     * In a denormalized number the biased exponent is zero, and mantissa has on-zero bits.
     * First, we shift mantissa into bits 0-9 of the 32-bit word.
     *
     *                  zeros           |  mantissa
     *      +---------------------------+------------+
     *      |0000 0000 0000 0000 0000 00|MM MMMM MMMM|
     *      +---------------------------+------------+
     * Bits             10-31                0-9
     *
     * Now, remember that denormalized half-precision numbers are represented as:
     *    FP16 = mantissa * 2**(-24).
     * The trick is to construct a normalized single-precision number with the same mantissa and thehalf-precision input
     * and with an exponent which would scale the corresponding mantissa bits to 2**(-24).
     * A normalized single-precision floating-point number is represented as:
     *    FP32 = (1 + mantissa * 2**(-23)) * 2**(exponent - 127)
     * Therefore, when the biased exponent is 126, a unit change in the mantissa of the input denormalized half-precision
     * number causes a change of the constructud single-precision number by 2**(-24), i.e. the same ammount.
     *
     * The last step is to adjust the bias of the constructed single-precision number. When the input half-precision number
     * is zero, the constructed single-precision number has the value of
     *    FP32 = 1 * 2**(126 - 127) = 2**(-1) = 0.5
     * Therefore, we need to subtract 0.5 from the constructed single-precision number to get the numerical equivalent of
     * the input half-precision number.
     */
    const uint32_t magic_mask = UINT32_C(126) << 23;
    const float magic_bias = 0.5f;
    const float denormalized_value = std::bit_cast<float>((two_w >> 17) | magic_mask) - magic_bias;

    /*
     * - Choose either results of conversion of input as a normalized number, or as a denormalized number, depending on the
     *   input exponent. The variable two_w contains input exponent in bits 27-31, therefore if its smaller than 2**27, the
     *   input is either a denormal number, or zero.
     * - Combine the result of conversion of exponent and mantissa with the sign of the input number.
     */
    const uint32_t denormalized_cutoff = UINT32_C(1) << 27;
    const uint32_t result = sign |
        (two_w < denormalized_cutoff ? std::bit_cast<uint32_t>(denormalized_value) : std::bit_cast<uint32_t>(normalized_value));
    return std::bit_cast<float>(result);
#endif
}

constexpr double convertFloat16ToFloat64(uint16_t h)
{
    return static_cast<double>(convertFloat16ToFloat32(h));
}

/*
 * Convert a 32-bit floating-point number in IEEE single-precision format to a 16-bit floating-point number in
 * IEEE half-precision format, in bit representation.
 *
 * @note The implementation relies on IEEE-like (no assumption about rounding mode and no operations on denormals)
 * floating-point operations and bitcasts between integer and floating-point variables.
 */
constexpr uint16_t convertFloat32ToFloat16(float f)
{
#if HAVE(FLOAT16)
    return std::bit_cast<uint16_t>(static_cast<_Float16>(f));
#else
    const float scale_to_inf = 0x1.0p+112f; // 0x77800000
    const float scale_to_zero = 0x1.0p-110f; // 0x08800000
    const float saturated_f = __builtin_fabsf(f) * scale_to_inf;
    float base = saturated_f * scale_to_zero;

    const uint32_t w = std::bit_cast<uint32_t>(f);
    const uint32_t shl1_w = w + w;
    const uint32_t sign = w & UINT32_C(0x80000000);
    uint32_t bias = shl1_w & UINT32_C(0xFF000000);
    if (bias < UINT32_C(0x71000000)) {
        bias = UINT32_C(0x71000000);
    }

    base = std::bit_cast<float>((bias >> 1) + UINT32_C(0x07800000)) + base;
    const uint32_t bits = std::bit_cast<uint32_t>(base);
    const uint32_t exp_bits = (bits >> 13) & UINT32_C(0x00007C00);
    const uint32_t mantissa_bits = bits & UINT32_C(0x00000FFF);
    const uint32_t nonsign = exp_bits + mantissa_bits;
    return (sign >> 16) | (shl1_w > UINT32_C(0xFF000000) ? UINT16_C(0x7E00) : nonsign);
#endif
}

// Adopted from V8's DoubleToFloat16, which adopted from https://gist.github.com/rygorous/2156668.
constexpr uint16_t convertFloat64ToFloat16(double value)
{
#if HAVE(FLOAT16)
    return std::bit_cast<uint16_t>(static_cast<_Float16>(value));
#else
    // uint64_t constants prefixed with kFP64 are bit patterns of doubles.
    // uint64_t constants prefixed with kFP16 are bit patterns of doubles encoding
    // limits of half-precision floating point values.
    constexpr int kFP64ExponentBits = 11;
    constexpr int kFP64MantissaBits = 52;
    constexpr uint64_t kFP64ExponentBias = 1023;
    constexpr uint64_t kFP64SignMask = uint64_t{1} << (kFP64ExponentBits + kFP64MantissaBits);
    constexpr uint64_t kFP64Infinity = uint64_t{2047} << kFP64MantissaBits;
    constexpr uint64_t kFP16InfinityAndNaNInfimum = (kFP64ExponentBias + 16) << kFP64MantissaBits;
    constexpr uint64_t kFP16MinExponent = kFP64ExponentBias - 14;
    constexpr uint64_t kFP16DenormalThreshold = kFP16MinExponent << kFP64MantissaBits;

    constexpr int kFP16MantissaBits = 10;
    constexpr uint16_t kFP16qNaN = 0x7e00;
    constexpr uint16_t kFP16Infinity = 0x7c00;

    // A value that, when added, has the effect that if any of the lower 41 bits of
    // the mantissa are set, the 11th mantissa bit from the front becomes set. Used
    // for rounding when converting from double to half-precision.
    constexpr uint64_t kFP64To16RoundingAddend = (uint64_t{1} << ((kFP64MantissaBits - kFP16MantissaBits) - 1)) - 1;
    // A value that, when added, rebiases the exponent of a double to the range of
    // the half precision and performs rounding as described above in
    // kFP64To16RoundingAddend. Note that 15-kFP64ExponentBias overflows into the
    // sign bit, but that bit is implicitly cut off when assigning the 64-bit double
    // to a 16-bit output.
    constexpr uint64_t kFP64To16RebiasExponentAndRound = ((uint64_t{15} - kFP64ExponentBias) << kFP64MantissaBits) + kFP64To16RoundingAddend;
    // A magic value that aligns 10 mantissa bits at the bottom of the double when
    // added to a double using floating point addition. Depends on floating point
    // addition being round-to-nearest-even.
    constexpr uint64_t kFP64To16DenormalMagic = (kFP16MinExponent + (kFP64MantissaBits - kFP16MantissaBits)) << kFP64MantissaBits;

    uint64_t in = std::bit_cast<uint64_t>(value);
    uint16_t out = 0;

    // Take the absolute value of the input.
    uint64_t sign = in & kFP64SignMask;
    in ^= sign;

    if (in >= kFP16InfinityAndNaNInfimum) {
        // Result is infinity or NaN.
        out = (in > kFP64Infinity) ? kFP16qNaN       // NaN->qNaN
        : kFP16Infinity;  // Inf->Inf
    } else {
        // Result is a (de)normalized number or zero.

        if (in < kFP16DenormalThreshold) {
            // Result is a denormal or zero. Use the magic value and FP addition to
            // align 10 mantissa bits at the bottom of the float. Depends on FP
            // addition being round-to-nearest-even.
            double temp = std::bit_cast<double>(in) +
            std::bit_cast<double>(kFP64To16DenormalMagic);
            out = std::bit_cast<uint64_t>(temp) - kFP64To16DenormalMagic;
        } else {
            // Result is not a denormal.

            // Remember if the result mantissa will be odd before rounding.
            uint64_t mant_odd = (in >> (kFP64MantissaBits - kFP16MantissaBits)) & 1;

            // Update the exponent and round to nearest even.
            //
            // Rounding to nearest even is handled in two parts. First, adding
            // kFP64To16RebiasExponentAndRound has the effect of rebiasing the
            // exponent and that if any of the lower 41 bits of the mantissa are set,
            // the 11th mantissa bit from the front becomes set. Second, adding
            // mant_odd ensures ties are rounded to even.
            in += kFP64To16RebiasExponentAndRound;
            in += mant_odd;

            out = in >> (kFP64MantissaBits - kFP16MantissaBits);
        }
    }

    out |= sign >> 48;
    return out;
#endif
}

#if HAVE(FLOAT16)
class Float16 {
public:
    constexpr Float16() = default;
    constexpr Float16(double value)
        : m_value(static_cast<_Float16>(value))
    {
    }

    static constexpr Float16 min() { return Float16 { std::bit_cast<_Float16>(static_cast<uint16_t>(0xfbff)) }; }
    static constexpr Float16 max() { return Float16 { std::bit_cast<_Float16>(static_cast<uint16_t>(0x7bff)) }; }

    constexpr operator double() const
    {
        return static_cast<double>(m_value);
    }

    auto operator<=>(const Float16& other) const
    {
        return m_value <=> other.m_value;
    }

    bool operator==(const Float16& other) const
    {
        return m_value == other.m_value;
    }

    bool operator!=(const Float16& other) const
    {
        return m_value != other.m_value;
    }

private:
    explicit constexpr Float16(_Float16 value)
        : m_value(value)
    {
    }

    _Float16 m_value { 0 };
};
#else
class Float16 {
public:
    constexpr Float16() = default;
    constexpr Float16(double value)
        : Float16(convertFloat64ToFloat16(value))
    {
    }

    static constexpr Float16 min() { return Float16 { static_cast<uint16_t>(0xfbff) }; }
    static constexpr Float16 max() { return Float16 { static_cast<uint16_t>(0x7bff) }; }

    constexpr operator double() const
    {
        return asDouble();
    }

    auto operator<=>(const Float16& other) const
    {
        return asDouble() <=> other.asDouble();
    }

    bool operator==(const Float16& other) const
    {
        return asDouble() == other.asDouble();
    }

    bool operator!=(const Float16& other) const
    {
        return asDouble() != other.asDouble();
    }

private:
    explicit constexpr Float16(uint16_t value)
        : m_value(value)
    {
    }

    constexpr double asDouble() const { return convertFloat16ToFloat64(m_value); }

    uint16_t m_value { 0 };
};
#endif

static_assert(sizeof(Float16) == sizeof(uint16_t));

}

using WTF::Float16;
