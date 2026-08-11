/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
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
 * \brief 16-bit floating-point math.
 *//*--------------------------------------------------------------------*/

#include "deFloat16.h"

DE_BEGIN_EXTERN_C

deFloat16 deFloat32To16(float val32)
{
    uint32_t sign;
    int expotent;
    uint32_t mantissa;
    union
    {
        float f;
        uint32_t u;
    } x;

    x.f      = val32;
    sign     = (x.u >> 16u) & 0x00008000u;
    expotent = (int)((x.u >> 23u) & 0x000000ffu) - (127 - 15);
    mantissa = x.u & 0x007fffffu;

    if (expotent <= 0)
    {
        if (expotent < -10)
        {
            /* Rounds to zero. */
            return (deFloat16)sign;
        }

        /* Converted to denormalized half, add leading 1 to significand. */
        mantissa = mantissa | 0x00800000u;

        /* Round mantissa to nearest (10+e) */
        {
            uint32_t t = 14u - expotent;
            uint32_t a = (1u << (t - 1u)) - 1u;
            uint32_t b = (mantissa >> t) & 1u;

            mantissa = (mantissa + a + b) >> t;
        }

        return (deFloat16)(sign | mantissa);
    }
    else if (expotent == 0xff - (127 - 15))
    {
        if (mantissa == 0u)
        {
            /* InF */
            return (deFloat16)(sign | 0x7c00u);
        }
        else
        {
            /* NaN */
            mantissa >>= 13u;
            return (deFloat16)(sign | 0x7c00u | mantissa | (mantissa == 0u));
        }
    }
    else
    {
        /* Normalized float. */
        mantissa = mantissa + 0x00000fffu + ((mantissa >> 13u) & 1u);

        if (mantissa & 0x00800000u)
        {
            /* Overflow in mantissa. */
            mantissa = 0u;
            expotent += 1;
        }

        if (expotent > 30)
        {
            /* \todo [pyry] Cause hw fp overflow */
            return (deFloat16)(sign | 0x7c00u);
        }

        return (deFloat16)(sign | ((uint32_t)expotent << 10u) | (mantissa >> 13u));
    }
}

