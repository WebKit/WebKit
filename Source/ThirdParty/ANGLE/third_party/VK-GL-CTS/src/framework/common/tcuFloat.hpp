#ifndef _TCUFLOAT_HPP
#define _TCUFLOAT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Reconfigurable floating-point value template.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

// For memcpy().
#include <limits>
#include <string.h>

namespace tcu
{

enum FloatFlags
{
    FLOAT_HAS_SIGN       = (1 << 0),
    FLOAT_SUPPORT_DENORM = (1 << 1),
    FLOAT_NO_INFINITY    = (1 << 2),
};

enum RoundingDirection
{
    ROUND_TO_EVEN = 0,
    ROUND_DOWNWARD, // Towards -Inf.
    ROUND_UPWARD,   // Towards +Inf.
    ROUND_TO_ZERO
};

/*--------------------------------------------------------------------*//*!
 * \brief Floating-point format template
 *
 * This template implements arbitrary floating-point handling. Template
 * can be used for conversion between different formats and checking
 * various properties of floating-point values.
 *//*--------------------------------------------------------------------*/
template <typename StorageType_, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
class Float
{
public:
    typedef StorageType_ StorageType;

    enum
    {
        EXPONENT_BITS = ExponentBits,
        MANTISSA_BITS = MantissaBits,
        EXPONENT_BIAS = ExponentBias,
        FLAGS         = Flags,
    };

    Float(void);
    explicit Float(StorageType value);
    explicit Float(float v, RoundingDirection rd = ROUND_TO_EVEN);
    explicit Float(double v, RoundingDirection rd = ROUND_TO_EVEN);

    template <typename OtherStorageType, int OtherExponentBits, int OtherMantissaBits, int OtherExponentBias,
              uint32_t OtherFlags>
    static Float convert(
        const Float<OtherStorageType, OtherExponentBits, OtherMantissaBits, OtherExponentBias, OtherFlags> &src,
        RoundingDirection rd = ROUND_TO_EVEN);

    static inline Float convert(const Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> &src,
                                RoundingDirection = ROUND_TO_EVEN)
    {
        return src;
    }

    /*--------------------------------------------------------------------*//*!
     * \brief Construct floating point value
     * \param sign        Sign. Must be +1/-1
     * \param exponent    Exponent in range [1-ExponentBias, ExponentBias+1]
     * \param mantissa    Mantissa bits with implicit leading bit explicitly set
     * \return The specified float
     *
     * This function constructs a floating point value from its inputs.
     * The normally implicit leading bit of the mantissa must be explicitly set.
     * The exponent normally used for zero/subnormals is an invalid input. Such
     * values are specified with the leading mantissa bit of zero and the lowest
     * normal exponent (1-ExponentBias). Additionally having both exponent and
     * mantissa set to zero is a shorthand notation for the correctly signed
     * floating point zero. Inf and NaN must be specified directly with an
     * exponent of ExponentBias+1 and the appropriate mantissa (with leading
     * bit set)
     *//*--------------------------------------------------------------------*/
    static inline Float construct(int sign, int exponent, StorageType mantissa);

    /*--------------------------------------------------------------------*//*!
     * \brief Construct floating point value. Explicit version
     * \param sign        Sign. Must be +1/-1
     * \param exponent    Exponent in range [-ExponentBias, ExponentBias+1]
     * \param mantissa    Mantissa bits
     * \return The specified float
     *
     * This function constructs a floating point value from its inputs with
     * minimal intervention.
     * The sign is turned into a sign bit and the exponent bias is added.
     * See IEEE-754 for additional information on the inputs and
     * the encoding of special values.
     *//*--------------------------------------------------------------------*/
    static Float constructBits(int sign, int exponent, StorageType mantissaBits);

    StorageType bits(void) const
    {
        return m_value;
    }
    float asFloat(void) const;
    double asDouble(void) const;

    inline int signBit(void) const
    {
        return (int)(m_value >> (ExponentBits + MantissaBits)) & 1;
    }
    inline StorageType exponentBits(void) const
    {
        return (m_value >> MantissaBits) & ((StorageType(1) << ExponentBits) - 1);
    }
    inline StorageType mantissaBits(void) const
    {
        return m_value & ((StorageType(1) << MantissaBits) - 1);
    }

