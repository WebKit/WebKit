#ifndef _DESHA1_HPP
#define _DESHA1_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief SHA1 hash functions
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

#include "deSha1.h"

#include <string>
#include <vector>

namespace de
{

class Sha1
{
public:
    Sha1(const deSha1 &hash) : m_hash(hash)
    {
    }

    static Sha1 parse(const std::string &str);
    static Sha1 compute(size_t size, const void *data);

    bool operator==(const Sha1 &other) const
    {
        return deSha1_equal(&m_hash, &other.m_hash) == true;
    }
    bool operator!=(const Sha1 &other) const
    {
        return !(*this == other);
    }

private:
    deSha1 m_hash;
};

class Sha1Stream
{
public:
    Sha1Stream(void);
    void process(size_t size, const void *data);
    Sha1 finalize(void);

private:
    deSha1Stream m_stream;
};

// Utility functions for building hash from values.
// \note This is not same as serializing the values and computing hash from the data.
//       Some extra care is required when dealing with types that have platform
//       specific size.
//       All vectors and strings will include their size in the hash. Following codes
//       produce different results:
//           stream << "Hi" << "Hello";
//       and
//           stream << "HiHello";

inline Sha1Stream &operator<<(Sha1Stream &stream, bool b)
{
    const uint8_t value = b ? 1 : 0;
    stream.process(sizeof(value), &value);
    return stream;
}

inline Sha1Stream &operator<<(Sha1Stream &stream, uint32_t value)
{
    const uint8_t data[] = {(uint8_t)(0xFFu & (value >> 24)), (uint8_t)(0xFFu & (value >> 16)),
                            (uint8_t)(0xFFu & (value >> 8)), (uint8_t)(0xFFu & (value >> 0))};

    stream.process(sizeof(data), data);
    return stream;
}

inline Sha1Stream &operator<<(Sha1Stream &stream, int32_t value)
{
    return stream << (uint32_t)value;
}

inline Sha1Stream &operator<<(Sha1Stream &stream, uint64_t value)
{
    const uint8_t data[] = {(uint8_t)(0xFFull & (value >> 56)), (uint8_t)(0xFFull & (value >> 48)),
                            (uint8_t)(0xFFull & (value >> 40)), (uint8_t)(0xFFull & (value >> 32)),
                            (uint8_t)(0xFFull & (value >> 24)), (uint8_t)(0xFFull & (value >> 16)),
                            (uint8_t)(0xFFull & (value >> 8)),  (uint8_t)(0xFFull & (value >> 0))};

    stream.process(sizeof(data), data);
    return stream;
}

inline Sha1Stream &operator<<(Sha1Stream &stream, int64_t value)
{
    return stream << (uint64_t)value;
}

template <typename T>
inline Sha1Stream &operator<<(Sha1Stream &stream, const std::vector<T> &values)
{
    stream << (uint64_t)values.size();

    for (size_t ndx = 0; ndx < values.size(); ndx++)
        stream << values[ndx];

    return stream;
}

inline Sha1Stream &operator<<(Sha1Stream &stream, const std::string &str)
{
    stream << (uint64_t)str.size();
    stream.process(str.size(), str.c_str());
    return stream;
}

} // namespace de

#endif // _DESHA1_HPP