deFloat16 deFloat64To16(double val64)
{
    uint64_t sign;
    long expotent;
    uint64_t mantissa;
    union
    {
        double f;
        uint64_t u;
    } x;

    x.f      = val64;
    sign     = (x.u >> 48u) & 0x00008000u;
    expotent = (long int)((x.u >> 52u) & 0x000007ffu) - (1023 - 15);
    mantissa = x.u & 0x00fffffffffffffu;

    if (expotent <= 0)
    {
        if (expotent < -10)
        {
            /* Rounds to zero. */
            return (deFloat16)sign;
        }

        /* Converted to denormalized half, add leading 1 to significand. */
        mantissa = mantissa | 0x0010000000000000u;

        /* Round mantissa to nearest (10+e) */
        {
            uint64_t t = 43u - expotent;
            uint64_t a = (1u << (t - 1u)) - 1u;
            uint64_t b = (mantissa >> t) & 1u;

            mantissa = (mantissa + a + b) >> t;
        }

        return (deFloat16)(sign | mantissa);
    }
    else if (expotent == 0x7ff - (1023 - 15))
    {
        if (mantissa == 0u)
        {
            /* InF */
            return (deFloat16)(sign | 0x7c00u);
        }
        else
        {
            /* NaN */
            mantissa >>= 42u;
            return (deFloat16)(sign | 0x7c00u | mantissa | (mantissa == 0u));
        }
    }
    else
    {
        /* Normalized float. */
        mantissa = mantissa + 0x000001ffffffffffu + ((mantissa >> 42u) & 1u);

        if (mantissa & 0x010000000000000u)
        {
            /* Overflow in mantissa. */
            mantissa = 0u;
            expotent += 1;
        }

        if (expotent > 30)
        {
            return (deFloat16)(sign | 0x7c00u);
        }

        return (deFloat16)(sign | ((uint32_t)expotent << 10u) | (mantissa >> 42u));
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Round the given number `val` to nearest even by discarding
 *        the last `numBitsToDiscard` bits.
 * \param val value to round
 * \param numBitsToDiscard number of (least significant) bits to discard
 * \return The rounded value with the last `numBitsToDiscard` removed
 *//*--------------------------------------------------------------------*/
static uint32_t roundToNearestEven(uint32_t val, const uint32_t numBitsToDiscard)
{
    const uint32_t lastBits = val & ((1 << numBitsToDiscard) - 1);
    const uint32_t headBit  = val & (1 << (numBitsToDiscard - 1));

    DE_ASSERT(numBitsToDiscard > 0 && numBitsToDiscard < 32); /* Make sure no overflow. */
    val >>= numBitsToDiscard;

    if (headBit == 0)
    {
        return val;
    }
    else if (headBit == lastBits)
    {
        if ((val & 0x1) == 0x1)
        {
            return val + 1;
        }
        else
        {
            return val;
        }
    }
    else
    {
        return val + 1;
    }
}

deFloat16 deFloat32To16Round(float val32, deRoundingMode mode)
{
    union
    {
        float f;    /* Interpret as 32-bit float */
        uint32_t u; /* Interpret as 32-bit unsigned integer */
    } x;
    uint32_t sign;  /* sign : 0000 0000 0000 0000 X000 0000 0000 0000 */
    uint32_t exp32; /* exp32: biased exponent for 32-bit floats */
    int exp16;      /* exp16: biased exponent for 16-bit floats */
    uint32_t mantissa;

    /* We only support these two rounding modes for now */
    DE_ASSERT(mode == DE_ROUNDINGMODE_TO_ZERO || mode == DE_ROUNDINGMODE_TO_NEAREST_EVEN);

    x.f      = val32;
    sign     = (x.u >> 16u) & 0x00008000u;
    exp32    = (x.u >> 23u) & 0x000000ffu;
    exp16    = (int)(exp32)-127 + 15; /* 15/127: exponent bias for 16-bit/32-bit floats */
    mantissa = x.u & 0x007fffffu;

    /* Case: zero and denormalized floats */
    if (exp32 == 0)
    {
        /* Denormalized floats are < 2^(1-127), not representable in 16-bit floats, rounding to zero. */
        return (deFloat16)sign;
    }
    /* Case: Inf and NaN */
    else if (exp32 == 0x000000ffu)
    {
        if (mantissa == 0u)
        {
            /* Inf */
            return (deFloat16)(sign | 0x7c00u);
        }
        else
        {
            /* NaN */
            mantissa >>= 13u; /* 16-bit floats has 10-bit for mantissa, 13-bit less than 32-bit floats. */
            /* Make sure we don't turn NaN into zero by | (mantissa == 0). */
            return (deFloat16)(sign | 0x7c00u | mantissa | (mantissa == 0u));
        }
    }
    /* The following are cases for normalized floats.
     *
     * * If exp16 is less than 0, we are experiencing underflow for the exponent. To encode this underflowed exponent,
     *   we can only shift the mantissa further right.
     *   The real exponent is exp16 - 15. A denormalized 16-bit float can represent -14 via its exponent.
     *   Note that the most significant bit in the mantissa of a denormalized float is already -1 as for exponent.
     *   So, we just need to right shift the mantissa -exp16 bits.
     * * If exp16 is 0, mantissa shifting requirement is similar to the above.
     * * If exp16 is greater than 30 (0b11110), we are experiencing overflow for the exponent of 16-bit normalized floats.
     */
    /* Case: normalized floats -> zero */
    else if (exp16 < -10)
    {
        /* 16-bit floats have only 10 bits for mantissa. Minimal 16-bit denormalized float is (2^-10) * (2^-14). */
        /* Expecting a number < (2^-10) * (2^-14) here, not representable, round to zero. */
        return (deFloat16)sign;
    }
    /* Case: normalized floats -> zero and denormalized halfs */
    else if (exp16 <= 0)
    {
        /* Add the implicit leading 1 in mormalized float to mantissa. */
        mantissa |= 0x00800000u;
        /* We have a (23 + 1)-bit mantissa, but 16-bit floats only expect 10-bit mantissa.
         * Need to discard the last 14-bits considering rounding mode.
         * We also need to shift right -exp16 bits to encode the underflowed exponent.
         */
        if (mode == DE_ROUNDINGMODE_TO_ZERO)
        {
            mantissa >>= (14 - exp16);
        }
        else
        {
            /* mantissa in the above may exceed 10-bits, in which case overflow happens.
             * The overflowed bit is automatically carried to exponent then.
             */
            mantissa = roundToNearestEven(mantissa, 14 - exp16);
        }
        return (deFloat16)(sign | mantissa);
    }
    /* Case: normalized floats -> normalized floats */
    else if (exp16 <= 30)
    {
        if (mode == DE_ROUNDINGMODE_TO_ZERO)
        {
            return (deFloat16)(sign | ((uint32_t)exp16 << 10u) | (mantissa >> 13u));
        }
        else
        {
            mantissa = roundToNearestEven(mantissa, 13);
            /* Handle overflow. exp16 may overflow (and become Inf) itself, but that's correct. */
            exp16 = (exp16 << 10u) + (mantissa & (1 << 10));
            mantissa &= (1u << 10) - 1;
            return (deFloat16)(sign | ((uint32_t)exp16) | mantissa);
        }
    }
    /* Case: normalized floats (too large to be representable as 16-bit floats) */
    else
    {
        /* According to IEEE Std 754-2008 Section 7.4,
         * * roundTiesToEven and roundTiesToAway carry all overflows to Inf with the sign
         *   of the intermediate  result.
         * * roundTowardZero carries all overflows to the format's largest finite number
         *   with the sign of the intermediate result.
         */
        if (mode == DE_ROUNDINGMODE_TO_ZERO)
        {
            return (deFloat16)(sign | 0x7bffu); /* 111 1011 1111 1111 */
        }
        else
        {
            return (deFloat16)(sign | (0x1f << 10));
        }
    }

    /* Make compiler happy */
    return (deFloat16)0;
}

/*--------------------------------------------------------------------*//*!
 * \brief Round the given number `val` to nearest even by discarding
 *        the last `numBitsToDiscard` bits.
 * \param val value to round
 * \param numBitsToDiscard number of (least significant) bits to discard
 * \return The rounded value with the last `numBitsToDiscard` removed
 *//*--------------------------------------------------------------------*/
static uint64_t roundToNearestEven64(uint64_t val, const uint64_t numBitsToDiscard)
{
    const uint64_t lastBits = val & (((uint64_t)1 << numBitsToDiscard) - 1);
    const uint64_t headBit  = val & ((uint64_t)1 << (numBitsToDiscard - 1));

    DE_ASSERT(numBitsToDiscard > 0 && numBitsToDiscard < 64); /* Make sure no overflow. */
    val >>= numBitsToDiscard;

    if (headBit == 0)
    {
        return val;
    }
    else if (headBit == lastBits)
    {
        if ((val & 0x1) == 0x1)
        {
            return val + 1;
        }
        else
        {
            return val;
        }
    }
    else
    {
        return val + 1;
    }
}

deFloat16 deFloat64To16Round(double val64, deRoundingMode mode)
{
    union
    {
        double f;   /* Interpret as 64-bit float */
        uint64_t u; /* Interpret as 64-bit unsigned integer */
    } x;
    uint64_t sign;  /* sign : 0000 0000 0000 0000 X000 0000 0000 0000 */
    uint64_t exp64; /* exp32: biased exponent for 64-bit floats */
    int exp16;      /* exp16: biased exponent for 16-bit floats */
    uint64_t mantissa;

    x.f      = val64;
    sign     = (x.u >> 48u) & 0x00008000u;
    exp64    = (x.u >> 52u) & 0x000007ffu;
    exp16    = (int)(exp64)-1023 + 15; /* 15/127: exponent bias for 16-bit/32-bit floats */
    mantissa = x.u & 0x00fffffffffffffu;

    /* Case: zero and denormalized floats */
    if (exp64 == 0)
    {
        /* Denormalized floats are < 2^(1-1023), not representable in 16-bit floats, rounding to zero. */
        return (deFloat16)sign;
    }
    /* Case: Inf and NaN */
    else if (exp64 == 0x000007ffu)
    {
        if (mantissa == 0u)
        {
            /* Inf */
            return (deFloat16)(sign | 0x7c00u);
        }
        else
        {
            /* NaN */
            mantissa >>= 42u; /* 16-bit floats has 10-bit for mantissa, 42-bit less than 64-bit floats. */
            /* Make sure we don't turn NaN into zero by | (mantissa == 0). */
            return (deFloat16)(sign | 0x7c00u | mantissa | (mantissa == 0u));
        }
    }
    /* The following are cases for normalized floats.
     *
     * * If exp16 is less than 0, we are experiencing underflow for the exponent. To encode this underflowed exponent,
     *   we can only shift the mantissa further right.
     *   The real exponent is exp16 - 15. A denormalized 16-bit float can represent -14 via its exponent.
     *   Note that the most significant bit in the mantissa of a denormalized float is already -1 as for exponent.
     *   So, we just need to right shift the mantissa -exp16 bits.
     * * If exp16 is 0, mantissa shifting requirement is similar to the above.
     * * If exp16 is greater than 30 (0b11110), we are experiencing overflow for the exponent of 16-bit normalized floats.
     */
    /* Case: normalized floats -> zero */
    else if (exp16 < -10)
    {
        /* 16-bit floats have only 10 bits for mantissa. Minimal 16-bit denormalized float is (2^-10) * (2^-14). */
        /* Expecting a number < (2^-10) * (2^-14) here, not representable, round to zero. */
        return (deFloat16)sign;
    }
    /* Case: normalized floats -> zero and denormalized halfs */
    else if (exp16 <= 0)
    {
        /* Add the implicit leading 1 in mormalized float to mantissa. */
        mantissa |= 0x0010000000000000u;
        /* We have a (23 + 1)-bit mantissa, but 16-bit floats only expect 10-bit mantissa.
         * Need to discard the last 14-bits considering rounding mode.
         * We also need to shift right -exp16 bits to encode the underflowed exponent.
         */
        if (mode == DE_ROUNDINGMODE_TO_ZERO)
        {
            mantissa >>= (43 - exp16);
        }
        else if (mode == DE_ROUNDINGMODE_TO_NEAREST_EVEN)
        {
            /* mantissa in the above may exceed 10-bits, in which case overflow happens.
             * The overflowed bit is automatically carried to exponent then.
             */
            mantissa = roundToNearestEven64(mantissa, 43 - exp16);
        }
        else
        {
            uint64_t discardedBits = mantissa & ((1ull << (43 - exp16)) - 1);
            mantissa >>= (43 - exp16);

            if (discardedBits != 0 && ((mode == DE_ROUNDINGMODE_TO_NEGATIVE_INF && sign != 0) ||
                                       (mode == DE_ROUNDINGMODE_TO_POSITIVE_INF && sign == 0)))
                mantissa++;
        }

        return (deFloat16)(sign | mantissa);
    }
    /* Case: normalized floats -> normalized floats */
    else if (exp16 <= 30)
    {
        if (mode == DE_ROUNDINGMODE_TO_ZERO)
            mantissa >>= 42u;
        else if (mode == DE_ROUNDINGMODE_TO_NEAREST_EVEN)
            mantissa = roundToNearestEven64(mantissa, 42);
        else
        {
            uint64_t discardedBits = mantissa & ((1ull << 42) - 1);
            mantissa >>= 42u;

            if (discardedBits != 0 && ((mode == DE_ROUNDINGMODE_TO_NEGATIVE_INF && sign != 0) ||
                                       (mode == DE_ROUNDINGMODE_TO_POSITIVE_INF && sign == 0)))
                mantissa++;
        }

        /* Handle overflow. exp16 may overflow (and become Inf) itself, but that's correct. */
        exp16 = (exp16 << 10u) + (deFloat16)(mantissa & (1 << 10));
        mantissa &= (1u << 10) - 1;
        return (deFloat16)(sign | ((uint32_t)exp16) | mantissa);
    }
    /* Case: normalized floats (too large to be representable as 16-bit floats) */
    else
    {
        /* According to IEEE Std 754-2008 Section 7.4,
         * * roundTiesToEven and roundTiesToAway carry all overflows to Inf with the sign
         *   of the intermediate  result.
         * * roundTowardZero carries all overflows to the format's largest finite number
         *   with the sign of the intermediate result.
         * * roundTowardNegative carries positive overflows to the format's largest finite number,
         *   and carries negative overflows to -Inf.
         * * roundTowardPositive carries negative overflows to the format's most negative finite
         *   number, and carries positive overflows to Inf.
         */
        if (mode == DE_ROUNDINGMODE_TO_ZERO || (mode == DE_ROUNDINGMODE_TO_NEGATIVE_INF && sign == 0) ||
            (mode == DE_ROUNDINGMODE_TO_POSITIVE_INF && sign != 0))
        {
            return (deFloat16)(sign | 0x7bffu); /* 111 1011 1111 1111 */
        }
        else
        {
            return (deFloat16)(sign | (0x1f << 10));
        }
    }

    /* Make compiler happy */
    return (deFloat16)0;
}

float deFloat16To32(deFloat16 val16)
{
    uint32_t sign;
    uint32_t expotent;
    uint32_t mantissa;
    union
    {
        float f;
        uint32_t u;
    } x;

    x.u = 0u;

    sign     = ((uint32_t)val16 >> 15u) & 0x00000001u;
    expotent = ((uint32_t)val16 >> 10u) & 0x0000001fu;
    mantissa = (uint32_t)val16 & 0x000003ffu;

    if (expotent == 0u)
    {
        if (mantissa == 0u)
        {
            /* +/- 0 */
            x.u = sign << 31u;
            return x.f;
        }
        else
        {
            /* Denormalized, normalize it. */

            while (!(mantissa & 0x00000400u))
            {
                mantissa <<= 1u;
                expotent -= 1u;
            }

            expotent += 1u;
            mantissa &= ~0x00000400u;
        }
    }
    else if (expotent == 31u)
    {
        if (mantissa == 0u)
        {
            /* +/- InF */
            x.u = (sign << 31u) | 0x7f800000u;
            return x.f;
        }
        else
        {
            /* +/- NaN */
            x.u = (sign << 31u) | 0x7f800000u | (mantissa << 13u);
            return x.f;
        }
    }

    expotent = expotent + (127u - 15u);
    mantissa = mantissa << 13u;

    x.u = (sign << 31u) | (expotent << 23u) | mantissa;
    return x.f;
}

double deFloat16To64(deFloat16 val16)
{
    uint64_t sign;
    uint64_t expotent;
    uint64_t mantissa;
    union
    {
        double f;
        uint64_t u;
    } x;

    x.u = 0u;

    sign     = ((uint32_t)val16 >> 15u) & 0x00000001u;
    expotent = ((uint32_t)val16 >> 10u) & 0x0000001fu;
    mantissa = (uint32_t)val16 & 0x000003ffu;

    if (expotent == 0u)
    {
        if (mantissa == 0u)
        {
            /* +/- 0 */
            x.u = sign << 63u;
            return x.f;
        }
        else
        {
            /* Denormalized, normalize it. */

            while (!(mantissa & 0x00000400u))
            {
                mantissa <<= 1u;
                expotent -= 1u;
            }

            expotent += 1u;
            mantissa &= ~0x00000400u;
        }
    }
    else if (expotent == 31u)
    {
        if (mantissa == 0u)
        {
            /* +/- InF */
            x.u = (sign << 63u) | 0x7ff0000000000000u;
            return x.f;
        }
        else
        {
            /* +/- NaN */
            x.u = (sign << 63u) | 0x7ff0000000000000u | (mantissa << 42u);
            return x.f;
        }
    }

    expotent = expotent + (1023u - 15u);
    mantissa = mantissa << 42u;

    x.u = (sign << 63u) | (expotent << 52u) | mantissa;
    return x.f;
}

DE_END_EXTERN_C
