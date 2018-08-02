/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "LifecycleLogger.h"
#include "MoveOnlyLifecycleLogger.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(WTF_NeverDestroyed, Construct)
{
    { static NeverDestroyed<LifecycleLogger> x; UNUSED_PARAM(x);}
    ASSERT_STREQ("construct(<default>) ", takeLogStr().c_str());

    { static NeverDestroyed<LifecycleLogger> x("name"); UNUSED_PARAM(x); }
    ASSERT_STREQ("construct(name) ", takeLogStr().c_str());

    { static auto x = makeNeverDestroyed(LifecycleLogger { "name" }); UNUSED_PARAM(x); }
    ASSERT_STREQ("construct(name) move-construct(name) destruct(<default>) ", takeLogStr().c_str());

    {
        static NeverDestroyed<LifecycleLogger> x("name");
        LifecycleLogger l = x.get();
        l.setName("x");
        ASSERT_STREQ(x.get().name, "name");
    }
    ASSERT_STREQ("construct(name) copy-construct(name) set-name(x) destruct(x) ", takeLogStr().c_str());

    { static NeverDestroyed<LifecycleLogger> x { [] { return LifecycleLogger { "name" }; }() }; UNUSED_PARAM(x); }
    ASSERT_STREQ("construct(name) move-construct(name) destruct(<default>) ", takeLogStr().c_str());

    {
        static auto x = makeNeverDestroyed([] {
            LifecycleLogger l { "name" };
            l.setName("x");
            return l;
        }());
        ASSERT_STREQ(x.get().name, "x");
    }
#if COMPILER(MSVC) && !defined(NDEBUG)
    ASSERT_STREQ("construct(name) set-name(x) move-construct(x) destruct(<default>) move-construct(x) destruct(<default>) ", takeLogStr().c_str());
#else
    ASSERT_STREQ("construct(name) set-name(x) move-construct(x) destruct(<default>) ", takeLogStr().c_str());
#endif

    {
        static NeverDestroyed<LifecycleLogger> x;
        static NeverDestroyed<LifecycleLogger> y { WTFMove(x) };
        UNUSED_PARAM(y);
    }
    ASSERT_STREQ("construct(<default>) move-construct(<default>) ", takeLogStr().c_str());

    {
        static NeverDestroyed<const LifecycleLogger> x;
        static NeverDestroyed<const LifecycleLogger> y { WTFMove(x) };
        UNUSED_PARAM(y);
    }
    ASSERT_STREQ("construct(<default>) move-construct(<default>) ", takeLogStr().c_str());

    { static NeverDestroyed<MoveOnlyLifecycleLogger> x { [] { return MoveOnlyLifecycleLogger { "name" }; }() }; UNUSED_PARAM(x); }
    ASSERT_STREQ("construct(name) move-construct(name) destruct(<default>) ", takeLogStr().c_str());

    {
        static auto x = makeNeverDestroyed([] {
            MoveOnlyLifecycleLogger l { "name" };
            l.setName("x");
            return l;
        }());
        UNUSED_PARAM(x);
    }
#if COMPILER(MSVC) && !defined(NDEBUG)
    ASSERT_STREQ("construct(name) set-name(x) move-construct(x) destruct(<default>) move-construct(x) destruct(<default>) ", takeLogStr().c_str());
#else
    ASSERT_STREQ("construct(name) set-name(x) move-construct(x) destruct(<default>) ", takeLogStr().c_str());
#endif
}

static const Vector<int>& list()
{
    static const auto x = makeNeverDestroyed(Vector<int> { 1, 2, 3 });
    return x;
}

TEST(WTF_NeverDestroyed, Basic)
{
    ASSERT_EQ(list().size(), 3u);
    ASSERT_EQ(&list(), &list());
}

} // namespace TestWebKitAPI
