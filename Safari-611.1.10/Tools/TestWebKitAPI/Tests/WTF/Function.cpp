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

#include "MoveOnly.h"
#include <wtf/Function.h>

namespace TestWebKitAPI {

static WTF::Function<int()> function_for_reentrancy_test;
static unsigned testObjectDestructorCalls = 0;

enum class AssignmentMode { Null, Lambda, None };

class TestObject {
public:
    TestObject(AssignmentMode);
    TestObject(TestObject&&);

    ~TestObject();
    int operator()();

    TestObject(const TestObject&) = delete;
    TestObject& operator=(const TestObject&) = delete;

private:
    AssignmentMode m_assignmentMode;
};

TestObject::TestObject(AssignmentMode assignmentMode)
    : m_assignmentMode(assignmentMode)
{
}

TestObject::TestObject(TestObject&& other)
{
    m_assignmentMode = std::exchange(other.m_assignmentMode, AssignmentMode::None);
}

TestObject::~TestObject()
{
    if (m_assignmentMode == AssignmentMode::None)
        return;

    ++testObjectDestructorCalls;
    if (m_assignmentMode == AssignmentMode::Null)
        function_for_reentrancy_test = nullptr;
    else
        function_for_reentrancy_test = [] { return -1; };
}

int TestObject::operator()()
{
    return 0;
}

TEST(WTF_Function, assignNullReEntersAssignNull)
{
    function_for_reentrancy_test = nullptr;
    testObjectDestructorCalls = 0;

    function_for_reentrancy_test = TestObject(AssignmentMode::Null);
    EXPECT_EQ(0, function_for_reentrancy_test());
    EXPECT_FALSE(!function_for_reentrancy_test);
    EXPECT_EQ(0U, testObjectDestructorCalls);
    function_for_reentrancy_test = nullptr;
    EXPECT_EQ(1U, testObjectDestructorCalls);
    EXPECT_TRUE(!function_for_reentrancy_test);
}

TEST(WTF_Function, assignLamdbaReEntersAssignNull)
{
    function_for_reentrancy_test = nullptr;
    testObjectDestructorCalls = 0;

    function_for_reentrancy_test = TestObject(AssignmentMode::Null);
    EXPECT_EQ(0, function_for_reentrancy_test());
    EXPECT_FALSE(!function_for_reentrancy_test);
    EXPECT_EQ(0U, testObjectDestructorCalls);
    function_for_reentrancy_test = [] { return 3; };
    EXPECT_EQ(1U, testObjectDestructorCalls);
    EXPECT_TRUE(!function_for_reentrancy_test);
}

TEST(WTF_Function, assignLamdbaReEntersAssignLamdba)
{
    function_for_reentrancy_test = nullptr;
    testObjectDestructorCalls = 0;

    function_for_reentrancy_test = TestObject(AssignmentMode::Lambda);
    EXPECT_EQ(0, function_for_reentrancy_test());
    EXPECT_FALSE(!function_for_reentrancy_test);
    EXPECT_EQ(0, function_for_reentrancy_test());
    EXPECT_EQ(0U, testObjectDestructorCalls);
    function_for_reentrancy_test = [] { return 3; };
    EXPECT_EQ(1U, testObjectDestructorCalls);
    EXPECT_FALSE(!function_for_reentrancy_test);
    EXPECT_EQ(-1, function_for_reentrancy_test());
}

TEST(WTF_Function, assignNullReEntersAssignLamda)
{
    function_for_reentrancy_test = nullptr;
    testObjectDestructorCalls = 0;

    function_for_reentrancy_test = TestObject(AssignmentMode::Lambda);
    EXPECT_EQ(0, function_for_reentrancy_test());
    EXPECT_FALSE(!function_for_reentrancy_test);
    EXPECT_EQ(0, function_for_reentrancy_test());
    EXPECT_EQ(0U, testObjectDestructorCalls);
    function_for_reentrancy_test = nullptr;
    EXPECT_EQ(1U, testObjectDestructorCalls);
    EXPECT_FALSE(!function_for_reentrancy_test);
    EXPECT_EQ(-1, function_for_reentrancy_test());
}

TEST(WTF_Function, Basics)
{
    Function<unsigned()> a;
    EXPECT_FALSE(static_cast<bool>(a));

    a = [] {
        return 1U;
    };
    EXPECT_TRUE(static_cast<bool>(a));
    EXPECT_EQ(1U, a());

    a = nullptr;
    EXPECT_FALSE(static_cast<bool>(a));

    a = MoveOnly { 2 };
    EXPECT_TRUE(static_cast<bool>(a));
    EXPECT_EQ(2U, a());

    Function<unsigned()> b = WTFMove(a);
    EXPECT_TRUE(static_cast<bool>(b));
    EXPECT_EQ(2U, b());
    EXPECT_FALSE(static_cast<bool>(a));

    a = MoveOnly { 3 };
    Function<unsigned()> c = WTFMove(a);
    EXPECT_TRUE(static_cast<bool>(c));
    EXPECT_EQ(3U, c());
    EXPECT_FALSE(static_cast<bool>(a));

    b = WTFMove(c);
    EXPECT_TRUE(static_cast<bool>(b));
    EXPECT_EQ(3U, b());
    EXPECT_FALSE(static_cast<bool>(c));
}

struct FunctionDestructionChecker {
    FunctionDestructionChecker(Function<unsigned()>& function)
        : function { function }
    {
    }

