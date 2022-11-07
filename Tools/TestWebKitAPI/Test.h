/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <type_traits>
#include <wtf/ASCIICType.h>
#include <wtf/text/WTFString.h>

#if ASSERT_ENABLED
#define MAYBE_ASSERT_ENABLED_DEATH_TEST(name) name##DeathTest
#else
#define MAYBE_ASSERT_ENABLED_DEATH_TEST(name) DISABLED_##name##DeathTest
#endif

#define EXPECT_NOT_NULL(expression) \
    EXPECT_TRUE(expression)

#define EXPECT_NULL(expression) \
    EXPECT_TRUE(!(expression))

#define ASSERT_NOT_NULL(expression) \
    ASSERT_TRUE(expression)

#define ASSERT_NULL(expression) \
    ASSERT_TRUE(!(expression))

#define EXPECT_STRONG_ENUM_EQ(expected, actual) \
    EXPECT_PRED_FORMAT2(TestWebKitAPI::assertStrongEnum, expected, actual)

namespace TestWebKitAPI {

template<typename T>
static inline ::testing::AssertionResult assertStrongEnum(const char* expected_expression, const char* actual_expression, T expected, T actual)
{
    static_assert(std::is_enum<T>::value, "T is not an enum type");
    typedef typename std::underlying_type<T>::type UnderlyingStorageType;
    return ::testing::internal::CmpHelperEQ(expected_expression, actual_expression, static_cast<UnderlyingStorageType>(expected), static_cast<UnderlyingStorageType>(actual));
}


// Test parameter formatter for the parameter-generated part of the
// name of value-parametrized tests.
// Clients should implement `void PrintTo(TheParameterType value, ::std::ostream* o)`
// in the same namespace as the TheParameterType.
struct TestParametersToStringFormatter {
    template <class ParamType>
    std::string operator()(const testing::TestParamInfo<ParamType> &info) const
    {
        std::string name = testing::PrintToStringParamName()(info);
        std::string sanitized;
        for (const char c : name) {
            if (isASCIIAlphanumeric(c))
                sanitized += c;
        }
        return sanitized;
    }
};

} // namespace TestWebKitAPI

namespace WTF {

inline std::ostream& operator<<(std::ostream& os, const String& string)
{
    return os << string.utf8().data();
}

}
