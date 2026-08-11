/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Testing of deMath functions.
 *//*--------------------------------------------------------------------*/

#include "deMath.h"
#include "deRandom.h"

DE_BEGIN_EXTERN_C

static bool conversionToFloatLosesPrecision(int32_t x)
{
    if (x == -0x7FFFFFFF - 1)
        return false;
    else if (x < 0)
        return conversionToFloatLosesPrecision(-x);
    else if (x == 0)
        return false;
    else if (((uint32_t)x & 0x1) == 0)
        return conversionToFloatLosesPrecision(x >> 1); /* remove trailing zeros */
    else
        return x > ((1 << 24) - 1); /* remaining part does not fit in the mantissa? */
}

static void testSingleInt32ToFloat(int32_t x)
{
    /* roundTowardsToNegInf(x) <= round(x) <= roundTowardsPosInf(x). */
    /* \note: Need to use inequalities since round(x) returns arbitrary precision floats. */
    DE_TEST_ASSERT(deInt32ToFloatRoundToNegInf(x) <= deInt32ToFloat(x));
    DE_TEST_ASSERT(deInt32ToFloat(x) <= deInt32ToFloatRoundToPosInf(x));

    /* if precision is lost, floor(x) < ceil(x). Else floor(x) == ceil(x) */
    if (conversionToFloatLosesPrecision(x))
        DE_TEST_ASSERT(deInt32ToFloatRoundToNegInf(x) < deInt32ToFloatRoundToPosInf(x));
    else
        DE_TEST_ASSERT(deInt32ToFloatRoundToNegInf(x) == deInt32ToFloatRoundToPosInf(x));

    /* max one ulp from each other */
    if (deInt32ToFloatRoundToNegInf(x) < deInt32ToFloatRoundToPosInf(x))
    {
        union
        {
            float f;
            int32_t u;
        } v0, v1;

        v0.f = deInt32ToFloatRoundToNegInf(x);
        v1.f = deInt32ToFloatRoundToPosInf(x);

        DE_TEST_ASSERT(v0.u + 1 == v1.u || v0.u == v1.u + 1);
    }
}

static void testInt32ToFloat(void)
{
    const int numIterations = 2500000;

    int sign;
    int numBits;
    int delta;
    int ndx;
    deRandom rnd;

    deRandom_init(&rnd, 0xdeadbeefu - 1);

    for (sign = -1; sign < 1; ++sign)
        for (numBits = 0; numBits < 32; ++numBits)
            for (delta = -2; delta < 3; ++delta)
            {
                const int64_t x = (int64_t)(sign == -1 ? (-1) : (+1)) * (1LL << (int64_t)numBits) + (int64_t)delta;

                /* would overflow */
                if (x > 0x7FFFFFFF || x < -0x7FFFFFFF - 1)
                    continue;

                testSingleInt32ToFloat((int32_t)x);
            }

    for (ndx = 0; ndx < numIterations; ++ndx)
        testSingleInt32ToFloat((int32_t)deRandom_getUint32(&rnd));
}

void deMath_selfTest(void)
{
    /* Test Int32ToFloat*(). */
    testInt32ToFloat();
}

DE_END_EXTERN_C
