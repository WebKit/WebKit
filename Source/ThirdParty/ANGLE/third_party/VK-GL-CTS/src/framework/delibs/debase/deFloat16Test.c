/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
 *
 * Copyright 2017 Google Inc.
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
 * \brief Testing of deFloat16 functions.
 *//*--------------------------------------------------------------------*/

#include "deFloat16.h"
#include "deRandom.h"

DE_BEGIN_EXTERN_C

static float getFloat32(uint32_t sign, uint32_t biased_exponent, uint32_t mantissa)
{
    union
    {
        float f;
        uint32_t u;
    } x;

    x.u = (sign << 31) | (biased_exponent << 23) | mantissa;

    return x.f;
}

static deFloat16 getFloat16(uint16_t sign, uint16_t biased_exponent, uint16_t mantissa)
{
    return (deFloat16)((sign << 15) | (biased_exponent << 10) | mantissa);
}

static deFloat16 deFloat32To16RTZ(float val32)
{
    return deFloat32To16Round(val32, DE_ROUNDINGMODE_TO_ZERO);
}

static deFloat16 deFloat32To16RTE(float val32)
{
    return deFloat32To16Round(val32, DE_ROUNDINGMODE_TO_NEAREST_EVEN);
}

void deFloat16_selfTest(void)
{
    /* 16-bit: 1    5 (0x00--0x1f)    10 (0x000--0x3ff)
     * 32-bit: 1    8 (0x00--0xff)    23 (0x000000--0x7fffff)
     */
    deRandom rnd;
    int idx;

    deRandom_init(&rnd, 0xdeadbeefu - 1);

    /* --- For rounding mode RTZ --- */

    /* Zero */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0, 0)) == getFloat16(0, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0, 0)) == getFloat16(1, 0, 0));

    /* Inf */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0)) == getFloat16(0, 0x1f, 0));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0)) == getFloat16(1, 0x1f, 0));

    /* SNaN */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 1)) == getFloat16(0, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 1)) == getFloat16(1, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x3fffff)) == getFloat16(0, 0x1f, 0x1ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x3fffff)) == getFloat16(1, 0x1f, 0x1ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x0003ff)) == getFloat16(0, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x0003ff)) == getFloat16(1, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x123456)) == getFloat16(0, 0x1f, 0x123456 >> 13));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x123456)) == getFloat16(1, 0x1f, 0x123456 >> 13));

    /* QNaN */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x400000)) == getFloat16(0, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x400000)) == getFloat16(1, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x7fffff)) == getFloat16(0, 0x1f, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x7fffff)) == getFloat16(1, 0x1f, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x4003ff)) == getFloat16(0, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x4003ff)) == getFloat16(1, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0xff, 0x723456)) == getFloat16(0, 0x1f, 0x723456 >> 13));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0xff, 0x723456)) == getFloat16(1, 0x1f, 0x723456 >> 13));

    /* Denormalized */
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t mantissa = deRandom_getUint32(&rnd);

        mantissa &= 0x7fffffu;       /* Take the last 23 bits */
        mantissa |= (mantissa == 0); /* Make sure it is not zero */

        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 0, mantissa)) == getFloat16(0, 0, 0));
        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 0, mantissa)) == getFloat16(1, 0, 0));
    }

    /* Normalized -> zero */
    /* Absolute value: minimal 32-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 1, 0)) == getFloat16(0, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 1, 0)) == getFloat16(1, 0, 0));
    /* Absolute value: 2^-24 - e, extremely near minimal 16-bit denormalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 25, 0x7fffff)) == getFloat16(0, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 25, 0x7fffff)) == getFloat16(1, 0, 0));
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t exponent = deRandom_getUint32(&rnd);
        uint32_t mantissa = deRandom_getUint32(&rnd);

        exponent = exponent % (127 - 25) + 1; /* Make sure >= 1, <= 127 - 25 */
        mantissa &= 0x7fffffu;                /* Take the last 23 bits */

        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, exponent, mantissa)) == getFloat16(0, 0, 0));
        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, exponent, mantissa)) == getFloat16(1, 0, 0));
    }

    /* Normalized -> denormalized */
    /* Absolute value: 2^-24, minimal 16-bit denormalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 24, 0)) == getFloat16(0, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 24, 0)) == getFloat16(1, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 24, 1)) == getFloat16(0, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 24, 1)) == getFloat16(1, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 20, 0x123456)) == getFloat16(0, 0, 0x12));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 20, 0x123456)) == getFloat16(1, 0, 0x12));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 18, 0x654321)) == getFloat16(0, 0, 0x72));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 18, 0x654321)) == getFloat16(1, 0, 0x72));
    /* Absolute value: 2^-14 - 2^-24 = (2 - 2^-9) * 2^-15, maximal 16-bit denormalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 15, 0x7fc000)) ==
                   getFloat16(0, 0, 0x3ff)); /* 0x7fc000: 0111 1111 1100 0000 0000 0000 */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 15, 0x7fc000)) == getFloat16(1, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 15, 0x7fc000 - 1)) == getFloat16(0, 0, 0x3fe));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 15, 0x7fc000 - 1)) == getFloat16(1, 0, 0x3fe));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 15, 0x7fc000 + 1)) == getFloat16(0, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 15, 0x7fc000 + 1)) == getFloat16(1, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 15, 0x7fffff)) == getFloat16(0, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 15, 0x7fffff)) == getFloat16(1, 0, 0x3ff));

    /* Normalized -> normalized */
    /* Absolute value: 2^-14, minimal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 - 14, 0)) == getFloat16(0, 1, 0));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 - 14, 0)) == getFloat16(1, 1, 0));
    /* Absolute value: 65504 - 2^-23, extremely near maximal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 + 15, (0x3ff << 13) - 1)) == getFloat16(0, 0x1e, 0x3fe));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 + 15, (0x3ff << 13) - 1)) == getFloat16(1, 0x1e, 0x3fe));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 + 15, (0x3ff << 13) - 0x456)) == getFloat16(0, 0x1e, 0x3fe));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 + 15, (0x3ff << 13) - 0x456)) == getFloat16(1, 0x1e, 0x3fe));
    /* Absolute value: 65504, maximal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 + 15, 0x3ff << 13)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 + 15, 0x3ff << 13)) == getFloat16(1, 0x1e, 0x3ff));
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t exponent = deRandom_getUint32(&rnd);
        uint32_t mantissa = deRandom_getUint32(&rnd);

        exponent = exponent % ((127 + 14) - (127 - 14) + 1) + (127 - 14); /* Make sure >= 127 - 14, <= 127 + 14 */
        mantissa &= 0x7fffffu;                                            /* Take the last 23 bits */

        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, exponent, mantissa)) ==
                       getFloat16(0, (uint16_t)(exponent + 15 - 127), (uint16_t)(mantissa >> 13)));
        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, exponent, mantissa)) ==
                       getFloat16(1, (uint16_t)(exponent + 15 - 127), (uint16_t)(mantissa >> 13)));
    }

    /* Normalized -> minimal/maximal normalized */
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 + 15, (0x3ff << 13) + 1)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 + 15, (0x3ff << 13) + 1)) == getFloat16(1, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, 127 + 15, (0x3ff << 13) + 0x123)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, 127 + 15, (0x3ff << 13) + 0x123)) == getFloat16(1, 0x1e, 0x3ff));
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t exponent = deRandom_getUint32(&rnd);
        uint32_t mantissa = deRandom_getUint32(&rnd);

        exponent = exponent % (0xfe - (127 + 16) + 1) + (127 + 16); /* Make sure >= 127 + 16, <= 0xfe */
        mantissa &= 0x7fffffu;                                      /* Take the last 23 bits */

        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(0, exponent, mantissa)) == getFloat16(0, 0x1e, 0x3ff));
        DE_TEST_ASSERT(deFloat32To16RTZ(getFloat32(1, exponent, mantissa)) == getFloat16(1, 0x1e, 0x3ff));
    }

    /* --- For rounding mode RTE --- */

    /* Zero */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0, 0)) == getFloat16(0, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0, 0)) == getFloat16(1, 0, 0));

    /* Inf */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0)) == getFloat16(0, 0x1f, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0)) == getFloat16(1, 0x1f, 0));

    /* SNaN */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 1)) == getFloat16(0, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 1)) == getFloat16(1, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x3fffff)) == getFloat16(0, 0x1f, 0x1ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x3fffff)) == getFloat16(1, 0x1f, 0x1ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x0003ff)) == getFloat16(0, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x0003ff)) == getFloat16(1, 0x1f, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x123456)) == getFloat16(0, 0x1f, 0x123456 >> 13));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x123456)) == getFloat16(1, 0x1f, 0x123456 >> 13));

    /* QNaN */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x400000)) == getFloat16(0, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x400000)) == getFloat16(1, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x7fffff)) == getFloat16(0, 0x1f, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x7fffff)) == getFloat16(1, 0x1f, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x4003ff)) == getFloat16(0, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x4003ff)) == getFloat16(1, 0x1f, 0x200));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0xff, 0x723456)) == getFloat16(0, 0x1f, 0x723456 >> 13));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0xff, 0x723456)) == getFloat16(1, 0x1f, 0x723456 >> 13));

    /* Denormalized */
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t mantissa = deRandom_getUint32(&rnd);

        mantissa &= 0x7fffffu;       /* Take the last 23 bits */
        mantissa |= (mantissa == 0); /* Make sure it is not zero */

        DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 0, mantissa)) == getFloat16(0, 0, 0));
        DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 0, mantissa)) == getFloat16(1, 0, 0));
    }

    /* Normalized -> zero and denormalized */
    /* Absolute value: minimal 32-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 1, 0)) == getFloat16(0, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 1, 0)) == getFloat16(1, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 42, 0x7abcde)) == getFloat16(0, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 42, 0x7abcde)) == getFloat16(1, 0, 0));
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t exponent = deRandom_getUint32(&rnd);
        uint32_t mantissa = deRandom_getUint32(&rnd);

        exponent = exponent % (127 - 26) + 1; /* Make sure >= 1, <= 127 - 26 */
        mantissa &= 0x7fffffu;                /* Take the last 23 bits */

        DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, exponent, mantissa)) == getFloat16(0, 0, 0));
        DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, exponent, mantissa)) == getFloat16(1, 0, 0));
    }
    /* Absolute value: 2^-25, minimal 16-bit denormalized: 2^-24 */
    /* The following six cases need to right shift mantissa (with leading 1) 10 bits  --------------------> to here */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 25, 0)) ==
                   getFloat16(0, 0, 0)); /* XX XXXX XXXX 1 000 0000 0000 0000 0000 0000 */
    /*                                                    Take the first 10 bits with RTE ------ 00 0000 0000 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 25, 0)) == getFloat16(1, 0, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 25, 1)) ==
                   getFloat16(0, 0, 1)); /* XX XXXX XXXX 1 000 0000 0000 0000 0000 0001 */
    /*                                                    Take the first 10 bits with RTE ------ 00 0000 0001 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 25, 1)) == getFloat16(1, 0, 1));
    /* Absolute value: 2^-24 - e, extremely near minimal 16-bit denormalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 25, 0x7fffff)) == getFloat16(0, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 25, 0x7fffff)) == getFloat16(1, 0, 1));
    /* Absolute value: 2^-24, minimal 16-bit denormalized */
    /* The following (127 - 24) cases need to right shift mantissa (with leading 1) 9 bits  -----------------> to here */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 24, 0)) ==
                   getFloat16(0, 0, 1)); /* X XXXX XXXX 1 000 0000 0000 0000 0000 0000 */
    /*                                                    Take the first 10 bits with RTE ---------- 0 0000 0000 1 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 24, 0)) == getFloat16(1, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 24, 1)) ==
                   getFloat16(0, 0, 1)); /* X XXXX XXXX 1 000 0000 0000 0000 0000 0001 */
    /*                                                    Take the first 10 bits with RTE ---------- 0 0000 0000 1 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 24, 1)) == getFloat16(1, 0, 1));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 24, 0x400000)) ==
                   getFloat16(0, 0, 2)); /* X XXXX XXXX 1 100 0000 0000 0000 0000 0000 */
    /*                                                    Take the first 10 bits with RTE ---------- 0 0000 0000 2 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 24, 0x400000)) == getFloat16(1, 0, 2));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 24, 0x400001)) ==
                   getFloat16(0, 0, 2)); /* X XXXX XXXX 1 100 0000 0000 0000 0000 0001 */
    /*                                                    Take the first 10 bits with RTE ---------- 0 0000 0000 2 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 24, 0x400001)) == getFloat16(1, 0, 2));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 24, 0x4fffff)) == getFloat16(0, 0, 2));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 24, 0x4fffff)) == getFloat16(1, 0, 2));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 20, 0x123456)) == getFloat16(0, 0, 0x12));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 20, 0x123456)) == getFloat16(1, 0, 0x12));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 18, 0x654321)) == getFloat16(0, 0, 0x73));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 18, 0x654321)) == getFloat16(1, 0, 0x73));
    /* Absolute value: 2^-14 - 2^-24 = (2 - 2^-9) * 2^-15, maximal 16-bit denormalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 15, 0x7fc000)) ==
                   getFloat16(0, 0, 0x3ff)); /* 0x7fc000: 0111 1111 1100 0000 0000 0000 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 15, 0x7fc000)) == getFloat16(1, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 15, 0x7fc000 - 1)) == getFloat16(0, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 15, 0x7fc000 - 1)) == getFloat16(1, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 15, 0x7fc000 + 1)) == getFloat16(0, 0, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 15, 0x7fc000 + 1)) == getFloat16(1, 0, 0x3ff));

    /* Normalized -> normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 15, 0x7fe000)) ==
                   getFloat16(0, 1, 0)); /* 0x7fe000: 0111 1111 1110 0000 0000 0000 */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 15, 0x7fe000)) == getFloat16(1, 1, 0));
    /* Absolute value: (2 - 2^-23) * 2^-15, extremely near 2^-14, minimal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 15, 0x7fffff)) == getFloat16(0, 1, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 15, 0x7fffff)) == getFloat16(1, 1, 0));
    /* Absolute value: 2^-14, minimal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 - 14, 0)) == getFloat16(0, 1, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 - 14, 0)) == getFloat16(1, 1, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3fe << 13) + (1 << 12))) == getFloat16(0, 0x1e, 0x3fe));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3fe << 13) + (1 << 12))) == getFloat16(1, 0x1e, 0x3fe));

    /* Normalized -> minimal/maximal normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3fe << 13) + (1 << 12) + 1)) ==
                   getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3fe << 13) + (1 << 12) + 1)) ==
                   getFloat16(1, 0x1e, 0x3ff));
    /* Absolute value: 65504 - 2^-23, extremely near maximal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3ff << 13) - 1)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3ff << 13) - 1)) == getFloat16(1, 0x1e, 0x3ff));
    /* Absolute value: 65504, maximal 16-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, 0x3ff << 13)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, 0x3ff << 13)) == getFloat16(1, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3ff << 13) + 1)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3ff << 13) + 1)) == getFloat16(1, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3ff << 13) + 0x456)) == getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3ff << 13) + 0x456)) == getFloat16(1, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3ff << 13) + (1 << 12) - 1)) ==
                   getFloat16(0, 0x1e, 0x3ff));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3ff << 13) + (1 << 12) - 1)) ==
                   getFloat16(1, 0x1e, 0x3ff));

    /* Normalized -> Inf */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, (0x3ff << 13) + (1 << 12))) == getFloat16(0, 0x1f, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, (0x3ff << 13) + (1 << 12))) == getFloat16(1, 0x1f, 0));
    /* Absolute value: maximal 32-bit normalized */
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, 127 + 15, 0x7fffff)) == getFloat16(0, 0x1f, 0));
    DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, 127 + 15, 0x7fffff)) == getFloat16(1, 0x1f, 0));
    for (idx = 0; idx < 256; ++idx)
    {
        uint32_t exponent = deRandom_getUint32(&rnd);
        uint32_t mantissa = deRandom_getUint32(&rnd);

        exponent = exponent % (0xfe - (127 + 16) + 1) + (127 + 16); /* Make sure >= 127 + 16, <= 0xfe */
        mantissa &= 0x7fffffu;                                      /* Take the last 23 bits */

        DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(0, exponent, mantissa)) == getFloat16(0, 0x1f, 0));
        DE_TEST_ASSERT(deFloat32To16RTE(getFloat32(1, exponent, mantissa)) == getFloat16(1, 0x1f, 0));
    }
}

DE_END_EXTERN_C
