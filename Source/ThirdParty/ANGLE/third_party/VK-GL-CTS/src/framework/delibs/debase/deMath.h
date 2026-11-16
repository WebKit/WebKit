#ifndef _DEMATH_H
#define _DEMATH_H
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

#include "deDefs.h"
#include "deMemory.h"

#include <math.h>
#include <float.h>

DE_BEGIN_EXTERN_C

/* Mathematical constants. */

#define DE_PI 3.14159265358979324f            /*!< Pi.                    */
#define DE_LOG_2 0.69314718056f               /*!< log_e(2.0)                */
#define DE_INV_LOG_2 1.44269504089f           /*!< 1.0 / log_e(2.0)        */
#define DE_E 2.71828182845904523536f          /*!< e.                        */
#define DE_LOG2_E 1.44269504088896340736f     /*!< log_2(e).                */
#define DE_INV_LOG2_E 0.69314718055994530942f /*!< 1.0 / log_2(e).        */

#define DE_PI_DOUBLE 3.14159265358979323846 /*!< Pi as a double.        */
#define DE_PI_16BIT 0x4248                  /*!< Pi. as a float16b        */

/* Rounding mode control. */

typedef enum deRoundingMode_e
{
    DE_ROUNDINGMODE_TO_NEAREST_EVEN = 0,
    DE_ROUNDINGMODE_TO_ZERO,
    DE_ROUNDINGMODE_TO_POSITIVE_INF,
    DE_ROUNDINGMODE_TO_NEGATIVE_INF,

    DE_ROUNDINGMODE_LAST
} deRoundingMode;

deRoundingMode deGetRoundingMode(void);
bool deSetRoundingMode(deRoundingMode mode);

void deMath_selfTest(void);

/* Float properties */

static inline uint32_t deFloatBitsToUint32(float x)
{
    uint32_t bits;
    deMemcpy((void *)&bits, (void *)&x, 4);
    return bits;
}

static inline float deUint32BitsToFloat(uint32_t x)
{
    float bits;
    deMemcpy((void *)&bits, (void *)&x, 4);
    return bits;
}

static inline uint64_t deDoubleBitsToUint64(double x)
{
    uint64_t bits;
    deMemcpy((void *)&bits, (void *)&x, 8);
    return bits;
}

static inline bool deFloatIsPositiveZero(float x)
{
    return x == 0 && (deFloatBitsToUint32(x) >> 31) == 0;
}

static inline bool deDoubleIsPositiveZero(double x)
{
    return x == 0 && (deDoubleBitsToUint64(x) >> 63) == 0;
}

static inline bool deFloatIsNegativeZero(float x)
{
    return x == 0 && (deFloatBitsToUint32(x) >> 31) != 0;
}

static inline bool deDoubleIsNegativeZero(double x)
{
    return x == 0 && (deDoubleBitsToUint64(x) >> 63) != 0;
}

static inline bool deFloatIsIEEENaN(float x)
{
    uint32_t e = (deFloatBitsToUint32(x) & 0x7f800000u) >> 23;
    uint32_t m = (deFloatBitsToUint32(x) & 0x007fffffu);
    return e == 0xff && m != 0;
}

static inline bool deDoubleIsIEEENaN(double x)
{
    uint64_t e = (deDoubleBitsToUint64(x) & 0x7ff0000000000000ull) >> 52;
    uint64_t m = (deDoubleBitsToUint64(x) & 0x000fffffffffffffull);
    return e == 0x7ff && m != 0;
}

/* \note The definition used for signaling NaN here is valid for ARM and
 * x86 but possibly not for other platforms.
 *
 * These are defined as overloads so that they can be used in templated
 * code without risking a type conversion which would triggern an exception
 * on a signaling NaN.  We don't use deIsNan in these helpers because they
 * do a comparison operation which may also trigger exceptions.
 */
static inline bool deFloatIsSignalingNaN(float x)
{
    return deFloatIsIEEENaN(x) && (deFloatBitsToUint32(x) & (1u << 22)) == 0;
}

static inline bool deDoubleIsSignalingNaN(double x)
{
    return deDoubleIsIEEENaN(x) && (deDoubleBitsToUint64(x) & (1ull << 51)) == 0;
}

static inline bool deFloatIsQuietNaN(float x)
{
    return deFloatIsIEEENaN(x) && (deFloatBitsToUint32(x) & (1u << 22)) != 0;
}

static inline bool deDoubleIsQuietNaN(double x)
{
    return deDoubleIsIEEENaN(x) && (deDoubleBitsToUint64(x) & (1ull << 51)) != 0;
}

/* Basic utilities. */

static inline float deFloatAbs(float x)
{
    return (x >= 0.0f) ? x : -x;
}
static inline float deFloatMin(float a, float b)
{
    return (a <= b) ? a : b;
}
static inline float deFloatMax(float a, float b)
{
    return (a >= b) ? a : b;
}
static inline float deFloatClamp(float x, float mn, float mx)
{
    return (x <= mn) ? mn : ((x >= mx) ? mx : x);
}