    inline int sign(void) const
    {
        return signBit() ? -1 : 1;
    }
    inline int exponent(void) const
    {
        return isDenorm() ? 1 - ExponentBias : (int)exponentBits() - ExponentBias;
    }
    inline StorageType mantissa(void) const
    {
        return isZero() || isDenorm() ? mantissaBits() : (mantissaBits() | (StorageType(1) << MantissaBits));
    }

    inline bool isInf(void) const
    {
        if (!(Flags & FLOAT_NO_INFINITY))
        {
            return exponentBits() == ((1 << ExponentBits) - 1) && mantissaBits() == 0;
        }
        else
        {
            return false;
        }
    }
    inline bool isNaN(void) const
    {
        if (!(Flags & FLOAT_NO_INFINITY))
        {
            return exponentBits() == ((1 << ExponentBits) - 1) && mantissaBits() != 0;
        }
        else
        {
            // For E4M3 there are only two NAN patterns - all mantissa/exponent bits set
            constexpr uint64_t mask = (uint64_t{1} << (ExponentBits + MantissaBits)) - 1;
            return (m_value & mask) == mask;
        }
    }
    inline bool isZero(void) const
    {
        return exponentBits() == 0 && mantissaBits() == 0;
    }
    inline bool isDenorm(void) const
    {
        return exponentBits() == 0 && mantissaBits() != 0;
    }

    inline bool operator<(const Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> &other) const
    {
        return this->asDouble() < other.asDouble();
    }

    static Float zero(int sign);
    static Float inf(int sign);
    static Float nan(void);

