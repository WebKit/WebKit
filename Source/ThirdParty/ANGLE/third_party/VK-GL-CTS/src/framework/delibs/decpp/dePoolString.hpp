#ifndef _DEPOOLSTRING_HPP
#define _DEPOOLSTRING_HPP
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

#include "deDefs.hpp"
#include "deMemPool.hpp"
#include "dePoolArray.hpp"

#include <ostream>
#include <string>

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief String template backed by memory pool
 *
 * \note Memory in PoolString is not contiguous so pointer arithmetic
 *       to access next element(s) doesn't work.
 *//*--------------------------------------------------------------------*/
class PoolString : public PoolArray<char>
{
public:
    explicit PoolString(MemPool *pool);
    PoolString(MemPool *pool, const PoolString &other);
    ~PoolString(void);

    void toString(std::string &str) const;
    std::string toString(void) const;

    void append(const char *str);
    void append(const std::string &str);
    void append(const PoolString &str);

    PoolString &operator=(const char *str)
    {
        clear();
        append(str);
        return *this;
    }
    PoolString &operator=(const std::string &str)
    {
        clear();
        append(str);
        return *this;
    }
    PoolString &operator=(const PoolString &str);

    PoolString &operator+=(const char *str)
    {
        append(str);
        return *this;
    }
    PoolString &operator+=(const std::string &str)
    {
        append(str);
        return *this;
    }
    PoolString &operator+=(const PoolString &str)
    {
        append(str);
        return *this;
    }

private:
    PoolString(const PoolString &other);
};

// Operators.
std::ostream &operator<<(std::ostream &stream, const PoolString &string);

// PoolString inline implementations.

inline PoolString::PoolString(MemPool *pool) : PoolArray<char>(pool)
{
}

inline PoolString::~PoolString(void)
{
}

inline std::string PoolString::toString(void) const
{
    std::string str;
    toString(str);
    return str;
}

inline PoolString &PoolString::operator=(const PoolString &str)
{
    if (this == &str)
        return *this;

    clear();
    append(str);
    return *this;
}

} // namespace de

#endif // _DEPOOLSTRING_HPP
