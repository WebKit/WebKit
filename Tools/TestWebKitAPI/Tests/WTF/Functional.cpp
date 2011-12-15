/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include <wtf/Functional.h>

namespace TestWebKitAPI {

static int returnFortyTwo()
{
    return 42;
}

TEST(FunctionalTest, Basic)
{
    Function<int ()> emptyFunction;
    ASSERT_TRUE(emptyFunction.isNull());

    Function<int ()> returnFortyTwoFunction = bind(returnFortyTwo);
    ASSERT_FALSE(returnFortyTwoFunction.isNull());
    ASSERT_EQ(42, returnFortyTwoFunction());
}

static int multiplyByTwo(int n)
{
    return n * 2;
}

static double multiplyByOneAndAHalf(double d)
{
    return d * 1.5;
}

TEST(FunctionalTest, UnaryBind)
{
    Function<int ()> multiplyFourByTwoFunction = bind(multiplyByTwo, 4);
    ASSERT_EQ(8, multiplyFourByTwoFunction());

    Function<double ()> multiplyByOneAndAHalfFunction = bind(multiplyByOneAndAHalf, 3);
    ASSERT_EQ(4.5, multiplyByOneAndAHalfFunction());
}

static int multiply(int x, int y)
{
    return x * y;
}

static int subtract(int x, int y)
{
    return x - y;
}

TEST(FunctionalTest, BinaryBind)
{
    Function<int ()> multiplyFourByTwoFunction = bind(multiply, 4, 2);
    ASSERT_EQ(8, multiplyFourByTwoFunction());

    Function<int ()> subtractTwoFromFourFunction = bind(subtract, 4, 2);
    ASSERT_EQ(2, subtractTwoFromFourFunction());
}

class A {
public:
    explicit A(int i)
        : m_i(i)
    {
    }

    int f() { return m_i; }
    int addF(int j) { return m_i + j; }

private:
    int m_i;
};

TEST(FunctionalTest, MemberFunctionBind)
{
    A a(10);
    Function<int ()> function1 = bind(&A::f, &a);
    ASSERT_EQ(10, function1());

    Function<int ()> function2 = bind(&A::addF, &a, 15);
    ASSERT_EQ(25, function2());
}

class B {
public:
    B()
        : m_numRefCalls(0)
        , m_numDerefCalls(0)
    {
    }

    ~B()
    {
        ASSERT_TRUE(m_numRefCalls == m_numDerefCalls);
        ASSERT_GT(m_numRefCalls, 0);
    }

    void ref()
    {
        m_numRefCalls++;
    }

    void deref()
    {
        m_numDerefCalls++;
    }

    void f() { ASSERT_GT(m_numRefCalls, 0); }
    void g(int) { ASSERT_GT(m_numRefCalls, 0); }

private:
    int m_numRefCalls;
    int m_numDerefCalls;
};

TEST(FunctionalTest, MemberFunctionBindRefDeref)
{
    B b;

    Function<void ()> function1 = bind(&B::f, &b);
    function1();

    Function<void ()> function2 = bind(&B::g, &b, 10);
    function2();
}

} // namespace TestWebKitAPI
