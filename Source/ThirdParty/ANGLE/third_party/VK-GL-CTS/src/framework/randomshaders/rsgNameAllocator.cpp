/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Name Allocator.
 *//*--------------------------------------------------------------------*/

#include "rsgNameAllocator.hpp"

namespace rsg
{

NameAllocator::NameAllocator(void) : m_nextName(1)
{
}

NameAllocator::~NameAllocator(void)
{
}

inline std::string nameNdxToStr(uint32_t name)
{
    std::string str      = "";
    uint32_t alphabetLen = 'z' - 'a' + 1;

    while (name > alphabetLen)
    {
        str.insert(str.begin(), (char)('a' + ((name - 1) % alphabetLen)));
        name = ((name - 1) / alphabetLen);
    }

    str.insert(str.begin(), (char)('a' + (name % (alphabetLen + 1)) - 1));

    return str;
}

std::string NameAllocator::allocate(void)
{
    DE_ASSERT(m_nextName != 0);
    return nameNdxToStr(m_nextName++);
}

} // namespace rsg