    ~FunctionDestructionChecker()
    {
        functionAsBool = static_cast<bool>(function);
        functionResult = function();
    }

    unsigned operator()() const
    {
        return 10;
    }

    Function<unsigned()>& function;
    static Optional<bool> functionAsBool;
    static Optional<unsigned> functionResult;
};

Optional<bool> FunctionDestructionChecker::functionAsBool;
Optional<unsigned> FunctionDestructionChecker::functionResult;

TEST(WTF_Function, AssignBeforeDestroy)
{
    Function<unsigned()> a;

    a = FunctionDestructionChecker(a);
    a = [] {
        return 1U;
    };
    EXPECT_TRUE(static_cast<bool>(FunctionDestructionChecker::functionAsBool));
    EXPECT_TRUE(static_cast<bool>(FunctionDestructionChecker::functionResult));
    EXPECT_TRUE(FunctionDestructionChecker::functionAsBool.value());
    EXPECT_EQ(1U, FunctionDestructionChecker::functionResult.value());
    FunctionDestructionChecker::functionAsBool = WTF::nullopt;
    FunctionDestructionChecker::functionResult = WTF::nullopt;

    a = FunctionDestructionChecker(a);
    a = MoveOnly { 2 };
    EXPECT_TRUE(static_cast<bool>(FunctionDestructionChecker::functionAsBool));
    EXPECT_TRUE(static_cast<bool>(FunctionDestructionChecker::functionResult));
    EXPECT_TRUE(FunctionDestructionChecker::functionAsBool.value());
    EXPECT_EQ(2U, FunctionDestructionChecker::functionResult.value());
    FunctionDestructionChecker::functionAsBool = WTF::nullopt;
    FunctionDestructionChecker::functionResult = WTF::nullopt;
}

static int returnThree()
{
    return 3;
}

static int returnFour()
{
    return 4;
}

static int returnPassedValue(int value)
{
    return value;
}

TEST(WTF_Function, AssignFunctionPointer)
{
    Function<int()> f1 = returnThree;
    EXPECT_TRUE(static_cast<bool>(f1));
    EXPECT_EQ(3, f1());

    f1 = returnFour;
    EXPECT_TRUE(static_cast<bool>(f1));
    EXPECT_EQ(4, f1());

    f1 = nullptr;
    EXPECT_FALSE(static_cast<bool>(f1));

    Function<int(int)> f2 = returnPassedValue;
    EXPECT_TRUE(static_cast<bool>(f2));
    EXPECT_EQ(3, f2(3));
    EXPECT_EQ(-3, f2(-3));

    f2 = nullptr;
    EXPECT_FALSE(static_cast<bool>(f2));
}

} // namespace TestWebKitAPI
