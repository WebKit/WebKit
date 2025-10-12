/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <float.h>
#include <limits>
#include <numbers>
#include <stdint.h>
#include <stdlib.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>

#if OS(OPENBSD)
#include <sys/types.h>
#include <machine/ieee.h>
#endif

constexpr double piOverTwoDouble = std::numbers::pi / 2;
constexpr float piOverTwoFloat = static_cast<float>(piOverTwoDouble);

constexpr double piOverFourDouble = std::numbers::pi / 4;
constexpr float piOverFourFloat = static_cast<float>(piOverFourDouble);

#if OS(WINDOWS)

// Work around a bug in Win, where atan2(+-infinity, +-infinity) yields NaN instead of specific values.
extern "C" inline double wtf_atan2(double x, double y)
{
    constexpr double posInf = std::numeric_limits<double>::infinity();
    constexpr double negInf = -std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();

    double result = nan;

    if (x == posInf && y == posInf)
        result = piOverFourDouble;
    else if (x == posInf && y == negInf)
        result = 3 * piOverFourDouble;
    else if (x == negInf && y == posInf)
        result = -piOverFourDouble;
    else if (x == negInf && y == negInf)
        result = -3 * piOverFourDouble;
    else
        result = ::atan2(x, y);

    return result;
}

#define atan2(x, y) wtf_atan2(x, y)

#endif // OS(WINDOWS)

constexpr double radiansPerDegreeDouble = std::numbers::pi / 180.0;
constexpr double degreesPerRadianDouble = 180.0 / std::numbers::pi;
constexpr double gradientsPerDegreeDouble = 400.0 / 360.0;
constexpr double degreesPerGradientDouble = 360.0 / 400.0;
constexpr double turnsPerDegreeDouble = 1.0 / 360.0;
constexpr double degreesPerTurnDouble = 360.0;
constexpr double radiansPerTurnDouble = 2.0 * std::numbers::pi;

constexpr double deg2rad(double d)  { return d * radiansPerDegreeDouble; }
constexpr double rad2deg(double r)  { return r * degreesPerRadianDouble; }
constexpr double deg2grad(double d) { return d * gradientsPerDegreeDouble; }
constexpr double grad2deg(double g) { return g * degreesPerGradientDouble; }
constexpr double deg2turn(double d) { return d * turnsPerDegreeDouble; }
constexpr double turn2deg(double t) { return t * degreesPerTurnDouble; }


// Note that these differ from the casting the double values above in their rounding errors.
constexpr float radiansPerDegreeFloat = std::numbers::pi_v<float> / 180.0f;
constexpr float degreesPerRadianFloat = 180.0f / std::numbers::pi_v<float>;
constexpr float gradientsPerDegreeFloat= 400.0f / 360.0f;
constexpr float degreesPerGradientFloat = 360.0f / 400.0f;
constexpr float turnsPerDegreeFloat = 1.0f / 360.0f;
constexpr float degreesPerTurnFloat = 360.0f;
constexpr float radiansPerTurnFloat = 2.0f * std::numbers::pi_v<float>;

constexpr float deg2rad(float d)  { return d * radiansPerDegreeFloat; }
constexpr float rad2deg(float r)  { return r * degreesPerRadianFloat; }
constexpr float deg2grad(float d) { return d * gradientsPerDegreeFloat; }
constexpr float grad2deg(float g) { return g * degreesPerGradientFloat; }
constexpr float deg2turn(float d) { return d * turnsPerDegreeFloat; }
constexpr float turn2deg(float t) { return t * degreesPerTurnFloat; }

