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
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_NeverDestroyed, Construct)
{
    { static NeverDestroyed<LifecycleLogger> x; UNUSED_PARAM(x);}
    ASSERT_STREQ("construct(<default>) ", takeLogStr().c_str());

    { static NeverDestroyed<LifecycleLogger> x("name"); UNUSED_PARAM(x); }
    ASSERT_STREQ("construct(name) ", takeLogStr().c_str());

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
        static NeverDestroyed<LifecycleLogger> x;
        static NeverDestroyed<LifecycleLogger> y { WTF::move(x) };
        UNUSED_PARAM(y);
    }
    ASSERT_STREQ("construct(<default>) move-construct(<default>) ", takeLogStr().c_str());

    {
        static NeverDestroyed<const LifecycleLogger> x;
        static NeverDestroyed<const LifecycleLogger> y { WTF::move(x) };
        UNUSED_PARAM(y);
    }
    ASSERT_STREQ("construct(<default>) move-construct(<default>) ", takeLogStr().c_str());

    { static NeverDestroyed<MoveOnlyLifecycleLogger> x { [] { return MoveOnlyLifecycleLogger { "name" }; }() }; UNUSED_PARAM(x); }
    ASSERT_STREQ("construct(name) move-construct(name) destruct(<default>) ", takeLogStr().c_str());
}

static const Vector<int>& list()
{
    static const NeverDestroyed x = Vector<int> { 1, 2, 3 };
    return x;
}

TEST(WTF_NeverDestroyed, Basic)
{
    ASSERT_EQ(list().size(), 3u);
    ASSERT_EQ(&list(), &list());
}

NODELETE static StringImpl* passThrough(StringImpl* string)
{
    return string;
}

NODELETE static AtomStringImpl* passThroughAtom(AtomStringImpl* atom)
{
    return atom;
}

// A wrapper of RefPtr<X> converts to X*, like the RefPtr. Wrappers of anything else do not convert to pointers.
static_assert(std::is_convertible_v<const NeverDestroyed<RefPtr<StringImpl>>&, StringImpl*>);
static_assert(std::is_convertible_v<const NeverDestroyed<const RefPtr<StringImpl>>&, StringImpl*>);
static_assert(std::is_convertible_v<const LazyNeverDestroyed<RefPtr<AtomStringImpl>>&, AtomStringImpl*>);
static_assert(std::is_convertible_v<const MainThreadNeverDestroyed<RefPtr<StringImpl>>&, StringImpl*>);
static_assert(std::is_convertible_v<const MainThreadLazyNeverDestroyed<const RefPtr<AtomStringImpl>>&, AtomStringImpl*>);
static_assert(!std::is_convertible_v<const NeverDestroyed<RefPtr<StringImpl>>&, AtomStringImpl*>);
static_assert(!std::is_convertible_v<const NeverDestroyed<Vector<int>>&, int*>);
static_assert(!std::is_convertible_v<const NeverDestroyed<int>&, void*>);
static_assert(!std::is_convertible_v<const NeverDestroyed<std::unique_ptr<int>>&, int*>);

TEST(WTF_NeverDestroyed, RefPtrConvertsToRawPointer)
{
    static NeverDestroyed<RefPtr<StringImpl>> string { StringImpl::create("never destroyed"_s) };
    static NeverDestroyed<const RefPtr<StringImpl>> constString { "const never destroyed"_si };
    static NeverDestroyed<RefPtr<StringImpl>> nullString;

    // Passing to a raw pointer parameter.
    EXPECT_EQ(passThrough(string), string.get().get());
    EXPECT_EQ(passThrough(constString), constString.get().get());
    EXPECT_FALSE(passThrough(nullString));
    EXPECT_TRUE(equal(passThrough(string), "never destroyed"_s));

    // Comparing with raw pointers and RefPtrs (identity).
    RefPtr<StringImpl> sameString = string;
    RefPtr<StringImpl> otherString = StringImpl::create("never destroyed"_s);
    EXPECT_TRUE(string == sameString.get());
    EXPECT_TRUE(sameString.get() == string);
    EXPECT_FALSE(string != sameString.get());
    EXPECT_FALSE(string == otherString.get());
    EXPECT_TRUE(nullString == nullptr);
    EXPECT_FALSE(string == nullptr);

    // Atoms, and the lazily constructed variant.
    static LazyNeverDestroyed<RefPtr<AtomStringImpl>> atom;
    atom.construct(AtomStringImpl::add("lazy atom"_s));
    RefPtr<AtomStringImpl> sameAtom = AtomStringImpl::add("lazy atom"_s);
    EXPECT_EQ(passThroughAtom(atom), sameAtom.get());
    EXPECT_TRUE(atom == sameAtom);
    EXPECT_TRUE(sameAtom == atom);
    EXPECT_TRUE(atom == sameAtom.get());
    EXPECT_FALSE(atom != sameAtom);
}

} // namespace TestWebKitAPI
