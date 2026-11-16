#ifndef _TCURANDOMVALUEITERATOR_HPP
#define _TCURANDOMVALUEITERATOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Random value iterator.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deRandom.hpp"

namespace tcu
{

template <typename T>
T getRandomValue(de::Random &rnd)
{
    // \note memcpy() is the only valid way to do cast from uint32 to float for instnance.
    uint8_t data[sizeof(T) + sizeof(T) % 4];
    DE_STATIC_ASSERT(sizeof(data) % 4 == 0);
    for (int vecNdx = 0; vecNdx < DE_LENGTH_OF_ARRAY(data) / 4; vecNdx++)
    {
        uint32_t rval = rnd.getUint32();
        for (int compNdx = 0; compNdx < 4; compNdx++)
            data[vecNdx * 4 + compNdx] = ((const uint8_t *)&rval)[compNdx];
    }
    return *(const T *)&data[0];
}

// Faster implementations for int types.
template <>
inline uint8_t getRandomValue<uint8_t>(de::Random &rnd)
{
    return (uint8_t)rnd.getUint32();
}
template <>
inline uint16_t getRandomValue<uint16_t>(de::Random &rnd)
{
    return (uint16_t)rnd.getUint32();
}
template <>
inline uint32_t getRandomValue<uint32_t>(de::Random &rnd)
{
    return rnd.getUint32();
}
template <>
inline uint64_t getRandomValue<uint64_t>(de::Random &rnd)
{
    return rnd.getUint64();
}
template <>
inline int8_t getRandomValue<int8_t>(de::Random &rnd)
{
    return (int8_t)rnd.getUint32();
}
template <>
inline int16_t getRandomValue<int16_t>(de::Random &rnd)
{
    return (int16_t)rnd.getUint32();
}
template <>
inline int32_t getRandomValue<int32_t>(de::Random &rnd)
{
    return (int32_t)rnd.getUint32();
}
template <>
inline int64_t getRandomValue<int64_t>(de::Random &rnd)
{
    return (int64_t)rnd.getUint64();
}

template <typename T>
class RandomValueIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T *;
    using reference         = T &;

    static RandomValueIterator begin(uint32_t seed, int numValues)
    {
        return RandomValueIterator<T>(seed, numValues);
    }
    static RandomValueIterator end(void)
    {
        return RandomValueIterator<T>(0, 0);
    }

    RandomValueIterator &operator++(void);
    RandomValueIterator operator++(int);

    const T &operator*(void) const
    {
        return m_curVal;
    }

    bool operator==(const RandomValueIterator<T> &other) const;
    bool operator!=(const RandomValueIterator<T> &other) const;

private:
    RandomValueIterator(uint32_t seed, int numLeft);

    de::Random m_rnd;
    int m_numLeft;
    T m_curVal;
};

template <typename T>
RandomValueIterator<T>::RandomValueIterator(uint32_t seed, int numLeft)
    : m_rnd(seed)
    , m_numLeft(numLeft)
    , m_curVal(numLeft > 0 ? getRandomValue<T>(m_rnd) : T())
{
}

template <typename T>
RandomValueIterator<T> &RandomValueIterator<T>::operator++(void)
{
    DE_ASSERT(m_numLeft > 0);

    m_numLeft -= 1;
    m_curVal = getRandomValue<T>(m_rnd);

    return *this;
}

template <typename T>
RandomValueIterator<T> RandomValueIterator<T>::operator++(int)
{
    RandomValueIterator copy(*this);
    ++(*this);
    return copy;
}

template <typename T>
bool RandomValueIterator<T>::operator==(const RandomValueIterator<T> &other) const
{
    return (m_numLeft == 0 && other.m_numLeft == 0) || (m_numLeft == other.m_numLeft && m_rnd == other.m_rnd);
}

template <typename T>
bool RandomValueIterator<T>::operator!=(const RandomValueIterator<T> &other) const
{
    return !(m_numLeft == 0 && other.m_numLeft == 0) && (m_numLeft != other.m_numLeft || m_rnd != other.m_rnd);
}

} // namespace tcu

#endif // _TCURANDOMVALUEITERATOR_HPP