// Treat these as conversions through the canonical unit for angles, which is degrees.
constexpr double rad2grad(double r) { return deg2grad(rad2deg(r)); }
constexpr double grad2rad(double g) { return deg2rad(grad2deg(g)); }
constexpr double turn2grad(double t) { return deg2grad(turn2deg(t)); }
constexpr double grad2turn(double g) { return deg2turn(grad2deg(g)); }
constexpr double turn2rad(double t) { return deg2rad(turn2deg(t)); }
constexpr double rad2turn(double r) { return deg2turn(rad2deg(r)); }
constexpr float rad2grad(float r) { return deg2grad(rad2deg(r)); }
constexpr float grad2rad(float g) { return deg2rad(grad2deg(g)); }
constexpr float turn2grad(float t) { return deg2grad(turn2deg(t)); }
constexpr float grad2turn(float g) { return deg2turn(grad2deg(g)); }
constexpr float turn2rad(float t) { return deg2rad(turn2deg(t)); }
constexpr float rad2turn(float r) { return deg2turn(rad2deg(r)); }

namespace WTF {

// FIXME: Replace with std::isnan() once std::isnan() is constexpr. requires C++23
template<std::floating_point T>
constexpr bool isNaNConstExpr(T value)
{
#if COMPILER_HAS_CLANG_BUILTIN(__builtin_isnan)
    return __builtin_isnan(value);
#else
    return value != value;
#endif
}

template<std::integral T>
constexpr bool isNaNConstExpr(T)
{
    return false;
}

// FIXME: Replace with std::fabs() once std::fabs() is constexpr. requires C++23
template<std::floating_point T>
constexpr T fabsConstExpr(T value)
{
    if (value != value)
        return value;
    if (!value)
        return 0.0; // -0.0 should be converted to +0.0
    if (value < 0.0)
        return -value;
    return value;
}

} // namespace WTF

inline double roundTowardsPositiveInfinity(double value) { return std::floor(value + 0.5); }
inline float roundTowardsPositiveInfinity(float value) { return std::floor(value + 0.5f); }

// std::numeric_limits<T>::min() returns the smallest positive value for floating point types
template<typename T> consteval T defaultMinimumForClamp() { return std::numeric_limits<T>::min(); }
template<> consteval float defaultMinimumForClamp() { return -std::numeric_limits<float>::max(); }
template<> consteval double defaultMinimumForClamp() { return -std::numeric_limits<double>::max(); }
template<typename T> consteval T defaultMaximumForClamp() { return std::numeric_limits<T>::max(); }

// Same type in and out.
template<typename TargetType, typename SourceType>
    requires std::same_as<TargetType, SourceType>
constexpr TargetType clampTo(SourceType value, TargetType min = defaultMinimumForClamp<TargetType>(), TargetType max = defaultMaximumForClamp<TargetType>())
{
    if (value >= max)
        return max;
    if (value <= min)
        return min;
    return value;
}

// Floating point source.
template<typename TargetType, typename SourceType>
    requires (!std::same_as<TargetType, SourceType>
          &&   std::floating_point<SourceType>
          && !(std::floating_point<TargetType> && sizeof(TargetType) > sizeof(SourceType)))
constexpr TargetType clampTo(SourceType value, TargetType min = defaultMinimumForClamp<TargetType>(), TargetType max = defaultMaximumForClamp<TargetType>())
{
    if (value >= static_cast<SourceType>(max))
        return max;
    // This will return min if value is NaN.
    if (!(value > static_cast<SourceType>(min)))
        return min;
    return static_cast<TargetType>(value);
}

template<typename TargetType, typename SourceType>
    requires (!std::same_as<TargetType, SourceType>
          &&   std::floating_point<SourceType>
          &&   std::floating_point<TargetType>
          &&   sizeof(TargetType) > sizeof(SourceType))
constexpr TargetType clampTo(SourceType value, TargetType min = defaultMinimumForClamp<TargetType>(), TargetType max = defaultMaximumForClamp<TargetType>())
{
    TargetType convertedValue = static_cast<TargetType>(value);
    if (convertedValue >= max)
        return max;
    if (convertedValue <= min)
        return min;
    return convertedValue;
}

// Source and Target have the same sign and Source is larger or equal to Target
template<typename TargetType, typename SourceType>
    requires (!std::same_as<TargetType, SourceType>
          &&   std::integral<SourceType>
          &&   std::integral<TargetType>
          &&   std::signed_integral<TargetType> == std::signed_integral<SourceType>
          &&   sizeof(SourceType) >= sizeof(TargetType))