static inline double deAbs(double x)
{
    return (x >= 0.0) ? x : -x;
}
static inline double deMin(double a, double b)
{
    return (a <= b) ? a : b;
}
static inline double deMax(double a, double b)
{
    return (a >= b) ? a : b;
}
static inline double deClamp(double x, double mn, double mx)
{
    return (x <= mn) ? mn : ((x >= mx) ? mx : x);
}

/* Utility functions. */

static inline float deFloatSign(float a)
{
    return (a == 0.0f) ? 0.0f : ((a > 0.0f) ? +1.0f : -1.0f);
}
static inline int deFloatIntSign(float a)
{
    return (a == 0.0f) ? 0 : ((a > 0.0f) ? +1 : -1);
}
static inline float deFloatFloor(float a)
{
    return (float)floor(a);
}
static inline float deFloatCeil(float a)
{
    return (float)ceil(a);
}
static inline float deFloatRound(float a)
{
    return deFloatFloor(a + 0.5f);
}
static inline float deFloatFrac(float a)
{
    return a - deFloatFloor(a);
}
static inline float deFloatMod(float a, float b)
{
    return (float)fmod(a, b);
}
static inline float deFloatModf(float x, float *i)
{
    double j   = 0;
    double ret = modf(x, &j);
    *i         = (float)j;
    return (float)ret;
}
static inline float deFloatMadd(float a, float b, float c)
{
    return (a * b) + c;
}
static inline float deFloatTrunc(float a)
{
    return deFloatSign(a) * deFloatFloor(deFloatAbs(a));
}
static inline float deFloatLdExp(float a, int exponent)
{
    return (float)ldexp(a, exponent);
}
static inline float deFloatFrExp(float x, int *exponent)
{
    return (float)frexp(x, exponent);
}
float deFloatFractExp(float x, int *exponent);

static inline double deSign(double x)
{
    return deDoubleIsIEEENaN(x) ? x : (double)((x > 0.0) - (x < 0.0));
}
static inline int deIntSign(double x)
{
    return (x > 0.0) - (x < 0.0);
}
static inline double deFloor(double a)
{
    return floor(a);
}
static inline double deCeil(double a)
{
    return ceil(a);
}
static inline double deRound(double a)
{
    return floor(a + 0.5);
}
static inline double deFrac(double a)
{
    return a - deFloor(a);
}
static inline double deMod(double a, double b)
{
    return fmod(a, b);
}
static inline double deModf(double x, double *i)
{
    return modf(x, i);
}
static inline double deMadd(double a, double b, double c)
{
    return (a * b) + c;
}
static inline double deTrunc(double a)
{
    return deSign(a) * floor(fabs(a));
}
static inline double deLdExp(double a, int exponent)
{
    return ldexp(a, exponent);
}
double deRoundEven(double a);
static inline double deFrExp(double x, int *exponent)
{
    return frexp(x, exponent);
}
/* Like frexp, except the returned fraction is in range [1.0, 2.0) */
double deFractExp(double x, int *exponent);

/* Exponential functions. */

static inline float deFloatPow(float a, float b)
{
    return (float)pow(a, b);
}
static inline float deFloatExp(float a)
{
    return (float)exp(a);
}
static inline float deFloatLog(float a)
{
    return (float)log(a);
}
static inline float deFloatExp2(float a)
{
    return (float)exp(a * DE_LOG_2);
}
static inline float deFloatLog2(float a)
{
    return (float)log(a) * DE_INV_LOG_2;
}
static inline float deFloatSqrt(float a)
{
    return (float)sqrt(a);
}
static inline float deFloatRcp(float a)
{
    return (1.0f / a);
}
static inline float deFloatRsq(float a)
{
    float s = (float)sqrt(a);
    return (s == 0.0f) ? 0.0f : (1.0f / s);
}

static inline double dePow(double a, double b)
{
    return pow(a, b);
}
static inline double deExp(double a)
{
    return exp(a);
}
static inline double deLog(double a)
{
    return log(a);
}
static inline double deExp2(double a)
{
    return exp(a * log(2.0));
}
static inline double deLog2(double a)
{
    return log(a) / log(2.0);
}
static inline double deSqrt(double a)
{
    return sqrt(a);
}
static inline double deCbrt(double a)
{
    return deSign(a) * dePow(deAbs(a), 1.0 / 3.0);
}

/* Geometric functions. */

static inline float deFloatRadians(float a)
{
    return a * (DE_PI / 180.0f);
}
static inline float deFloatDegrees(float a)
{
    return a * (180.0f / DE_PI);
}
static inline float deFloatSin(float a)
{
    return (float)sin(a);
}
static inline float deFloatCos(float a)
{
    return (float)cos(a);
}
static inline float deFloatTan(float a)
{
    return (float)tan(a);
}
static inline float deFloatAsin(float a)
{
    return (float)asin(a);
}
static inline float deFloatAcos(float a)
{
    return (float)acos(a);
}
static inline float deFloatAtan2(float y, float x)
{
    return (float)atan2(y, x);
}
static inline float deFloatAtanOver(float yOverX)
{
    return (float)atan(yOverX);
}
static inline float deFloatSinh(float a)
{
    return (float)sinh(a);
}
static inline float deFloatCosh(float a)
{
    return (float)cosh(a);
}
static inline float deFloatTanh(float a)
{
    return (float)tanh(a);
}
static inline float deFloatAsinh(float a)
{
    return deFloatLog(a + deFloatSqrt(a * a + 1));
}
static inline float deFloatAcosh(float a)
{
    return deFloatLog(a + deFloatSqrt(a * a - 1));
}
static inline float deFloatAtanh(float a)
{
    return 0.5f * deFloatLog((1.0f + a) / (1.0f - a));
}

