#ifndef _DEARRAYUTIL_HPP
#define _DEARRAYUTIL_HPP
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
 * \brief Array utils
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deMeta.hpp"

namespace de
{

//! Get an element of an array with a specified size.
template <int LastElementIndex, int Size, typename Elem>
const Elem &getSizedArrayElement(const Elem (&array)[Size],
                                 typename de::meta::EnableIf<int, LastElementIndex == Size>::Type offset)
{
    DE_ASSERT(inBounds(offset, 0, Size));
    return array[offset];
}

//! Get an element of an array with a compile-time constant size.
template <int Size, typename Elem>
const Elem &getArrayElement(const Elem (&array)[Size], int offset)
{
    DE_ASSERT(inBounds(offset, 0, Size));
    return array[offset];
}

} // namespace de

#endif // _DEARRAYUTIL_HPP