constexpr TargetType clampTo(SourceType value, TargetType min = defaultMinimumForClamp<TargetType>(), TargetType max = defaultMaximumForClamp<TargetType>())
{
    if (value >= static_cast<SourceType>(max))
        return max;
    if (value <= static_cast<SourceType>(min))
        return min;
    return static_cast<TargetType>(value);
}

// Clamping a unsigned integer to the max signed value.
template<typename TargetType, typename SourceType>
    requires (!std::same_as<TargetType, SourceType>
          &&   std::unsigned_integral<SourceType>
          &&   std::signed_integral<TargetType>
          &&   sizeof(SourceType) >= sizeof(TargetType))
constexpr TargetType clampTo(SourceType value)
{
    TargetType max = std::numeric_limits<TargetType>::max();
    if (value >= static_cast<SourceType>(max))
        return max;
    return static_cast<TargetType>(value);
}

// Clamping a signed integer into a valid unsigned integer.
template<typename TargetType, typename SourceType>
    requires (!std::same_as<TargetType, SourceType>
          &&   std::unsigned_integral<TargetType>
          &&   std::signed_integral<SourceType>
          &&   sizeof(SourceType) == sizeof(TargetType))
constexpr TargetType clampTo(SourceType value)
{
    if (value < 0)
        return 0;
    return static_cast<TargetType>(value);
}

template<typename TargetType, typename SourceType>
    requires (!std::same_as<TargetType, SourceType>
          &&   std::unsigned_integral<TargetType>
          &&   std::signed_integral<SourceType>
          &&   sizeof(SourceType) > sizeof(TargetType))
constexpr TargetType clampTo(SourceType value, TargetType min = defaultMinimumForClamp<TargetType>(), TargetType max = defaultMaximumForClamp<TargetType>())
{
    if (value >= static_cast<SourceType>(max))
        return max;
    if (value <= static_cast<SourceType>(min))
        return min;
    return static_cast<TargetType>(value);
}

constexpr int clampToInteger(double value)
{
    return clampTo<int>(value);
}

constexpr unsigned clampToUnsigned(double value)
{
    return clampTo<unsigned>(value);
}

constexpr float clampToFloat(double value)
{
    return clampTo<float>(value);
}

constexpr int clampToPositiveInteger(double value)
{
    return clampTo<int>(value, 0);
}

constexpr int clampToInteger(float value)
{
    return clampTo<int>(value);
}

template<std::integral T>
constexpr int clampToInteger(T x)
{
    constexpr T intMax = static_cast<unsigned>(std::numeric_limits<int>::max());

    if (x >= intMax)
        return std::numeric_limits<int>::max();
    return static_cast<int>(x);
}

// Explicitly accept 64bit result when clamping double value.
// Keep in mind that double can only represent 53bit integer precisely.
template<typename T>
constexpr T clampToAccepting64(double value, T min = defaultMinimumForClamp<T>(), T max = defaultMaximumForClamp<T>())
{
    return (value >= static_cast<double>(max)) ? max : ((value <= static_cast<double>(min)) ? min : static_cast<T>(value));
}

constexpr bool isWithinIntRange(float x)
{
    return x > static_cast<float>(std::numeric_limits<int>::min()) && x < static_cast<float>(std::numeric_limits<int>::max());
}

constexpr float normalizedFloat(float value)
{
    if (value > 0 && value < std::numeric_limits<float>::min())
        return std::numeric_limits<float>::min();
    if (value < 0 && value > -std::numeric_limits<float>::min())
        return -std::numeric_limits<float>::min();
    return value;
}

template<typename T>
constexpr bool hasOneBitSet(T value)
{
    return !((value - 1) & value) && value;
}

template<typename T>
constexpr bool hasZeroOrOneBitsSet(T value)
{
    return !((value - 1) & value);
}

template<typename T>
constexpr bool hasTwoOrMoreBitsSet(T value)
{
    return !hasZeroOrOneBitsSet(value);
}

template<typename T>
constexpr T divideRoundedUp(T a, T b)
{
    return (a + b - 1) / b;
}

