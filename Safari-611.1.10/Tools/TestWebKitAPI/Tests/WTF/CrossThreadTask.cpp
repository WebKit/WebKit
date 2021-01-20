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

#include <wtf/CrossThreadTask.h>
#include <wtf/HashCountedSet.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

static size_t totalDestructorCalls;
static size_t totalIsolatedCopyCalls;

static HashCountedSet<String> defaultConstructorSet;
static HashCountedSet<String> nameConstructorSet;
static HashCountedSet<String> copyConstructorSet;
static HashCountedSet<String> moveConstructorSet;

struct LifetimeLogger {
    LifetimeLogger()
    {
        defaultConstructorSet.add(fullName());
    }

    LifetimeLogger(const char* inputName)
        : name(*inputName)
    {
        nameConstructorSet.add(fullName());
    }

    LifetimeLogger(const LifetimeLogger& other)
        : name(other.name)
        , copyGeneration(other.copyGeneration + 1)
        , moveGeneration(other.moveGeneration)
    {
        copyConstructorSet.add(fullName());
    }

    LifetimeLogger(LifetimeLogger&& other)
        : name(other.name)
        , copyGeneration(other.copyGeneration)
        , moveGeneration(other.moveGeneration + 1)
    {
        moveConstructorSet.add(fullName());
    }

    ~LifetimeLogger()
    {
        ++totalDestructorCalls;
    }

    LifetimeLogger isolatedCopy() const
    {
        ++totalIsolatedCopyCalls;
        return LifetimeLogger(*this);
    }

    String fullName()
    {
        StringBuilder builder;
        builder.append(&name);
        builder.append("-");
        builder.append(String::number(copyGeneration));
        builder.append("-");
        builder.append(String::number(moveGeneration));

        return builder.toString();
    }

    const char& name { *"<default>" };
    int copyGeneration { 0 };
    int moveGeneration { 0 };
};

void testFunction(const LifetimeLogger&, const LifetimeLogger&, const LifetimeLogger&)
{
    // Do nothing - Just need to check the side effects of the arguments getting in here.
}

TEST(WTF_CrossThreadTask, Basic)
{
    {
        LifetimeLogger logger1;
        LifetimeLogger logger2(logger1);
        LifetimeLogger logger3("logger");

        auto task = createCrossThreadTask(testFunction, logger1, logger2, logger3);
        task.performTask();
    }

    ASSERT_EQ(1u, defaultConstructorSet.size());
    ASSERT_EQ(1u, defaultConstructorSet.count("<default>-0-0"));

    ASSERT_EQ(1u, nameConstructorSet.size());
    ASSERT_EQ(1u, nameConstructorSet.count("logger-0-0"));

    ASSERT_EQ(3u, copyConstructorSet.size());
    ASSERT_EQ(1u, copyConstructorSet.count("logger-1-0"));
    ASSERT_EQ(2u, copyConstructorSet.count("<default>-1-0"));
    ASSERT_EQ(1u, copyConstructorSet.count("<default>-2-0"));

#if !COMPILER(MSVC)
    ASSERT_EQ(6u, moveConstructorSet.size());
#else
    // The number of times the move constructor is called is different on Windows in this test.
    // This seems to be caused by differences in MSVC's implementation of lambdas or std functions like std::make_tuple.
    ASSERT_EQ(9u, moveConstructorSet.size());
#endif
    ASSERT_EQ(1u, moveConstructorSet.count("logger-1-1"));
    ASSERT_EQ(1u, moveConstructorSet.count("logger-1-2"));
    ASSERT_EQ(1u, moveConstructorSet.count("<default>-2-1"));
    ASSERT_EQ(1u, moveConstructorSet.count("<default>-2-2"));
    ASSERT_EQ(1u, moveConstructorSet.count("<default>-1-1"));
    ASSERT_EQ(1u, moveConstructorSet.count("<default>-1-2"));

#if !COMPILER(MSVC)
    ASSERT_EQ(12u, totalDestructorCalls);
#else
    // Since the move constructor is called 3 more times on Windows (see above), we will have 3 more destructor calls.
    ASSERT_EQ(15u, totalDestructorCalls);
#endif
    ASSERT_EQ(3u, totalIsolatedCopyCalls);

}
    
} // namespace TestWebKitAPI
