/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FractionalLayoutUnit_h
#define FractionalLayoutUnit_h

#include <limits.h>
#include <limits>
#include <math.h>
#include <stdlib.h>

#if PLATFORM(CHROMIUM) || PLATFORM(MAC)
#define ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
#endif

namespace WebCore {

static const int kFixedPointDenominator = 60;
const int intMaxForLayoutUnit = INT_MAX / kFixedPointDenominator;
const int intMinForLayoutUnit = -intMaxForLayoutUnit;

class FractionalLayoutUnit {
public:
    FractionalLayoutUnit() : m_value(0) { }
    FractionalLayoutUnit(int value) { ASSERT(isInBounds(value)); m_value = value * kFixedPointDenominator; }
    FractionalLayoutUnit(unsigned short value) { ASSERT(isInBounds(value)); m_value = value * kFixedPointDenominator; }
    FractionalLayoutUnit(unsigned int value) { ASSERT(isInBounds(value)); m_value = value * kFixedPointDenominator; }
    FractionalLayoutUnit(float value) { ASSERT(isInBounds(value)); m_value = value * kFixedPointDenominator; }
    FractionalLayoutUnit(double value) { ASSERT(isInBounds(value)); m_value = value * kFixedPointDenominator; }
    FractionalLayoutUnit(const FractionalLayoutUnit& value) { m_value = value.rawValue(); }
#ifdef ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
    FractionalLayoutUnit(size_t value) { ASSERT(isInBounds(value)); m_value = static_cast<int>(value * kFixedPointDenominator); }
#endif

    inline int toInt() const { return m_value / kFixedPointDenominator; }
    inline unsigned toUnsigned() const { ASSERT(m_value >= 0); return toInt(); }
    inline float toFloat() const { return static_cast<float>(m_value) / kFixedPointDenominator; }
    inline double toDouble() const { return static_cast<double>(m_value) / kFixedPointDenominator; }

    operator int() const { return toInt(); }
    operator unsigned() const { return toUnsigned(); }
    operator float() const { return toFloat(); }
    operator double() const { return toDouble(); }
    operator bool() const { return m_value; }

    inline int rawValue() const { return m_value; }
    inline void setRawValue(int value) { m_value = value; }
    inline void setRawValue(long long value)
    {
        ASSERT(value > std::numeric_limits<int>::min() && value < std::numeric_limits<int>::max());
        m_value = static_cast<int>(value);
    }

    inline FractionalLayoutUnit abs() const
    {
        FractionalLayoutUnit returnValue;
        returnValue.setRawValue(::abs(m_value));
        return returnValue;
    }
#if OS(DARWIN)
    inline int wtf_ceil() const
#else
    inline int ceil() const
#endif
    {
        if (m_value > 0)
            return (m_value + kFixedPointDenominator - 1) / kFixedPointDenominator;
        return (m_value - kFixedPointDenominator + 1) / kFixedPointDenominator;
    }
    inline int round() const
    {
        if (m_value > 0)
            return (m_value + (kFixedPointDenominator / 2)) / kFixedPointDenominator;
        return (m_value - (kFixedPointDenominator / 2)) / kFixedPointDenominator;
    }

    inline int floor() const
    {
        return toInt();
    }

    static float epsilon() { return 1 / kFixedPointDenominator; }
    static const FractionalLayoutUnit max()
    {
        FractionalLayoutUnit m;
        m.m_value = std::numeric_limits<int>::max();
        return m;
    }
    static const FractionalLayoutUnit min()
    {
        FractionalLayoutUnit m;
        m.m_value = std::numeric_limits<int>::min();
        return m;
    }
    
private:
    inline bool isInBounds(int value)
    {
        return ::abs(value) <= std::numeric_limits<int>::max() / kFixedPointDenominator;
    }
    inline bool isInBounds(unsigned value)
    {
        return value <= static_cast<unsigned>(std::numeric_limits<int>::max()) / kFixedPointDenominator;
    }
    inline bool isInBounds(double value)
    {
        return ::fabs(value) <= std::numeric_limits<int>::max() / kFixedPointDenominator;
    }
#ifdef ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
    inline bool isInBounds(size_t value)
    {
        return value <= static_cast<size_t>(std::numeric_limits<int>::max()) / kFixedPointDenominator;
    }
#endif

