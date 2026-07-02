#ifndef _TCUSEEDBUILDER_HPP
#define _TCUSEEDBUILDER_HPP
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

#include "tcuDefs.hpp"
#include "tcuVector.hpp"

#include <string>
#include <vector>

namespace tcu
{

class SeedBuilder
{
public:
    SeedBuilder(void);
    uint32_t get(void) const
    {
        return m_hash;
    }
    void feed(size_t size, const void *ptr);

private:
    uint32_t m_hash;
} DE_WARN_UNUSED_TYPE;

SeedBuilder &operator<<(SeedBuilder &builder, bool value);
SeedBuilder &operator<<(SeedBuilder &builder, int8_t value);
SeedBuilder &operator<<(SeedBuilder &builder, uint8_t value);

SeedBuilder &operator<<(SeedBuilder &builder, int16_t value);
SeedBuilder &operator<<(SeedBuilder &builder, uint16_t value);

SeedBuilder &operator<<(SeedBuilder &builder, int32_t value);
SeedBuilder &operator<<(SeedBuilder &builder, uint32_t value);

SeedBuilder &operator<<(SeedBuilder &builder, int64_t value);
SeedBuilder &operator<<(SeedBuilder &builder, uint64_t value);

SeedBuilder &operator<<(SeedBuilder &builder, float value);
SeedBuilder &operator<<(SeedBuilder &builder, double value);

SeedBuilder &operator<<(SeedBuilder &builder, const std::string &value);

template <class T, int Size>
SeedBuilder &operator<<(SeedBuilder &builder, const tcu::Vector<T, Size> &value)
{
    for (int i = 0; i < Size; i++)
        builder << value[i];

    return builder;
}

} // namespace tcu

#endif // _TCUSEEDBUILDER_HPP
