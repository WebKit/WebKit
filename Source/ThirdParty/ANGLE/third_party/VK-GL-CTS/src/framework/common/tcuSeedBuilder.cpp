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
 * \brief Utility class to build seeds from different data types.
 *
 * Values are first XORed with type specifig mask, which makes sure that
 * two values with different types, but same bit presentation produce
 * different results. Then values are passed through 32 bit crc.
 *//*--------------------------------------------------------------------*/

#include "tcuSeedBuilder.hpp"

#include "deMemory.h"

namespace tcu
{

namespace
{

uint32_t advanceCrc32(uint32_t oldCrc, size_t len, const uint8_t *data)
{
    const uint32_t generator = 0x04C11DB7u;
    uint32_t crc             = oldCrc;

    for (size_t i = 0; i < len; i++)
    {
        const uint32_t current = static_cast<uint32_t>(data[i]);
        crc                    = crc ^ current;

        for (size_t bitNdx = 0; bitNdx < 8; bitNdx++)
        {
            if (crc & 1u)
                crc = (crc >> 1u) ^ generator;
            else
                crc = (crc >> 1u);
        }
    }

    return crc;
}

} // namespace

SeedBuilder::SeedBuilder(void) : m_hash(0xccf139d7u)
{
}

void SeedBuilder::feed(size_t size, const void *ptr)
{
    m_hash = advanceCrc32(m_hash, size, (const uint8_t *)ptr);
}

SeedBuilder &operator<<(SeedBuilder &builder, bool value)
{
    const uint8_t val = (value ? 54 : 7);

    builder.feed(sizeof(val), &val);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, int8_t value)
{
    const int8_t val = value ^ 75;

    builder.feed(sizeof(val), &val);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, uint8_t value)
{
    const uint8_t val = value ^ 140u;

    builder.feed(sizeof(val), &val);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, int16_t value)
{
    const int16_t val    = value ^ 555;
    const uint8_t data[] = {
        (uint8_t)(((uint16_t)val) & 0xFFu),
        (uint8_t)(((uint16_t)val) >> 8),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, uint16_t value)
{
    const uint16_t val   = value ^ 37323u;
    const uint8_t data[] = {
        (uint8_t)(val & 0xFFu),
        (uint8_t)(val >> 8),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, int32_t value)
{
    const int32_t val    = value ^ 53054741;
    const uint8_t data[] = {
        (uint8_t)(((uint32_t)val) & 0xFFu),
        (uint8_t)((((uint32_t)val) >> 8) & 0xFFu),
        (uint8_t)((((uint32_t)val) >> 16) & 0xFFu),
        (uint8_t)((((uint32_t)val) >> 24) & 0xFFu),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, uint32_t value)
{
    const uint32_t val   = value ^ 1977303630u;
    const uint8_t data[] = {
        (uint8_t)(val & 0xFFu),
        (uint8_t)((val >> 8) & 0xFFu),
        (uint8_t)((val >> 16) & 0xFFu),
        (uint8_t)((val >> 24) & 0xFFu),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, int64_t value)
{
    const int64_t val    = value ^ 772935234179004386ll;
    const uint8_t data[] = {
        (uint8_t)(((uint64_t)val) & 0xFFu),         (uint8_t)((((uint64_t)val) >> 8) & 0xFFu),
        (uint8_t)((((uint64_t)val) >> 16) & 0xFFu), (uint8_t)((((uint64_t)val) >> 24) & 0xFFu),

        (uint8_t)((((uint64_t)val) >> 32) & 0xFFu), (uint8_t)((((uint64_t)val) >> 40) & 0xFFu),
        (uint8_t)((((uint64_t)val) >> 48) & 0xFFu), (uint8_t)((((uint64_t)val) >> 56) & 0xFFu),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, uint64_t value)
{
    const uint64_t val   = value ^ 4664937258000467599ull;
    const uint8_t data[] = {
        (uint8_t)(val & 0xFFu),         (uint8_t)((val >> 8) & 0xFFu),
        (uint8_t)((val >> 16) & 0xFFu), (uint8_t)((val >> 24) & 0xFFu),

        (uint8_t)((val >> 32) & 0xFFu), (uint8_t)((val >> 40) & 0xFFu),
        (uint8_t)((val >> 48) & 0xFFu), (uint8_t)((val >> 56) & 0xFFu),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, float value)
{
    // \note Assume that float has same endianess as uint32.
    uint32_t val;

    deMemcpy(&val, &value, sizeof(uint32_t));

    {
        const uint8_t data[] = {
            (uint8_t)(val & 0xFFu),
            (uint8_t)((val >> 8) & 0xFFu),
            (uint8_t)((val >> 16) & 0xFFu),
            (uint8_t)((val >> 24) & 0xFFu),
        };

        builder.feed(sizeof(data), data);
        return builder;
    }
}

SeedBuilder &operator<<(SeedBuilder &builder, double value)
{
    // \note Assume that double has same endianess as uint64.
    uint64_t val;

    deMemcpy(&val, &value, sizeof(uint64_t));

    const uint8_t data[] = {
        (uint8_t)(val & 0xFFu),         (uint8_t)((val >> 8) & 0xFFu),
        (uint8_t)((val >> 16) & 0xFFu), (uint8_t)((val >> 24) & 0xFFu),

        (uint8_t)((val >> 32) & 0xFFu), (uint8_t)((val >> 40) & 0xFFu),
        (uint8_t)((val >> 48) & 0xFFu), (uint8_t)((val >> 56) & 0xFFu),
    };

    builder.feed(sizeof(data), data);
    return builder;
}

SeedBuilder &operator<<(SeedBuilder &builder, const std::string &value)
{
    builder.feed(value.length(), value.c_str());
    return builder;
}

} // namespace tcu