template<typename T>
constexpr T timesThreePlusOneDividedByTwo(T value)
{
    // Mathematically equivalent to:
    //   (value * 3 + 1) / 2;
    // or:
    //   (unsigned)ceil(value * 1.5));
    // This form is not prone to internal overflow.
    return value + (value >> 1) + (value & 1);
}

template<typename T>
constexpr bool isNotZeroAndOrdered(T value)
{
    return value > 0.0 || value < 0.0;
}

template<typename T>
constexpr bool isZeroOrUnordered(T value)
{
    return !isNotZeroAndOrdered(value);
}

template<typename T>
constexpr bool isGreaterThanNonZeroPowerOfTwo(T value, unsigned power)
{
    // The crazy way of testing of index >= 2 ** power
    // (where I use ** to denote pow()).
    return !!((value >> 1) >> (power - 1));
}

template<typename T> constexpr bool isLessThan(const T& a, const T& b) { return a < b; }
template<typename T> constexpr bool isLessThanEqual(const T& a, const T& b) { return a <= b; }
template<typename T> constexpr bool isGreaterThan(const T& a, const T& b) { return a > b; }
template<typename T> constexpr bool isGreaterThanEqual(const T& a, const T& b) { return a >= b; }
template<typename T> constexpr bool isInRange(const T& a, const T& min, const T& max) { return a >= min && a <= max; }

// decompose 'number' to its sign, exponent, and mantissa components.
// The result is interpreted as:
//     (sign ? -1 : 1) * pow(2, exponent) * (mantissa / (1 << 52))
inline void decomposeDouble(double number, bool& sign, int32_t& exponent, uint64_t& mantissa)
{
    ASSERT(std::isfinite(number));

    sign = std::signbit(number);

    uint64_t bits = std::bit_cast<uint64_t>(number);
    exponent = (static_cast<int32_t>(bits >> 52) & 0x7ff) - 0x3ff;
    mantissa = bits & 0xFFFFFFFFFFFFFull;

    // Check for zero/denormal values; if so, adjust the exponent,
    // if not insert the implicit, omitted leading 1 bit.
    if (exponent == -0x3ff)
        exponent = mantissa ? -0x3fe : 0;
    else
        mantissa |= 0x10000000000000ull;
}

template<typename T> constexpr unsigned countOfBits = sizeof(T) * CHAR_BIT;
template<typename T> constexpr unsigned countOfMagnitudeBits = countOfBits<T> - std::is_signed_v<T>;

constexpr float powerOfTwo(unsigned e)
{
    float p = 1;
    while (e--)
        p *= 2;
    return p;
}

template<typename T> constexpr float maxPlusOne = powerOfTwo(countOfMagnitudeBits<T>);

// Calculate d % 2^{64}.
inline void doubleToInteger(double d, unsigned long long& value)
{
    if (std::isnan(d) || std::isinf(d))
        value = 0;
    else {
        // -2^{64} < fmodValue < 2^{64}.
        double fmodValue = fmod(trunc(d), maxPlusOne<unsigned long long>);
        if (fmodValue >= 0) {
            // 0 <= fmodValue < 2^{64}.
            // 0 <= value < 2^{64}. This cast causes no loss.
            value = static_cast<unsigned long long>(fmodValue);
        } else {
            // -2^{64} < fmodValue < 0.
            // 0 < fmodValueInUnsignedLongLong < 2^{64}. This cast causes no loss.
            unsigned long long fmodValueInUnsignedLongLong = static_cast<unsigned long long>(-fmodValue);
            // -1 < (std::numeric_limits<unsigned long long>::max() - fmodValueInUnsignedLongLong) < 2^{64} - 1.
            // 0 < value < 2^{64}.
            value = std::numeric_limits<unsigned long long>::max() - fmodValueInUnsignedLongLong + 1;
        }
    }
}

