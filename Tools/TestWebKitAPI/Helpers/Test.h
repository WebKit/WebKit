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

#include <wtf/Platform.h>

#if !PLATFORM(COCOA) || !__has_feature(modules)

#include <concepts>
#include <optional>
#include <type_traits>
#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>
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

inline std::ostream& operator<<(std::ostream& os, StringImpl* string)
{
    return os << utf8(string).toStdString();
}

inline std::ostream& operator<<(std::ostream& os, const ASCIILiteral& string)
{
    return os << string.characters();
}

inline std::ostream& operator<<(std::ostream& os, const UTF8CString& string)
{
    return os << string.toStdString();
}

template<typename T> requires std::is_convertible_v<T*, const StringImpl*>
inline std::ostream& operator<<(std::ostream& os, const RefPtr<T>& string)
{
    return os << utf8(string).toStdString();
}

template<typename T> requires std::is_convertible_v<T*, const StringImpl*>
inline std::ostream& operator<<(std::ostream& os, const Ref<T>& string)
{
    return os << utf8(string).toStdString();
}

// EXPECT_EQ / EXPECT_NE compare strings by content, as they did when strings were String objects:
// - values of the same type (other than raw pointers) compare with contentEquals(), which compares StringImpl smart
//   pointers by content, atoms by identity, containers of strings element-wise, and everything else with ==;
// - other mixes compare with equal() where operator== is unavailable (StringImpl smart pointers vs. raw pointers,
//   literals, views and spans).
// Raw pointers keep comparing by identity.
template<typename T1, typename T2>
concept TestHasEqualityOperator = requires(const T1& a, const T2& b)
{
    a == b;
};

template<typename T1, typename T2>
concept TestHasContentEquals = requires(const T1& a, const T2& b)
{
    { contentEquals(a, b) } -> std::convertible_to<bool>;
};

template<typename T1, typename T2>
concept TestHasEqualFunction = requires(const T1& a, const T2& b)
{
    { equal(a, b) } -> std::convertible_to<bool>;
};

template<typename T1, typename T2>
concept TestComparesWithContentEquals = std::same_as<T1, T2> && !std::is_pointer_v<T1> && !std::is_arithmetic_v<T1> && !std::is_enum_v<T1>
    && TestHasContentEquals<T1, T2>;

template<typename T1, typename T2>
concept TestComparesStringContentsDirectly = TestComparesWithContentEquals<T1, T2>
    || (!TestHasEqualityOperator<T1, T2> && TestHasEqualFunction<T1, T2>);

template<typename T> inline constexpr bool isStdOptionalForTesting = false;
template<typename T> inline constexpr bool isStdOptionalForTesting<std::optional<T>> = true;

// An engaged std::optional of a string compares with a string the way std::optional<String> compared with a String.
template<typename T1, typename T2>
concept TestComparesOptionalStringContents = isStdOptionalForTesting<T1> && !isStdOptionalForTesting<T2>
    && !TestHasEqualityOperator<T1, T2> && TestComparesStringContentsDirectly<typename T1::value_type, T2>;

template<typename T1, typename T2>
concept TestComparesStringContents = TestComparesStringContentsDirectly<T1, T2> || TestComparesOptionalStringContents<T1, T2>;

template<typename T1, typename T2> requires (TestComparesStringContents<T1, T2>)
bool testStringContentsEqual(const T1& a, const T2& b)
{
    if constexpr (TestComparesOptionalStringContents<T1, T2>)
        return a && testStringContentsEqual(*a, b);
    else if constexpr (TestComparesWithContentEquals<T1, T2>)
        return contentEquals(a, b);
    else
        return equal(a, b);
}

// Found by argument-dependent lookup from testing::internal::EqHelper::Compare(); more constrained than gtest's.
template<typename T1, typename T2> requires (TestComparesStringContents<T1, T2>)
::testing::AssertionResult CmpHelperEQ(const char* lhsExpression, const char* rhsExpression, const T1& lhs, const T2& rhs)
{
    if (testStringContentsEqual(lhs, rhs))
        return ::testing::AssertionSuccess();
    return ::testing::internal::CmpHelperEQFailure(lhsExpression, rhsExpression, lhs, rhs);
}

}

namespace testing::internal {

// EXPECT_NE names ::testing::internal::CmpHelperNE explicitly; this overload is more constrained than gtest's.
template<typename T1, typename T2> requires (WTF::TestComparesStringContents<T1, T2>)
AssertionResult CmpHelperNE(const char* expr1, const char* expr2, const T1& val1, const T2& val2)
{
    if (!WTF::testStringContentsEqual(val1, val2))
        return AssertionSuccess();
    return CmpHelperOpFailure(expr1, expr2, val1, val2, "!=");
}

} // namespace testing::internal

#endif // !PLATFORM(COCOA) || !__has_feature(modules)
