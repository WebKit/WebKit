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
 * \brief Memory pool -backed string.
 *//*--------------------------------------------------------------------*/

#include "dePoolString.hpp"

#include <algorithm>
#include <cstring>

namespace de
{

void PoolString::toString(std::string &str) const
{
    str.resize(size());
    std::copy(begin(), end(), str.begin());
}

void PoolString::append(const char *str)
{
    uintptr_t oldEnd = size();
    size_t len       = strlen(str);

    resize(size() + len);
    std::copy(str, str + len, begin() + oldEnd);
}

void PoolString::append(const std::string &str)
{
    uintptr_t oldEnd = size();

    resize(size() + str.length());
    std::copy(str.begin(), str.end(), begin() + oldEnd);
}

void PoolString::append(const PoolString &str)
{
    uintptr_t oldEnd = size();

    resize(size() + str.size());
    std::copy(str.begin(), str.end(), begin() + oldEnd);
}

std::ostream &operator<<(std::ostream &stream, const PoolString &string)
{
    for (PoolString::ConstIterator i = string.begin(); i != string.end(); i++)
        stream << *i;
    return stream;
}

} // namespace de