    int m_value;
};

inline bool operator<=(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    return a.rawValue() <= b.rawValue();
}

inline bool operator<=(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() <= b;
}

inline bool operator<=(const FractionalLayoutUnit& a, int b)
{
    return a <= FractionalLayoutUnit(b);
}

inline bool operator<=(const float a, const FractionalLayoutUnit& b)
{
    return a <= b.toFloat();
}

inline bool operator<=(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) <= b;
}

inline bool operator>=(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    return a.rawValue() >= b.rawValue();
}

inline bool operator>=(const FractionalLayoutUnit& a, int b)
{
    return a >= FractionalLayoutUnit(b);
}

inline bool operator>=(const float a, const FractionalLayoutUnit& b)
{
    return a >= b.toFloat();
}

inline bool operator>=(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() >= b;
}

inline bool operator>=(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) >= b;
}

inline bool operator<(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    return a.rawValue() < b.rawValue();
}

inline bool operator<(const FractionalLayoutUnit& a, int b)
{
    return a < FractionalLayoutUnit(b);
}

inline bool operator<(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() < b;
}

inline bool operator<(const FractionalLayoutUnit& a, double b)
{
    return a.toDouble() < b;
}

inline bool operator<(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) < b;
}

inline bool operator<(const float a, const FractionalLayoutUnit& b)
{
    return a < b.toFloat();
}

inline bool operator>(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    return a.rawValue() > b.rawValue();
}

inline bool operator>(const FractionalLayoutUnit& a, double b)
{
    return a.toDouble() > b;
}

inline bool operator>(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() > b;
}

inline bool operator>(const FractionalLayoutUnit& a, int b)
{
    return a > FractionalLayoutUnit(b);
}

inline bool operator>(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) > b;
}

inline bool operator>(const float a, const FractionalLayoutUnit& b)
{
    return a > b.toFloat();
}

inline bool operator>(const double a, const FractionalLayoutUnit& b)
{
    return a > b.toDouble();
}

inline bool operator!=(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    return a.rawValue() != b.rawValue();
}

inline bool operator!=(const FractionalLayoutUnit& a, float b)
{
    return a != FractionalLayoutUnit(b);
}

inline bool operator!=(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) != b;
}

inline bool operator!=(const FractionalLayoutUnit& a, int b)
{
    return a != FractionalLayoutUnit(b);
}

inline bool operator==(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    return a.rawValue() == b.rawValue();
}

inline bool operator==(const FractionalLayoutUnit& a, int b)
{
    return a == FractionalLayoutUnit(b);
}

inline bool operator==(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) == b;
}

inline bool operator==(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() == b;
}

inline bool operator==(const float a, const FractionalLayoutUnit& b)
{
    return a == b.toFloat();
}

// For multiplication that's prone to overflow, this bounds it to FractionalLayoutUnit::max() and ::min()
inline FractionalLayoutUnit boundedMultiply(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    FractionalLayoutUnit returnVal;
    long long rawVal = static_cast<long long>(a.rawValue()) * b.rawValue() / kFixedPointDenominator;
    if (rawVal > std::numeric_limits<int>::max())
        return FractionalLayoutUnit::max();
    if (rawVal < std::numeric_limits<int>::min())
        return FractionalLayoutUnit::min();
    returnVal.setRawValue(rawVal);
    return returnVal;
}

inline FractionalLayoutUnit operator*(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    FractionalLayoutUnit returnVal;
    long long rawVal = static_cast<long long>(a.rawValue()) * b.rawValue() / kFixedPointDenominator;
    returnVal.setRawValue(rawVal);
    return returnVal;
}    

inline double operator*(const FractionalLayoutUnit& a, double b)
{
    return a.toDouble() * b;
}

inline float operator*(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() * b;
}

inline FractionalLayoutUnit operator*(const FractionalLayoutUnit& a, int b)
{
    return a * FractionalLayoutUnit(b);
}

inline FractionalLayoutUnit operator*(const FractionalLayoutUnit& a, unsigned b)
{
    return a * FractionalLayoutUnit(b);
}

#ifdef ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
inline FractionalLayoutUnit operator*(const FractionalLayoutUnit& a, size_t b)
{
    return a * FractionalLayoutUnit(b);
}
#endif

inline FractionalLayoutUnit operator*(unsigned a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) * b;
}

inline FractionalLayoutUnit operator*(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) * b;
}

inline float operator*(const float a, const FractionalLayoutUnit& b)
{
    return a * b.toFloat();
}

inline double operator*(const double a, const FractionalLayoutUnit& b)
{
    return a * b.toDouble();
}

