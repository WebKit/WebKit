#ifndef _DERANDOM_HPP
#define _DERANDOM_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief Random number generator utilities.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deRandom.h"

#include <iterator>  // std::distance()
#include <algorithm> // std::swap()

namespace de
{

//! Random self-test - compare returned values against hard-coded values.
void Random_selfTest(void);

class Random
{
public:
    Random(uint32_t seed)
    {
        deRandom_init(&m_rnd, seed);
    }
    ~Random(void)
    {
    }

    float getFloat(void)
    {
        return deRandom_getFloat(&m_rnd);
    }
    double getDouble(void)
    {
        return deRandom_getDouble(&m_rnd);
    }
    bool getBool(void)
    {
        return deRandom_getBool(&m_rnd) == true;
    }

    float getFloat(float min, float max);
    double getDouble(double min, double max);
    int getInt(int min, int max);

    int64_t getInt64(void)
    {
        uint32_t upper = getUint32();
        return static_cast<int64_t>((uint64_t)upper << 32ull | (uint64_t)getUint32());
    }
    uint64_t getUint64(void)
    {
        uint32_t upper = getUint32();
        return (uint64_t)upper << 32ull | (uint64_t)getUint32();
    }
    int32_t getInt32(void)
    {
        return static_cast<int32_t>(getUint32());
    }
    uint32_t getUint32(void)
    {
        return deRandom_getUint32(&m_rnd);
    }
    uint16_t getUint16(void)
    {
        return (uint16_t)deRandom_getUint32(&m_rnd);
    }
    uint8_t getUint8(void)
    {
        return (uint8_t)deRandom_getUint32(&m_rnd);
    }

    template <class InputIter, class OutputIter>
    void choose(InputIter first, InputIter last, OutputIter result, int numItems);

    template <typename T, class InputIter>
    T choose(InputIter first, InputIter last);

    // \note Weights must be floats
    template <typename T, class InputIter, class WeightIter>
    T chooseWeighted(InputIter first, InputIter last, WeightIter weight);

    template <class Iterator>
    void shuffle(Iterator first, Iterator last);

    bool operator==(const Random &other) const;
    bool operator!=(const Random &other) const;

private:
    deRandom m_rnd;
} DE_WARN_UNUSED_TYPE;

// Inline implementations

inline float Random::getFloat(float min, float max)
{
    DE_ASSERT(min <= max);
    return min + (max - min) * getFloat();
}

inline double Random::getDouble(double min, double max)
{
    DE_ASSERT(min <= max);
    return min + (max - min) * getDouble();
}

inline int Random::getInt(int min, int max)
{
    DE_ASSERT(min <= max);
    if (min == (int)0x80000000 && max == (int)0x7fffffff)
        return (int)getUint32();
    else
        return min + (int)(getUint32() % ((uint32_t)max - (uint32_t)min + 1u));
}

// Template implementations

template <class InputIter, class OutputIter>
void Random::choose(InputIter first, InputIter last, OutputIter result, int numItems)
{
    // Algorithm: Reservoir sampling
    // http://en.wikipedia.org/wiki/Reservoir_sampling
    // \note Will not work for suffling an array. Use suffle() instead.

    int ndx;
    for (ndx = 0; first != last; ++first, ++ndx)
    {
        if (ndx < numItems)
            *(result + ndx) = *first;
        else
        {
            int r = getInt(0, ndx);
            if (r < numItems)
                *(result + r) = *first;
        }
    }

    DE_ASSERT(ndx >= numItems);
}

template <typename T, class InputIter>
T Random::choose(InputIter first, InputIter last)
{
    T val = T();
    DE_ASSERT(first != last);
    choose(first, last, &val, 1);
    return val;
}

template <typename T, class InputIter, class WeightIter>
T Random::chooseWeighted(InputIter first, InputIter last, WeightIter weight)
{
    // Compute weight sum
    float weightSum = 0.0f;
    int ndx;
    for (ndx = 0; (first + ndx) != last; ndx++)
        weightSum += *(weight + ndx);

    // Random point in 0..weightSum
    float p = getFloat(0.0f, weightSum);

    // Find item in range
    InputIter lastNonZero = last;
    float curWeight       = 0.0f;
    for (ndx = 0; (first + ndx) != last; ndx++)
    {
        float w = *(weight + ndx);

        curWeight += w;

        if (p < curWeight)
            return *(first + ndx);
        else if (w > 0.0f)
            lastNonZero = first + ndx;
    }

    DE_ASSERT(lastNonZero != last);
    return *lastNonZero;
}

template <class Iterator>
void Random::shuffle(Iterator first, Iterator last)
{
    using std::swap;

    // Fisher-Yates suffle
    int numItems = (int)std::distance(first, last);

    for (int i = numItems - 1; i >= 1; i--)
    {
        int j = getInt(0, i);
        swap(*(first + i), *(first + j));
    }
}

template <typename T>
T randomScalar(de::Random &rnd, T minValue, T maxValue);
template <>
inline float randomScalar(de::Random &rnd, float minValue, float maxValue)
{
    return rnd.getFloat(minValue, maxValue);
}
template <>
inline double randomScalar(de::Random &rnd, double minValue, double maxValue)
{
    return rnd.getDouble(minValue, maxValue);
}
template <>
inline int32_t randomScalar(de::Random &rnd, int32_t minValue, int32_t maxValue)
{
    return rnd.getInt(minValue, maxValue);
}
template <>
inline uint32_t randomScalar(de::Random &rnd, uint32_t minValue, uint32_t maxValue)
{
    if (minValue == 0 && maxValue == 0xffffffff)
        return rnd.getUint32();
    return minValue + rnd.getUint32() % (maxValue - minValue + 1);
}
template <>
inline int16_t randomScalar(de::Random &rnd, int16_t minValue, int16_t maxValue)
{
    return (int16_t)rnd.getInt(minValue, maxValue);
}
template <>
inline uint16_t randomScalar(de::Random &rnd, uint16_t minValue, uint16_t maxValue)
{
    return (uint16_t)(minValue + rnd.getUint16() % (maxValue - minValue + 1));
}
template <>
inline int8_t randomScalar(de::Random &rnd, int8_t minValue, int8_t maxValue)
{
    return (int8_t)rnd.getInt(minValue, maxValue);
}
template <>
inline uint8_t randomScalar(de::Random &rnd, uint8_t minValue, uint8_t maxValue)
{
    return (uint8_t)(minValue + rnd.getUint8() % (maxValue - minValue + 1));
}

void fillWithRandomData(de::Random &rnd, void *data, size_t size);

template <typename T>
void fillWithRandomData(de::Random &rnd, T *something)
{
    fillWithRandomData(rnd, something, sizeof(T));
}

} // namespace de

#endif // _DERANDOM_HPP