namespace WTF {

constexpr uint32_t roundUpToPowerOfTwo(auto v)
{
    return std::bit_ceil(v);
}

constexpr bool isPowerOfTwo(auto value)
{
    return std::has_single_bit(value);
}

constexpr unsigned maskForSize(unsigned size)
{
    if (!size)
        return 0;
    return roundUpToPowerOfTwo(size) - 1;
}

constexpr unsigned fastLog2(unsigned i)
{
    if (!i)
        return 0;
    constexpr int unsignedBitWidth = std::numeric_limits<unsigned>::digits - 1;
    unsigned log2 = unsignedBitWidth - std::countl_zero(i);
    if (i & (i - 1))
        log2 += 1;
    return log2;
}

constexpr unsigned fastLog2(uint64_t value)
{
    unsigned high = static_cast<unsigned>(value >> 32);
    if (high)
        return fastLog2(high) + 32;
    return fastLog2(static_cast<unsigned>(value));
}

template<std::floating_point T>
constexpr T safeFPDivision(T u, T v)
{
    // Protect against overflow / underflow.
    if (v < 1 && u > v * std::numeric_limits<T>::max())
        return std::numeric_limits<T>::max();
    if (v > 1 && u < v * std::numeric_limits<T>::min())
        return 0;
    return u / v;
}

// Floating point numbers comparison:
// u is "essentially equal" [1][2] to v if: | u - v | / |u| <= e and | u - v | / |v| <= e
//
// [1] Knuth, D. E. "Accuracy of Floating Point Arithmetic." The Art of Computer Programming. 3rd ed. Vol. 2.
//     Boston: Addison-Wesley, 1998. 229-45.
// [2] http://www.boost.org/doc/libs/1_34_0/libs/test/doc/components/test_tools/floating_point_comparison.html
template<std::floating_point T>
constexpr bool areEssentiallyEqual(T u, T v, T epsilon = std::numeric_limits<T>::epsilon())
{
    if (u == v)
        return true;

    const T delta = std::abs(u - v);
    return safeFPDivision(delta, std::abs(u)) <= epsilon && safeFPDivision(delta, std::abs(v)) <= epsilon;
}

// Match behavior of Math.min, where NaN is returned if either argument is NaN.
template<std::floating_point T>
constexpr T nanPropagatingMin(T a, T b)
{
    return isNaNConstExpr(a) || isNaNConstExpr(b) ? std::numeric_limits<T>::quiet_NaN() : std::min(a, b);
}

// Match behavior of Math.max, where NaN is returned if either argument is NaN.
template<std::floating_point T>
constexpr T nanPropagatingMax(T a, T b)
{
    return isNaNConstExpr(a) || isNaNConstExpr(b) ? std::numeric_limits<T>::quiet_NaN() : std::max(a, b);
}

constexpr bool isIntegral(float value)
{
    return !std::isinf(value) && std::trunc(value) == value;
}

template<typename T>
constexpr void incrementWithSaturation(T& value)
{
    if (value != std::numeric_limits<T>::max())
        ++value;
}

template<typename T>
constexpr T leftShiftWithSaturation(T value, unsigned shiftAmount, T max = std::numeric_limits<T>::max())
{
    T result = value << shiftAmount;
    // We will have saturated if shifting right doesn't recover the original value.
    if (result >> shiftAmount != value)
        return max;
    if (result > max)
        return max;
    return result;
}

// Check if two ranges overlap assuming that neither range is empty.
template<typename T>
constexpr bool nonEmptyRangesOverlap(T leftMin, T leftMax, T rightMin, T rightMax)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(leftMin < leftMax);
    ASSERT_UNDER_CONSTEXPR_CONTEXT(rightMin < rightMax);

    return leftMax > rightMin && rightMax > leftMin;
}

// Pass ranges with the min being inclusive and the max being exclusive. For example, this should
// return false:
//
//     rangesOverlap(0, 8, 8, 16)
template<typename T>
constexpr bool rangesOverlap(T leftMin, T leftMax, T rightMin, T rightMax)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(leftMin <= leftMax);
    ASSERT_UNDER_CONSTEXPR_CONTEXT(rightMin <= rightMax);
    
    // Empty ranges interfere with nothing.
    if (leftMin == leftMax)
        return false;
    if (rightMin == rightMax)
        return false;

    return nonEmptyRangesOverlap(leftMin, leftMax, rightMin, rightMax);
}

