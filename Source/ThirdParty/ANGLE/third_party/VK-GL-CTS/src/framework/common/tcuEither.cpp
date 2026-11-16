/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Template class that is either type of Left or Right.
 *//*--------------------------------------------------------------------*/

#include "tcuEither.hpp"

namespace tcu
{
namespace
{

enum
{
    COPYCHECK_VALUE = 1637423219
};

class TestClassWithConstructor
{
public:
    TestClassWithConstructor(int i) : m_i(i), m_copyCheck(COPYCHECK_VALUE)
    {
    }

    ~TestClassWithConstructor(void)
    {
        DE_TEST_ASSERT(m_copyCheck == COPYCHECK_VALUE);
    }

    TestClassWithConstructor(const TestClassWithConstructor &other) : m_i(other.m_i), m_copyCheck(other.m_copyCheck)
    {
    }

    TestClassWithConstructor &operator=(const TestClassWithConstructor &other)
    {
        TCU_CHECK(m_copyCheck == COPYCHECK_VALUE);

        if (this == &other)
            return *this;

        m_i         = other.m_i;
        m_copyCheck = other.m_copyCheck;

        TCU_CHECK(m_copyCheck == COPYCHECK_VALUE);

        return *this;
    }

    int getValue(void) const
    {
        TCU_CHECK(m_copyCheck == COPYCHECK_VALUE);

        return m_i;
    }

private:
    int m_i;
    int m_copyCheck;
};

} // namespace

void Either_selfTest(void)
{
    // Simple test for first
    {
        const int intValue = 1503457782;
        const Either<int, float> either(intValue);

        TCU_CHECK(either.isFirst());
        TCU_CHECK(!either.isSecond());

        TCU_CHECK(either.is<int>());
        TCU_CHECK(!either.is<float>());

        TCU_CHECK(either.getFirst() == intValue);
        TCU_CHECK(either.get<int>() == intValue);
    }

    // Simple test for second
    {
        const float floatValue = 0.43223332995f;
        const Either<int, float> either(floatValue);

        TCU_CHECK(!either.isFirst());
        TCU_CHECK(either.isSecond());

        TCU_CHECK(!either.is<int>());
        TCU_CHECK(either.is<float>());

        TCU_CHECK(either.getSecond() == floatValue);
        TCU_CHECK(either.get<float>() == floatValue);
    }

    // Assign first value
    {
        const int intValue     = 1942092699;
        const float floatValue = 0.43223332995f;
        Either<int, float> either(floatValue);

        either = intValue;

        TCU_CHECK(either.isFirst());
        TCU_CHECK(!either.isSecond());

        TCU_CHECK(either.is<int>());
        TCU_CHECK(!either.is<float>());

        TCU_CHECK(either.getFirst() == intValue);
        TCU_CHECK(either.get<int>() == intValue);
    }

    // Assign second value
    {
        const int intValue     = 1942092699;
        const float floatValue = 0.43223332995f;
        Either<int, float> either(intValue);

        either = floatValue;

        TCU_CHECK(!either.isFirst());
        TCU_CHECK(either.isSecond());

        TCU_CHECK(!either.is<int>());
        TCU_CHECK(either.is<float>());

        TCU_CHECK(either.getSecond() == floatValue);
        TCU_CHECK(either.get<float>() == floatValue);
    }

    // Assign first either value
    {
        const int intValue     = 1942092699;
        const float floatValue = 0.43223332995f;
        Either<int, float> either(floatValue);
        const Either<int, float> otherEither(intValue);

        either = otherEither;

        TCU_CHECK(either.isFirst());
        TCU_CHECK(!either.isSecond());

        TCU_CHECK(either.is<int>());
        TCU_CHECK(!either.is<float>());

        TCU_CHECK(either.getFirst() == intValue);
        TCU_CHECK(either.get<int>() == intValue);
    }

    // Assign second either value
    {
        const int intValue     = 1942092699;
        const float floatValue = 0.43223332995f;
        Either<int, float> either(intValue);
        const Either<int, float> otherEither(floatValue);

        either = otherEither;

        TCU_CHECK(!either.isFirst());
        TCU_CHECK(either.isSecond());

        TCU_CHECK(!either.is<int>());
        TCU_CHECK(either.is<float>());

        TCU_CHECK(either.getSecond() == floatValue);
        TCU_CHECK(either.get<float>() == floatValue);
    }

    // Simple test for first with constructor
    {
        const TestClassWithConstructor testObject(171899615);
        const Either<TestClassWithConstructor, int> either(testObject);

        TCU_CHECK(either.isFirst());
        TCU_CHECK(!either.isSecond());

        TCU_CHECK(either.is<TestClassWithConstructor>());
        TCU_CHECK(!either.is<int>());

        TCU_CHECK(either.getFirst().getValue() == testObject.getValue());
        TCU_CHECK(either.get<TestClassWithConstructor>().getValue() == testObject.getValue());
    }

    // Simple test for second with constructor
    {
        const TestClassWithConstructor testObject(171899615);
        const Either<int, TestClassWithConstructor> either(testObject);

        TCU_CHECK(!either.isFirst());
        TCU_CHECK(either.isSecond());

        TCU_CHECK(either.is<TestClassWithConstructor>());
        TCU_CHECK(!either.is<int>());

        TCU_CHECK(either.getSecond().getValue() == testObject.getValue());
        TCU_CHECK(either.get<TestClassWithConstructor>().getValue() == testObject.getValue());
    }

    // Assign first with constructor
    {
        const int intValue = 1942092699;
        const TestClassWithConstructor testObject(171899615);
        Either<TestClassWithConstructor, int> either(intValue);

        either = testObject;

        TCU_CHECK(either.isFirst());
        TCU_CHECK(!either.isSecond());

        TCU_CHECK(either.is<TestClassWithConstructor>());
        TCU_CHECK(!either.is<int>());

        TCU_CHECK(either.getFirst().getValue() == testObject.getValue());
        TCU_CHECK(either.get<TestClassWithConstructor>().getValue() == testObject.getValue());
    }

    // Assign second with constructor
    {
        const int intValue = 1942092699;
        const TestClassWithConstructor testObject(171899615);
        Either<int, TestClassWithConstructor> either(intValue);

        either = testObject;

        TCU_CHECK(!either.isFirst());
        TCU_CHECK(either.isSecond());

        TCU_CHECK(either.is<TestClassWithConstructor>());
        TCU_CHECK(!either.is<int>());

        TCU_CHECK(either.getSecond().getValue() == testObject.getValue());
        TCU_CHECK(either.get<TestClassWithConstructor>().getValue() == testObject.getValue());
    }

    // Assign first either with constructor
    {
        const int intValue = 1942092699;
        const TestClassWithConstructor testObject(171899615);
        Either<TestClassWithConstructor, int> either(intValue);
        const Either<TestClassWithConstructor, int> otherEither(testObject);

        either = otherEither;

        TCU_CHECK(either.isFirst());
        TCU_CHECK(!either.isSecond());

        TCU_CHECK(either.is<TestClassWithConstructor>());
        TCU_CHECK(!either.is<int>());

        TCU_CHECK(either.getFirst().getValue() == testObject.getValue());
        TCU_CHECK(either.get<TestClassWithConstructor>().getValue() == testObject.getValue());
    }

    // Assign second either with constructor
    {
        const int intValue = 1942092699;
        const TestClassWithConstructor testObject(171899615);
        Either<int, TestClassWithConstructor> either(intValue);
        const Either<int, TestClassWithConstructor> otherEither(testObject);

        either = otherEither;

        TCU_CHECK(!either.isFirst());
        TCU_CHECK(either.isSecond());

        TCU_CHECK(either.is<TestClassWithConstructor>());
        TCU_CHECK(!either.is<int>());

        TCU_CHECK(either.getSecond().getValue() == testObject.getValue());
        TCU_CHECK(either.get<TestClassWithConstructor>().getValue() == testObject.getValue());
    }
}

} // namespace tcu
