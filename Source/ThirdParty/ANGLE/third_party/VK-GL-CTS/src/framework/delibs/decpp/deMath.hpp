#ifndef _DEMATH_HPP
#define _DEMATH_HPP
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

#include "deFloat16.h"
#include "deMath.h"

#include <limits>

inline double deToDouble(deFloat16 x)
{
    return deFloat16To64(x);
}
inline double deToDouble(float x)
{
    return x;
}
inline double deToDouble(double x)
{
    return x;
}

template <typename T>
inline T deToFloatType(double x)
{
    return static_cast<T>(x);
}

template <>
inline deFloat16 deToFloatType<deFloat16>(double x)
{
    return deFloat64To16(x);
}

// These helpers make the C helpers usable from templates.  Because some of
// these deal with signaling NaN, it's important that no implicit float
// conversion operations happen.
inline bool deIsPositiveZero(deFloat16 x)
{
    return deHalfIsPositiveZero(x);
}
inline bool deIsPositiveZero(float x)
{
    return deFloatIsPositiveZero(x);
}
inline bool deIsPositiveZero(double x)
{
    return deDoubleIsPositiveZero(x);
}
inline bool deIsNegativeZero(deFloat16 x)
{
    return deHalfIsNegativeZero(x);
}
inline bool deIsNegativeZero(float x)
{
    return deFloatIsNegativeZero(x);
}
inline bool deIsNegativeZero(double x)
{
    return deDoubleIsNegativeZero(x);
}
inline bool deIsIEEENaN(deFloat16 x)
{
    return deHalfIsIEEENaN(x);
}
inline bool deIsIEEENaN(float x)
{
    return deFloatIsIEEENaN(x);
}
inline bool deIsIEEENaN(double x)
{
    return deDoubleIsIEEENaN(x);
}
inline bool deIsSignalingNaN(deFloat16 x)
{
    return deHalfIsSignalingNaN(x);
}
inline bool deIsSignalingNaN(float x)
{
    return deFloatIsSignalingNaN(x);
}
inline bool deIsSignalingNaN(double x)
{
    return deDoubleIsSignalingNaN(x);
}
inline bool deIsQuietNaN(deFloat16 x)
{
    return deHalfIsQuietNaN(x);
}
inline bool deIsQuietNaN(float x)
{
    return deFloatIsQuietNaN(x);
}
inline bool deIsQuietNaN(double x)
{
    return deDoubleIsQuietNaN(x);
}

template <typename T>
inline T deQuietNaN()
{
    return std::numeric_limits<T>::quiet_NaN();
}

template <>
inline deFloat16 deQuietNaN<deFloat16>()
{
    return 0x7e01;
}

template <typename T>
inline T deSignalingNaN()
{
    return std::numeric_limits<T>::signaling_NaN();
}

template <>
inline deFloat16 deSignalingNaN<deFloat16>()
{
    return 0x7c01;
}

#endif // _DEMATH_HPP
