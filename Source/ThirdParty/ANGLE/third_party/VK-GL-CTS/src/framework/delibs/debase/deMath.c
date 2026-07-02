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
 * \brief Basic mathematical operations.
 *//*--------------------------------------------------------------------*/

#include "deMath.h"
#include "deInt32.h"

#if (DE_COMPILER == DE_COMPILER_MSC)
#include <float.h>
#endif

#if (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
#include <fenv.h>
#endif

deRoundingMode deGetRoundingMode(void)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    unsigned int status = 0;
    int ret;

    ret = _controlfp_s(&status, 0, 0);
    DE_ASSERT(ret == 0);

    switch (status & _MCW_RC)
    {
    case _RC_CHOP:
        return DE_ROUNDINGMODE_TO_ZERO;
    case _RC_UP:
        return DE_ROUNDINGMODE_TO_POSITIVE_INF;
    case _RC_DOWN:
        return DE_ROUNDINGMODE_TO_NEGATIVE_INF;
    case _RC_NEAR:
        return DE_ROUNDINGMODE_TO_NEAREST_EVEN;
    default:
        return DE_ROUNDINGMODE_LAST;
    }
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    int mode = fegetround();
    switch (mode)
    {
    case FE_TOWARDZERO:
        return DE_ROUNDINGMODE_TO_ZERO;
    case FE_UPWARD:
        return DE_ROUNDINGMODE_TO_POSITIVE_INF;
    case FE_DOWNWARD:
        return DE_ROUNDINGMODE_TO_NEGATIVE_INF;
    case FE_TONEAREST:
        return DE_ROUNDINGMODE_TO_NEAREST_EVEN;
    default:
        return DE_ROUNDINGMODE_LAST;
    }
#else
#error Implement deGetRoundingMode().
#endif
}

bool deSetRoundingMode(deRoundingMode mode)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    unsigned int flag = 0;
    unsigned int oldState;
    int ret;

    switch (mode)
    {
    case DE_ROUNDINGMODE_TO_ZERO:
        flag = _RC_CHOP;
        break;
    case DE_ROUNDINGMODE_TO_POSITIVE_INF:
        flag = _RC_UP;
        break;
    case DE_ROUNDINGMODE_TO_NEGATIVE_INF:
        flag = _RC_DOWN;
        break;
    case DE_ROUNDINGMODE_TO_NEAREST_EVEN:
        flag = _RC_NEAR;
        break;
    default:
        DE_ASSERT(false);
    }

    ret = _controlfp_s(&oldState, flag, _MCW_RC);
    return ret == 0;
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    int flag = 0;
    int ret;

    switch (mode)
    {
    case DE_ROUNDINGMODE_TO_ZERO:
        flag = FE_TOWARDZERO;
        break;
    case DE_ROUNDINGMODE_TO_POSITIVE_INF:
        flag = FE_UPWARD;
        break;
    case DE_ROUNDINGMODE_TO_NEGATIVE_INF:
        flag = FE_DOWNWARD;
        break;
    case DE_ROUNDINGMODE_TO_NEAREST_EVEN:
        flag = FE_TONEAREST;
        break;
    default:
        DE_ASSERT(false);
    }

    ret = fesetround(flag);
    return ret == 0;
#else
#error Implement deSetRoundingMode().
#endif
}

double deFractExp(double x, int *exponent)
{
    if (isinf(x))
    {
        *exponent = 0;
        return x;
    }
    else
    {
        int tmpExp   = 0;
        double fract = frexp(x, &tmpExp);
        *exponent    = tmpExp - 1;
        return fract * 2.0;
    }
}

/* We could use frexpf, if available. */
float deFloatFractExp(float x, int *exponent)
{
    return (float)deFractExp(x, exponent);
}

double deRoundEven(double a)
{
    double integer;
    double fract = modf(a, &integer);
    if (fabs(fract) == 0.5)
        return 2.0 * deRound(a / 2.0);
    return deRound(a);
}

float deInt32ToFloatRoundToNegInf(int32_t x)
{
    /* \note Sign bit is separate so the range is symmetric */
    if (x >= -0xFFFFFF && x <= 0xFFFFFF)
    {
        /* 24 bits are representable (23 mantissa + 1 implicit). */
        return (float)x;
    }
    else if (x != -0x7FFFFFFF - 1)
    {
        /* we are losing bits */
        const int exponent      = 31 - deClz32((uint32_t)deAbs32(x));
        const int numLostBits   = exponent - 23;
        const uint32_t lostMask = deBitMask32(0, numLostBits);

        DE_ASSERT(numLostBits > 0);

        if (x > 0)
        {
            /* Mask out lost bits to floor to a representable value */
            return (float)(int32_t)(~lostMask & (uint32_t)x);
        }
        else if ((lostMask & (uint32_t)-x) == 0u)
        {
            /* this was a representable value */
            DE_ASSERT((int32_t)(float)x == x);
            return (float)x;
        }
        else
        {
            /* not representable, choose the next lower */
            const float nearestHigher = (float)-(int32_t)(~lostMask & (uint32_t)-x);
            const float oneUlp        = (float)(1u << (uint32_t)numLostBits);
            const float nearestLower  = nearestHigher - oneUlp;

            /* check sanity */
            DE_ASSERT((int32_t)(float)nearestHigher > (int32_t)(float)nearestLower);

            return nearestLower;
        }
    }
    else
        return -(float)0x80000000u;
}

float deInt32ToFloatRoundToPosInf(int32_t x)
{
    if (x == -0x7FFFFFFF - 1)
        return -(float)0x80000000u;
    else
        return -deInt32ToFloatRoundToNegInf(-x);
}
