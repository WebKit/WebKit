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
 * \brief Utilities for STL containers.
 *//*--------------------------------------------------------------------*/

#include "deSTLUtil.hpp"

#include <map>
#include <set>

using std::map;
using std::set;

namespace de
{

void STLUtil_selfTest(void)
{
    {
        map<int, int> m;
        insert(m, 4, 5);
        DE_TEST_ASSERT(contains(m, 4));
        DE_TEST_ASSERT(lookup(m, 4) == 5);
        DE_TEST_ASSERT(*tryLookup(m, 4) == 5);
        DE_TEST_ASSERT(!contains(m, 3));
        DE_TEST_ASSERT(tryLookup(m, 3) == nullptr);
        DE_TEST_ASSERT(lookupDefault(m, 3, 7) == 7);
    }

    {
        set<int> s1;
        s1.insert(2);
        s1.insert(3);
        DE_TEST_ASSERT(contains(s1, 2));
        DE_TEST_ASSERT(contains(s1, 3));
        DE_TEST_ASSERT(!contains(s1, 5));

        set<int> s2;
        s2.insert(3);
        s2.insert(5);
        DE_TEST_ASSERT(!contains(s2, 2));
        DE_TEST_ASSERT(contains(s2, 3));
        DE_TEST_ASSERT(contains(s2, 5));

        set<int> si = intersection(s1, s2);
        DE_TEST_ASSERT(!contains(si, 2));
        DE_TEST_ASSERT(contains(si, 3));
        DE_TEST_ASSERT(!contains(si, 5));

        set<int> su = set_union(s1, s2);
        DE_TEST_ASSERT(contains(su, 2));
        DE_TEST_ASSERT(contains(su, 3));
        DE_TEST_ASSERT(contains(su, 5));
    }
}

} // namespace de