static inline double deSin(double a)
{
    return sin(a);
}
static inline double deCos(double a)
{
    return cos(a);
}
static inline double deTan(double a)
{
    return tan(a);
}
static inline double deAsin(double a)
{
    return asin(a);
}
static inline double deAcos(double a)
{
    return acos(a);
}
static inline double deAtan2(double y, double x)
{
    return atan2(y, x);
}
static inline double deAtanOver(double yOverX)
{
    return atan(yOverX);
}
static inline double deSinh(double a)
{
    return sinh(a);
}
static inline double deCosh(double a)
{
    return cosh(a);
}
static inline double deTanh(double a)
{
    return tanh(a);
}
static inline double deAsinh(double a)
{
    return deLog(a + deSqrt(a * a + 1));
}
static inline double deAcosh(double a)
{
    return deLog(a + deSqrt(a * a - 1));
}
static inline double deAtanh(double a)
{
    return 0.5 * deLog((1.0 + a) / (1.0 - a));
}

/* Interpolation. */

static inline float deFloatMix(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}
static inline float deFloatStep(float limit, float val)
{
    return (val < limit) ? 0.0f : 1.0f;
}
static inline float deFloatSmoothStep(float e0, float e1, float v)
{
    float t;
    if (v <= e0)
        return 0.0f;
    if (v >= e1)
        return 1.0f;
    t = (v - e0) / (e1 - e0);
    return t * t * (3.0f - 2.0f * t);
}

static inline double deMix(double a, double b, double t)
{
    return a * (1.0 - t) + b * t;
}
static inline double deStep(double limit, double val)
{
    return (val < limit) ? 0.0 : 1.0;
}

/* Convert int to float. If the value cannot be represented exactly in native single precision format, return
 * either the nearest lower or the nearest higher representable value, chosen in an implementation-defined manner.
 *
 * \note Choosing either nearest lower or nearest higher means that implementation could for example consistently
 *       choose the lower value, i.e. this function does not round towards nearest.
 * \note Value returned is in native single precision format. For example with x86 extended precision, the value
 *       returned might not be representable in IEEE single precision float.
 */
static inline float deInt32ToFloat(int32_t x)
{
    return (float)x;
}

/* Convert to float. If the value cannot be represented exactly in IEEE single precision floating point format,
 * return the nearest lower (round towards negative inf). */
float deInt32ToFloatRoundToNegInf(int32_t x);

/* Convert to float. If the value cannot be represented exactly IEEE single precision floating point format,
 * return the nearest higher (round towards positive inf). */
float deInt32ToFloatRoundToPosInf(int32_t x);

/* Conversion to integer. */

static inline int32_t deChopFloatToInt32(float x)
{
    return (int32_t)x;
}
static inline int32_t deFloorFloatToInt32(float x)
{
    return (int32_t)(deFloatFloor(x));
}
static inline int32_t deCeilFloatToInt32(float x)
{
    return (int32_t)(deFloatCeil(x));
}

static inline int32_t deChopToInt32(double x)
{
    return (int32_t)x;
}
static inline int32_t deFloorToInt32(double x)
{
    return (int32_t)(deFloor(x));
}
static inline int32_t deCeilToInt32(double x)
{
    return (int32_t)(deCeil(x));
}

/* Arithmetic round */
static inline int16_t deRoundFloatToInt16(float x)
{
    if (x >= 0.0f)
        return (int16_t)(x + 0.5f);
    else
        return (int16_t)(x - 0.5f);
}
static inline int32_t deRoundFloatToInt32(float x)
{
    if (x >= 0.0f)
        return (int32_t)(x + 0.5f);
    else
        return (int32_t)(x - 0.5f);
}
static inline int64_t deRoundFloatToInt64(float x)
{
    if (x >= 0.0f)
        return (int64_t)(x + 0.5f);
    else
        return (int64_t)(x - 0.5f);
}

static inline int16_t deRoundToInt16(double x)
{
    if (x >= 0.0)
        return (int16_t)(x + 0.5);
    else
        return (int16_t)(x - 0.5);
}
static inline int32_t deRoundToInt32(double x)
{
    if (x >= 0.0)
        return (int32_t)(x + 0.5);
    else
        return (int32_t)(x - 0.5);
}
static inline int64_t deRoundToInt64(double x)
{
    if (x >= 0.0)
        return (int64_t)(x + 0.5);
    else
        return (int64_t)(x - 0.5);
}

DE_END_EXTERN_C

#endif /* _DEMATH_H */
