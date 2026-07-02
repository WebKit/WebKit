#ifndef _DEFLOAT16_H
#define _DEFLOAT16_H
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

#include "deDefs.h"
#include "deMath.h"

DE_BEGIN_EXTERN_C

typedef uint16_t deFloat16;

#if defined(DE_DEPRECATED_TYPES)
typedef deFloat16 DEfloat16;
#endif

/*--------------------------------------------------------------------*//*!
 * \brief Convert 32-bit floating point number to 16 bit.
 * \param val32    Input value.
 * \return Converted 16-bit floating-point value.
 *//*--------------------------------------------------------------------*/
deFloat16 deFloat32To16(float val32);
deFloat16 deFloat32To16Round(float val32, deRoundingMode mode);
void deFloat16_selfTest(void);

deFloat16 deFloat64To16(double val64);
deFloat16 deFloat64To16Round(double val64, deRoundingMode mode);

/*--------------------------------------------------------------------*//*!
 * \brief Convert 16-bit floating point number to 32 bit.
 * \param val16    Input value.
 * \return Converted 32-bit floating-point value.
 *//*--------------------------------------------------------------------*/
float deFloat16To32(deFloat16 val16);

/*--------------------------------------------------------------------*//*!
 * \brief Convert 16-bit floating point number to 64 bit.
 * \param val16    Input value.
 * \return Converted 64-bit floating-point value.
 *//*--------------------------------------------------------------------*/
double deFloat16To64(deFloat16 val16);

static inline uint16_t deHalfExponent(deFloat16 x)
{
    return (uint16_t)((x & 0x7c00u) >> 10);
}

static inline uint16_t deHalfMantissa(deFloat16 x)
{
    return (uint16_t)(x & 0x03ffu);
}

static inline uint16_t deHalfHighestMantissaBit(deFloat16 x)
{
    return (uint16_t)(x & (1u << 9));
}

static inline uint16_t deHalfSign(deFloat16 x)
{
    return (uint16_t)(x >> 15);
}

static const uint16_t deHalfMaxExponent = 0x1f;

static inline bool deHalfIsZero(deFloat16 x)
{
    return deHalfExponent(x) == 0 && deHalfMantissa(x) == 0;
}

static inline bool deHalfIsPositiveZero(deFloat16 x)
{
    return deHalfIsZero(x) && (deHalfSign(x) == 0);
}

static inline bool deHalfIsNegativeZero(deFloat16 x)
{
    return deHalfIsZero(x) && (deHalfSign(x) != 0);
}

static inline bool deHalfIsIEEENaN(deFloat16 x)
{
    return deHalfExponent(x) == deHalfMaxExponent && deHalfMantissa(x) != 0;
}

static inline bool deHalfIsSignalingNaN(deFloat16 x)
{
    return deHalfIsIEEENaN(x) && deHalfHighestMantissaBit(x) == 0;
}

static inline bool deHalfIsQuietNaN(deFloat16 x)
{
    return deHalfIsIEEENaN(x) && deHalfHighestMantissaBit(x) != 0;
}

static inline bool deHalfIsInf(deFloat16 x)
{
    return deHalfExponent(x) == deHalfMaxExponent && deHalfMantissa(x) == 0;
}

static inline bool deHalfIsPositiveInf(deFloat16 x)
{
    return deHalfIsInf(x) && (deHalfSign(x) == 0);
}

static inline bool deHalfIsNegativeInf(deFloat16 x)
{
    return deHalfIsInf(x) && (deHalfSign(x) != 0);
}

static inline bool deHalfIsDenormal(deFloat16 x)
{
    return deHalfExponent(x) == 0 && deHalfMantissa(x) != 0;
}

DE_END_EXTERN_C

#endif /* _DEFLOAT16_H */
