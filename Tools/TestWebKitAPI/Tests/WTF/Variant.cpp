/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "Counters.h"
#include "RefLogger.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_Variant, Initial)
{
    std::experimental::variant<int, double> v1;
    EXPECT_TRUE(v1.index() == 0);
    EXPECT_TRUE(std::experimental::get<int>(v1) == 0);

    struct T {
        T() : value(15) { }
        int value;
    };

    std::experimental::variant<T, int> v2;
    EXPECT_TRUE(v2.index() == 0);
    EXPECT_TRUE(std::experimental::get<T>(v2).value == 15);
}

TEST(WTF_Variant, Basic)
{
    std::experimental::variant<int, double> variant = 1;
    EXPECT_TRUE(variant.index() == 0);
    EXPECT_TRUE(std::experimental::get<int>(variant) == 1);
    EXPECT_TRUE(*std::experimental::get_if<int>(variant) == 1);
    EXPECT_TRUE(std::experimental::get_if<double>(variant) == nullptr);
    EXPECT_TRUE(std::experimental::holds_alternative<int>(variant));
    EXPECT_FALSE(std::experimental::holds_alternative<double>(variant));

    variant = 1.0;
    EXPECT_TRUE(variant.index() == 1);
    EXPECT_TRUE(std::experimental::get<double>(variant) == 1);
    EXPECT_TRUE(*std::experimental::get_if<double>(variant) == 1.0);
    EXPECT_TRUE(std::experimental::get_if<int>(variant) == nullptr);
    EXPECT_TRUE(std::experimental::holds_alternative<double>(variant));
    EXPECT_FALSE(std::experimental::holds_alternative<int>(variant));
}

TEST(WTF_Variant, BasicVisitor)
{
    enum class Type {
        None,
        Int,
        Float,
        String,
    };

    struct Visitor {
        Visitor(Type& t)
            : type(t)
        {
        }
        
        Type& type;

        void operator()(int) const { type = Type::Int; }
        void operator()(float) const { type = Type::Float;  }
        void operator()(String) const { type = Type::String;  }
    };

    Type type = Type::None;

    std::experimental::variant<int, float, String> variant = 8;
    std::experimental::visit(Visitor(type), variant);
    EXPECT_TRUE(Type::Int == type);


    variant = 1.0f;
    std::experimental::visit(Visitor(type), variant);
    EXPECT_TRUE(Type::Float == type);


    variant = "hello";
    std::experimental::visit(Visitor(type), variant);
    EXPECT_TRUE(Type::String == type);
}

TEST(WTF_Variant, VisitorUsingMakeVisitor)
{
    enum class Type {
        None,
        Int,
        Float,
        String,
    };

    Type type = Type::None;

    auto visitor = WTF::makeVisitor(
        [&](int) { type = Type::Int; },
        [&](float) { type = Type::Float; },
        [&](String) { type = Type::String; }
    );

    std::experimental::variant<int, float, String> variant = 8;
    std::experimental::visit(visitor, variant);
    EXPECT_TRUE(Type::Int == type);


    variant = 1.0f;
    std::experimental::visit(visitor, variant);
    EXPECT_TRUE(Type::Float == type);


    variant = "hello";
    std::experimental::visit(visitor, variant);
    EXPECT_TRUE(Type::String == type);
}

TEST(WTF_Variant, ConstructorDestructor)
{
    ConstructorDestructorCounter::TestingScope scope;

    {
        auto uniquePtr = std::make_unique<ConstructorDestructorCounter>();
        std::experimental::variant<std::unique_ptr<ConstructorDestructorCounter>, int> v = WTFMove(uniquePtr);

        EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
        EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);
    }
    
    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_Variant, RefPtr)
{
    {
        RefLogger a("a");
        RefPtr<RefLogger> ref(&a);
        std::experimental::variant<RefPtr<RefLogger>, int> v = ref;
    }

    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");
        RefPtr<RefLogger> ref(&a);
        std::experimental::variant<RefPtr<RefLogger>, int> v = WTFMove(ref);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_Variant, Ref)
{
    {
        RefLogger a("a");
        Ref<RefLogger> ref(a);
        std::experimental::variant<Ref<RefLogger>, int> v = WTFMove(ref);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

}
