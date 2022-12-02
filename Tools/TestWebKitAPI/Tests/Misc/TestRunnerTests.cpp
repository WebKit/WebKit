/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Test.h"

namespace TestWebKitAPI {

namespace {

enum class TestEnum1 {
    Cat, Dog
};

enum class TestEnum2 {
    Red, Green
};

void PrintTo(TestEnum1 value, ::std::ostream* o)
{
    if (value == TestEnum1::Cat)
        *o << "Cat";
    else if (value == TestEnum1::Dog)
        *o << "Dog";
    else
        *o << "Unknown";
}

void PrintTo(TestEnum2 value, ::std::ostream* o)
{
    if (value == TestEnum2::Red)
        *o << "Red";
    else if (value == TestEnum2::Green)
        *o << "Green";
    else
        *o << "Unknown";
}

class ValueParametrizedTest : public testing::TestWithParam<std::tuple<TestEnum1, TestEnum2>> {
public:
    TestEnum1 enum1() const { return std::get<0>(GetParam()); }
    TestEnum2 enum2() const { return std::get<1>(GetParam()); }
};

}

// Tests that WebKitTestRunner supports value-parametrized tests.
// See: https://github.com/google/googletest/blob/master/docs/advanced.md#value-parameterized-tests
// At the time of writing the test runner python script couldn't parse the
// results of --gtest_list_tests.
TEST_P(ValueParametrizedTest, ValueParametrizedTestsSupported)
{
    EXPECT_TRUE(true);
}

INSTANTIATE_TEST_SUITE_P(Misc,
    ValueParametrizedTest,
    testing::Combine(
        testing::Values(TestEnum1::Cat, TestEnum1::Dog),
        testing::Values(TestEnum2::Red, TestEnum2::Green)),
    TestParametersToStringFormatter());

} // namespace TestWebKitAPI
