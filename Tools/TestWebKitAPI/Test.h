/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef Test_h
#define Test_h

#include "TestsController.h"

namespace TestWebKitAPI {

// Abstract base class that all tests inherit from.
class Test {
public:
    virtual ~Test() { }
    
    virtual void run() = 0;
    std::string name() const { return m_identifier; }
    
    template<typename TestClassTy> class Register {
    public:
        Register(const std::string& testSuite, const std::string& testCase)
        {
            TestsController::shared().registerCreateTestFunction(testSuite + "/" + testCase, Register::create);
        }
    
    private:
        static Test* create(const std::string& identifier) 
        {
            return new TestClassTy(identifier);
        }
    };

protected:
    Test(const std::string& identifier)
        : m_identifier(identifier)
    {
    }

    std::string m_identifier;
};

#define TEST_CLASS_NAME(testSuite, testCaseName) testSuite##testCaseName##_Test
#define TEST_REGISTRAR_NAME(testSuite, testCaseName) testSuite##testCaseName##_Registrar

// Use this to define a new test.
#define TEST(testSuite, testCaseName) \
    class TEST_CLASS_NAME(testSuite, testCaseName) : public Test { \
    public: \
        TEST_CLASS_NAME(testSuite, testCaseName)(const std::string& identifier) \
            : Test(identifier) \
        { \
        } \
        virtual void run(); \
    }; \
    \
    static Test::Register<TEST_CLASS_NAME(testSuite, testCaseName)> TEST_REGISTRAR_NAME(testSuite, testCaseName)(#testSuite, #testCaseName); \
    \
    void TEST_CLASS_NAME(testSuite, testCaseName)::run()

#define _TEST_ASSERT_HELPER(expression, returnStatement) do { if (!(expression)) { TestsController::shared().testFailed(__FILE__, __LINE__, #expression); returnStatement; } } while (0)
#define TEST_ASSERT(expression) _TEST_ASSERT_HELPER(expression, return)
#define TEST_ASSERT_RETURN(expression, returnValue) _TEST_ASSERT_HELPER(expression, return (returnValue))

} // namespace TestWebKitAPI

#endif // Test_h