template<typename VectorType, typename RandomFunc>
constexpr void shuffleVector(VectorType& vector, size_t size, const RandomFunc& randomFunc)
{
    for (size_t i = 0; i + 1 < size; ++i)
        std::swap(vector[i], vector[i + randomFunc(size - i)]);
}

template<typename VectorType, typename RandomFunc>
constexpr void shuffleVector(VectorType& vector, const RandomFunc& randomFunc)
{
    shuffleVector(vector, vector.size(), randomFunc);
}

template<typename T>
constexpr unsigned clzConstexpr(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    auto uValue = unsignedCast(value);

    unsigned zeroCount = 0;
    for (int i = bitSize - 1; i >= 0; i--) {
        if (uValue >> i)
            break;
        zeroCount++;
    }
    return zeroCount;
}

template<typename T>
inline unsigned clz(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    auto uValue = unsignedCast(value);

    constexpr unsigned bitSize64 = sizeof(uint64_t) * CHAR_BIT;
    if (uValue)
        return __builtin_clzll(uValue) - (bitSize64 - bitSize);
    return bitSize;
}

template<typename T>
constexpr unsigned ctzConstexpr(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    auto uValue = unsignedCast(value);

    unsigned zeroCount = 0;
    for (unsigned i = 0; i < bitSize; i++) {
        if (uValue & 1)
            break;

        zeroCount++;
        uValue >>= 1;
    }
    return zeroCount;
}

template<typename T>
inline unsigned ctz(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    auto uValue = unsignedCast(value);

    if (uValue)
        return __builtin_ctzll(uValue);
    return bitSize;
}

template<typename T>
inline unsigned getLSBSet(T t)
{
    ASSERT(t);
    return ctz(t);
}

template<typename T>
constexpr unsigned getLSBSetConstexpr(T t)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(t);
    return ctzConstexpr(t);
}

template<typename T>
inline unsigned getMSBSet(T t)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;
    ASSERT(t);
    return bitSize - 1 - clz(t);
}

template<typename T>
constexpr unsigned getMSBSetConstexpr(T t)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;
    ASSERT_UNDER_CONSTEXPR_CONTEXT(t);
    return bitSize - 1 - clzConstexpr(t);
}

inline uint32_t reverseBits32(uint32_t value)
{
#if CPU(ARM64)
    uint32_t result;
    asm ("rbit %w0, %w1"
        : "=r"(result)
        : "r"(value));
    return result;
#else
    value = ((value & 0xaaaaaaaa) >> 1) | ((value & 0x55555555) << 1);
    value = ((value & 0xcccccccc) >> 2) | ((value & 0x33333333) << 2);
    value = ((value & 0xf0f0f0f0) >> 4) | ((value & 0x0f0f0f0f) << 4);
    value = ((value & 0xff00ff00) >> 8) | ((value & 0x00ff00ff) << 8);
    return (value >> 16) | (value << 16);
#endif
}

// For use in places where we could negate std::numeric_limits<T>::min and would like to avoid UB.
template<std::integral T>
constexpr T negate(T v)
{
    return static_cast<T>(~unsignedCast(v) + 1U);
}

template<typename BitsType, typename InputType>
constexpr bool isIdentical(InputType left, InputType right)
{
    return std::bit_cast<BitsType>(left) == std::bit_cast<BitsType>(right);
}

constexpr bool isIdentical(int32_t left, int32_t right)
{
    return isIdentical<int32_t>(left, right);
}

constexpr bool isIdentical(int64_t left, int64_t right)
{
    return isIdentical<int64_t>(left, right);
}

constexpr bool isIdentical(double left, double right)
{
    return isIdentical<int64_t>(left, right);
}

constexpr bool isIdentical(float left, float right)
{
    return isIdentical<int32_t>(left, right);
}

template<typename ResultType, typename InputType, typename BitsType>
constexpr bool isRepresentableAsImpl(InputType originalValue)
{
    // Convert the original value to the desired result type.
    ResultType result = static_cast<ResultType>(originalValue);

    // Convert the converted value back to the original type. The original value is representable
    // using the new type if such round-tripping doesn't lose bits.
    InputType newValue = static_cast<InputType>(result);

    return isIdentical<BitsType>(originalValue, newValue);
}