#ifdef ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
inline FractionalLayoutUnit operator*(size_t a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) * b;
}
#endif

inline FractionalLayoutUnit operator/(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    FractionalLayoutUnit returnVal;
    long long rawVal = static_cast<long long>(kFixedPointDenominator) * a.rawValue() / b.rawValue();
    returnVal.setRawValue(rawVal);
    return returnVal;
}    

inline float operator/(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() / b;
}

inline double operator/(const FractionalLayoutUnit& a, double b)
{
    return a.toDouble() / b;
}

inline FractionalLayoutUnit operator/(const FractionalLayoutUnit& a, int b)
{
    return a / FractionalLayoutUnit(b);
}

inline FractionalLayoutUnit operator/(const FractionalLayoutUnit& a, unsigned int b)
{
    return a / FractionalLayoutUnit(b);
}

#ifdef ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
inline FractionalLayoutUnit operator/(const FractionalLayoutUnit& a, size_t b)
{
    return a / FractionalLayoutUnit(b);
}
#endif

inline float operator/(const float a, const FractionalLayoutUnit& b)
{
    return a / b.toFloat();
}

inline FractionalLayoutUnit operator/(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) / b;
}

inline FractionalLayoutUnit operator/(unsigned int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) / b;
}

#ifdef ARE_SIZE_T_UNSIGNED_DIFFERENT_UNIT
inline FractionalLayoutUnit operator/(size_t a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) / b;
}
#endif

inline FractionalLayoutUnit operator+(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    FractionalLayoutUnit returnVal;
    returnVal.setRawValue(a.rawValue() + b.rawValue());
    return returnVal;
}

inline FractionalLayoutUnit operator+(const FractionalLayoutUnit& a, int b)
{
    return a + FractionalLayoutUnit(b);
}

inline float operator+(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() + b;
}

inline double operator+(const FractionalLayoutUnit& a, double b)
{
    return a.toDouble() + b;
}

inline FractionalLayoutUnit operator+(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) + b;
}

inline float operator+(const float a, const FractionalLayoutUnit& b)
{
    return a + b.toFloat();
}

inline FractionalLayoutUnit operator-(const FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    FractionalLayoutUnit returnVal;
    returnVal.setRawValue(a.rawValue() - b.rawValue());
    return returnVal;
}

inline FractionalLayoutUnit operator-(const FractionalLayoutUnit& a, int b)
{
    return a - FractionalLayoutUnit(b);
}

inline FractionalLayoutUnit operator-(const FractionalLayoutUnit& a, unsigned b)
{
    return a - FractionalLayoutUnit(b);
}

inline float operator-(const FractionalLayoutUnit& a, float b)
{
    return a.toFloat() - b;
}

inline FractionalLayoutUnit operator-(const int a, const FractionalLayoutUnit& b)
{
    return FractionalLayoutUnit(a) - b;
}

inline float operator-(const float a, const FractionalLayoutUnit& b)
{
    return a - b.toFloat();
}

inline FractionalLayoutUnit operator-(const FractionalLayoutUnit& a)
{
    FractionalLayoutUnit returnVal;
    returnVal.setRawValue(-a.rawValue());
    return returnVal;
}

inline FractionalLayoutUnit& operator+=(FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    a = a + b;
    return a;
}

inline FractionalLayoutUnit& operator+=(FractionalLayoutUnit& a, int b)
{
    a = a + b;
    return a;
}

inline float& operator+=(float& a, const FractionalLayoutUnit& b)
{
    a = a + b;
    return a;
}

inline FractionalLayoutUnit& operator-=(FractionalLayoutUnit& a, int b)
{
    a = a - b;
    return a;
}

inline FractionalLayoutUnit& operator-=(FractionalLayoutUnit& a, const FractionalLayoutUnit& b)
{
    a = a - b;
    return a;
}

inline float& operator-=(float& a, const FractionalLayoutUnit& b)
{
    a = a - b;
    return a;
}

inline FractionalLayoutUnit& operator*=(FractionalLayoutUnit& a, int b)
{
    a = a * b;
    return a;
}

inline FractionalLayoutUnit& operator*=(FractionalLayoutUnit& a, float b)
{
    a = a * b;
    return a;
}

inline int snapSizeToPixel(FractionalLayoutUnit size, FractionalLayoutUnit location) 
{
    return (location + size).round() - location.round();
}

} // namespace WebCore

#endif // FractionalLayoutUnit_h