    static Float largestNormal(int sign);
    static Float smallestNormal(int sign);

private:
    StorageType m_value;
} DE_WARN_UNUSED_TYPE;

// Common floating-point types.
typedef Float<uint16_t, 5, 10, 15, FLOAT_HAS_SIGN | FLOAT_SUPPORT_DENORM>
    Float16; //!< IEEE 754-2008 16-bit floating-point value
typedef Float<uint32_t, 8, 23, 127, FLOAT_HAS_SIGN | FLOAT_SUPPORT_DENORM>
    Float32; //!< IEEE 754 32-bit floating-point value
typedef Float<uint64_t, 11, 52, 1023, FLOAT_HAS_SIGN | FLOAT_SUPPORT_DENORM>
    Float64; //!< IEEE 754 64-bit floating-point value
typedef Float<uint16_t, 8, 7, 127, FLOAT_HAS_SIGN | FLOAT_SUPPORT_DENORM>
    BrainFloat16; //!< IEEE 754-2008 16-bit floating-point value
                  //
typedef Float<uint16_t, 5, 10, 15, FLOAT_HAS_SIGN>
    Float16Denormless; //!< IEEE 754-2008 16-bit floating-point value without denormalized support

typedef Float<uint8_t, 5, 2, 15, FLOAT_HAS_SIGN | FLOAT_SUPPORT_DENORM> FloatE5M2;

typedef Float<uint8_t, 4, 3, 7, FLOAT_HAS_SIGN | FLOAT_SUPPORT_DENORM | FLOAT_NO_INFINITY> FloatE4M3;

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::Float(void) : m_value(0)
{
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::Float(StorageType value) : m_value(value)
{
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::Float(float value, RoundingDirection rd)
    : m_value(0)
{
    uint32_t u32;
    memcpy(&u32, &value, sizeof(uint32_t));
    *this = convert(Float32(u32), rd);
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::Float(double value, RoundingDirection rd)
    : m_value(0)
{
    uint64_t u64;
    memcpy(&u64, &value, sizeof(uint64_t));
    *this = convert(Float64(u64), rd);
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline float Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::asFloat(void) const
{
    float v;
    uint32_t u32 = Float32::convert(*this).bits();
    memcpy(&v, &u32, sizeof(uint32_t));
    return v;
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline double Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::asDouble(void) const
{
    double v;
    uint64_t u64 = Float64::convert(*this).bits();
    memcpy(&v, &u64, sizeof(uint64_t));
    return v;
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::zero(int sign)
{
    DE_ASSERT(sign == 1 || ((Flags & FLOAT_HAS_SIGN) && sign == -1));
    return Float(StorageType((sign > 0 ? 0ull : 1ull) << (ExponentBits + MantissaBits)));
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::inf(int sign)
{
    DE_ASSERT(!(Flags & FLOAT_NO_INFINITY));
    DE_ASSERT(sign == 1 || ((Flags & FLOAT_HAS_SIGN) && sign == -1));
    return Float(StorageType(((sign > 0 ? 0ull : 1ull) << (ExponentBits + MantissaBits)) |
                             (((1ull << ExponentBits) - 1) << MantissaBits)));
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::nan(void)
{
    return Float(StorageType((1ull << (ExponentBits + MantissaBits)) - 1));
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::largestNormal(int sign)
{
    DE_ASSERT(sign == 1 || ((Flags & FLOAT_HAS_SIGN) && sign == -1));
    if (!(Flags & FLOAT_NO_INFINITY))
    {
        return Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::construct(
            sign, ExponentBias, (static_cast<StorageType>(1) << (MantissaBits + 1)) - 1);
    }
    else
    {
        // E4M3 has all exponent bits set, and the LSB of mantissa not set
        return Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::construct(
            sign, ExponentBias + 1, (static_cast<StorageType>(1) << (MantissaBits + 1)) - 2);
    }
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
inline Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::smallestNormal(int sign)
{
    DE_ASSERT(sign == 1 || ((Flags & FLOAT_HAS_SIGN) && sign == -1));
    return Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::construct(
        sign, 1 - ExponentBias, (static_cast<StorageType>(1) << MantissaBits));
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::construct(int sign, int exponent,
                                                                             StorageType mantissa)
{
    // Repurpose this otherwise invalid input as a shorthand notation for zero (no need for caller to care about internal representation)
    const bool isShorthandZero = exponent == 0 && mantissa == 0;

    // Handles the typical notation for zero (min exponent, mantissa 0). Note that the exponent usually used exponent (-ExponentBias) for zero/subnormals is not used.
    // Instead zero/subnormals have the (normally implicit) leading mantissa bit set to zero.
    const bool isDenormOrZero = (exponent == 1 - ExponentBias) && (mantissa >> MantissaBits == 0);
    const StorageType s   = StorageType((StorageType(sign < 0 ? 1 : 0)) << (StorageType(ExponentBits + MantissaBits)));
    const StorageType exp = (isShorthandZero || isDenormOrZero) ? StorageType(0) : StorageType(exponent + ExponentBias);

    DE_ASSERT(sign == +1 || sign == -1);
    DE_ASSERT(isShorthandZero || isDenormOrZero || mantissa >> MantissaBits == 1);
    DE_ASSERT(exp >> ExponentBits == 0);

    return Float(StorageType(s | (exp << MantissaBits) | (mantissa & ((StorageType(1) << MantissaBits) - 1))));
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<
    StorageType, ExponentBits, MantissaBits, ExponentBias, Flags>::constructBits(int sign, int exponent,
                                                                                 StorageType mantissaBits)
{
    const StorageType signBit      = static_cast<StorageType>(sign < 0 ? 1 : 0);
    const StorageType exponentBits = static_cast<StorageType>(exponent + ExponentBias);

    DE_ASSERT(sign == +1 || sign == -1);
    DE_ASSERT(exponentBits >> ExponentBits == 0);
    DE_ASSERT(mantissaBits >> MantissaBits == 0);

    return Float(
        StorageType((signBit << (ExponentBits + MantissaBits)) | (exponentBits << MantissaBits) | (mantissaBits)));
}

template <typename StorageType, int ExponentBits, int MantissaBits, int ExponentBias, uint32_t Flags>
template <typename OtherStorageType, int OtherExponentBits, int OtherMantissaBits, int OtherExponentBias,
          uint32_t OtherFlags>
Float<StorageType, ExponentBits, MantissaBits, ExponentBias, Flags> Float<StorageType, ExponentBits, MantissaBits,
                                                                          ExponentBias, Flags>::
    convert(const Float<OtherStorageType, OtherExponentBits, OtherMantissaBits, OtherExponentBias, OtherFlags> &other,
            RoundingDirection rd)
{
    int sign = other.sign();

    if (!(Flags & FLOAT_HAS_SIGN) && sign < 0)
    {
        // Negative number, truncate to zero.
        return zero(+1);
    }

    auto infVal = !(Flags & FLOAT_NO_INFINITY) ? inf(sign) : nan();

    if (other.isInf())
    {
        return infVal;
    }

    if (other.isNaN())
    {
        return nan();
    }

    if (other.isZero())
    {
        return zero(sign);
    }

    const int eMin = 1 - ExponentBias;
    const int eMax = ((1 << ExponentBits) - (!(Flags & FLOAT_NO_INFINITY) ? 2 : 1)) - ExponentBias;

    const StorageType s = StorageType((StorageType(other.signBit()))
                                      << (StorageType(ExponentBits + MantissaBits))); // \note Not sign, but sign bit.
    int e               = other.exponent();
    uint64_t m          = other.mantissa();

    // Normalize denormalized values prior to conversion.
    while (!(m & (1ull << OtherMantissaBits)))
    {
        m <<= 1;
        e -= 1;
    }

    if (e < eMin)
    {
        // Underflow.
        if ((Flags & FLOAT_SUPPORT_DENORM) && (eMin - e - 1 <= MantissaBits))
        {
            // Shift and round.
            int bitDiff = (OtherMantissaBits - MantissaBits) + (eMin - e);

            if (bitDiff < 0)
            {
                // The mantissa has more bits than the original and should be left shifted, no need to round.
                return Float(StorageType(s | (m << -bitDiff)));
            }

            uint64_t lastBitsMask = (1ull << bitDiff) - 1ull;
            uint64_t lastBits     = (static_cast<uint64_t>(m) & lastBitsMask);
            uint64_t half         = (1ull << (bitDiff - 1)) - 1;
            uint64_t bias         = (m >> bitDiff) & 1;

            switch (rd)
            {
            case ROUND_TO_EVEN:
                return Float(StorageType(s | (m + half + bias) >> bitDiff));

            case ROUND_DOWNWARD:
                m = (m >> bitDiff);
                if (lastBits != 0ull && sign < 0)
                {
                    m += 1;
                }
                return Float(StorageType(s | m));

            case ROUND_UPWARD:
                m = (m >> bitDiff);
                if (lastBits != 0ull && sign > 0)
                {
                    m += 1;
                }
                return Float(StorageType(s | m));

            case ROUND_TO_ZERO:
                return Float(StorageType(s | (m >> bitDiff)));

            default:
                DE_ASSERT(false);
                break;
            }
        }

        return zero(sign);
    }

    // Remove leading 1.
    m = m & ~(1ull << OtherMantissaBits);

    if (MantissaBits < OtherMantissaBits)
    {
        // Round mantissa.
        int bitDiff           = OtherMantissaBits - MantissaBits;
        uint64_t lastBitsMask = (1ull << bitDiff) - 1ull;
        uint64_t lastBits     = (static_cast<uint64_t>(m) & lastBitsMask);
        uint64_t half         = (1ull << (bitDiff - 1)) - 1;
        uint64_t bias         = (m >> bitDiff) & 1;

        switch (rd)
        {
        case ROUND_TO_EVEN:
            m = (m + half + bias) >> bitDiff;
            break;

        case ROUND_DOWNWARD:
            m = (m >> bitDiff);
            if (lastBits != 0ull && sign < 0)
            {
                m += 1;
            }
            break;

        case ROUND_UPWARD:
            m = (m >> bitDiff);
            if (lastBits != 0ull && sign > 0)
            {
                m += 1;
            }
            break;

        case ROUND_TO_ZERO:
            m = (m >> bitDiff);
            break;

        default:
            DE_ASSERT(false);
            break;
        }

        if (m & (1ull << MantissaBits))
        {
            // Overflow in mantissa.
            m = 0;
            e += 1;
        }
    }
    else
    {
        int bitDiff = MantissaBits - OtherMantissaBits;
        m           = m << bitDiff;
    }

    if (e > eMax)
    {
        // Overflow.
        return (((sign < 0 && rd == ROUND_UPWARD) || (sign > 0 && rd == ROUND_DOWNWARD)) ? largestNormal(sign) :
                                                                                           infVal);
    }

    DE_ASSERT(de::inRange(e, eMin, eMax));
    DE_ASSERT(((e + ExponentBias) & ~((1ull << ExponentBits) - 1)) == 0);
    DE_ASSERT((m & ~((1ull << MantissaBits) - 1)) == 0);

    return Float(StorageType(s | (StorageType(e + ExponentBias) << MantissaBits) | m));
}

typedef typename Float16::StorageType float16_t;
template <class F>
inline constexpr F floatQuietNaN = std::numeric_limits<F>::quiet_NaN();
template <>
inline constexpr float16_t floatQuietNaN<float16_t> = 0x7e01;
template <class F>
inline constexpr F floatSignalingNaN = std::numeric_limits<F>::signaling_NaN();
template <>
inline constexpr float16_t floatSignalingNaN<float16_t> = 0x7c01;

} // namespace tcu

#endif // _TCUFLOAT_HPP
