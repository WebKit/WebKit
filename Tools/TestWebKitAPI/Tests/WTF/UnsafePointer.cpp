/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/UnsafePointer.h>

#include <gtest/gtest.h>
#include <type_traits>

namespace TestWebKitAPI {

// Only declarations before the test, to ensure that UnsafePointer can work with pointers to incomplete types.
class UnsafeObject;
UnsafeObject* GetNullUnsafeObject();
UnsafeObject* GetUnsafeObjectA();
UnsafeObject* GetUnsafeObjectB();

TEST(WTF_UnsafePointer, Basic)
{
    UnsafeObject* n = GetNullUnsafeObject();
    ASSERT_EQ(n, nullptr);
    UnsafeObject* a = GetUnsafeObjectA();
    ASSERT_NE(a, nullptr);
    UnsafeObject* b = GetUnsafeObjectB();
    ASSERT_NE(b, nullptr);
    ASSERT_NE(a, b);

    using UP = UnsafePointer<UnsafeObject*>;

    static_assert(std::is_constructible_v<bool, UP>, "explicit UnsafePointer conversion to bool should be allowed");
    static_assert(!std::is_convertible_v<UP, bool>, "implicit UnsafePointer conversion to bool should not be allowed");

    ASSERT_FALSE(UP(n));
    ASSERT_TRUE(!UP(n));

    ASSERT_TRUE(UP(a));
    ASSERT_FALSE(!UP(a));

    static_assert(std::is_convertible_v<UnsafeObject*, UP>, "implicit UnsafePointer construction from pointer should be allowed");
    static_assert(!std::is_convertible_v<UP, UnsafeObject*>, "UnsafePointer conversion to pointer should not be allowed");

    for (UnsafeObject* p1 : { n, a, b }) {
        for (UnsafeObject* p2 : { n, a, b }) {
            EXPECT_EQ(UP(p1) == UP(p2), p1 == p2);
            EXPECT_EQ(UP(p1) == p2, p1 == p2);
            EXPECT_EQ(p1 == UP(p2), p1 == p2);

            EXPECT_EQ(UP(p1) != UP(p2), p1 != p2);
            EXPECT_EQ(UP(p1) != p2, p1 != p2);
            EXPECT_EQ(p1 != UP(p2), p1 != p2);
        }
    }
}

// Object that can only be explicitly constructed, other accesses are forbidden.
// This is to ensure that UnsafePointer doesn't dereferences it.
class UnsafeObject {
public:
    struct ConstructionToken { };
    explicit UnsafeObject(ConstructionToken) { }

    UnsafeObject(const UnsafeObject&) = delete;
    UnsafeObject(UnsafeObject&&) = delete;
};

UnsafeObject* GetNullUnsafeObject()
{
    return nullptr;
}

UnsafeObject* GetUnsafeObjectA()
{
    static UnsafeObject a { UnsafeObject::ConstructionToken { } };
    return &a;
}

UnsafeObject* GetUnsafeObjectB()
{
    static UnsafeObject b { UnsafeObject::ConstructionToken { } };
    return &b;
}

} // namespace TestWebKitAPI