template<typename ResultType>
constexpr bool isRepresentableAs(int32_t value)
{
    return isRepresentableAsImpl<ResultType, int32_t, int32_t>(value);
}

template<typename ResultType>
constexpr bool isRepresentableAs(int64_t value)
{
    return isRepresentableAsImpl<ResultType, int64_t, int64_t>(value);
}

template<typename ResultType>
constexpr bool isRepresentableAs(size_t value)
{
    return isRepresentableAsImpl<ResultType, size_t, size_t>(value);
}

template<typename ResultType>
constexpr bool isRepresentableAs(double value)
{
    return isRepresentableAsImpl<ResultType, double, int64_t>(value);
}

template<typename T>
ALWAYS_INLINE constexpr T roundUpToMultipleOfImpl(size_t divisor, T x)
{
    T remainderMask = static_cast<T>(divisor) - 1;
    return (x + remainderMask) & ~remainderMask;
}

// Efficient implementation that takes advantage of powers of two.
template<typename T>
constexpr T roundUpToMultipleOf(size_t divisor, T x)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(divisor && isPowerOfTwo(divisor));
    return roundUpToMultipleOfImpl<T>(divisor, x);
}

template<size_t divisor>
constexpr size_t roundUpToMultipleOf(size_t x)
{
    static_assert(divisor && isPowerOfTwo(divisor));
    return roundUpToMultipleOfImpl(divisor, x);
}

template<size_t divisor, typename T>
constexpr T* roundUpToMultipleOf(T* x)
{
    static_assert(sizeof(T*) == sizeof(size_t));
    return reinterpret_cast<T*>(roundUpToMultipleOf<divisor>(reinterpret_cast<size_t>(x)));
}

template<typename T>
constexpr T roundUpToMultipleOfNonPowerOfTwo(size_t divisor, T x)
{
    T remainder = x % divisor;
    if (!remainder)
        return x;
    return x + static_cast<T>(divisor - remainder);
}

template<typename T, typename C>
constexpr Checked<T, C> roundUpToMultipleOfNonPowerOfTwo(Checked<T, C> divisor, Checked<T, C> x)
{
    if (x.hasOverflowed() || divisor.hasOverflowed())
        return ResultOverflowed;
    T remainder = x % divisor;
    if (!remainder)
        return x;
    return x + static_cast<T>(divisor.value() - remainder);
}

// Returns positive distance to next multiple of a power-of-two divisor.
template<size_t divisor>
constexpr size_t distanceToMultipleOf(size_t x)
{
    static_assert(divisor && isPowerOfTwo(divisor));
    return (divisor - (x % divisor)) % divisor;
}

template<typename T>
constexpr T roundDownToMultipleOf(size_t divisor, T x)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(divisor && isPowerOfTwo(divisor));
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>(mask(static_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

template<typename T>
constexpr T* roundDownToMultipleOf(size_t divisor, T* x)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(isPowerOfTwo(divisor));
    return reinterpret_cast<T*>(mask(reinterpret_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

template<size_t divisor, typename T>
constexpr T roundDownToMultipleOf(T x)
{
    static_assert(isPowerOfTwo(divisor), "'divisor' must be a power of two.");
    return roundDownToMultipleOf(divisor, x);
}

} // namespace WTF

using WTF::shuffleVector;
using WTF::clz;
using WTF::ctz;
using WTF::getLSBSet;
using WTF::getMSBSet;
using WTF::isNaNConstExpr;
using WTF::fabsConstExpr;
using WTF::reverseBits32;
using WTF::roundDownToMultipleOf;
using WTF::roundUpToMultipleOf;
using WTF::roundUpToMultipleOfNonPowerOfTwo;
using WTF::distanceToMultipleOf;
using WTF::roundUpToPowerOfTwo;
using WTF::isIdentical;
using WTF::isRepresentableAs;
using WTF::isPowerOfTwo;
