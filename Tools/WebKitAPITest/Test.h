/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Test_h
#define Test_h

#include "TestsController.h"

namespace WebKitAPITest {

// Abstract base class that all tests inherit from.
class Test {
public:
    ~Test() { }
    virtual const char* name() const = 0;
    virtual void run() = 0;
};

#define TEST_CLASS_NAME(testCaseName, testName) testCaseName##_##testName##_Test

// Use this to define a new test.
#define TEST(testCaseName, testName) \
    class TEST_CLASS_NAME(testCaseName, testName) : public Test { \
    public: \
        virtual const char* name() const { return #testCaseName ": " #testName; } \
        virtual void run(); \
        static const bool initialized; \
    }; \
    \
    const bool TEST_CLASS_NAME(testCaseName, testName)::initialized = (TestsController::shared().addTest(new TEST_CLASS_NAME(testCaseName, testName)), true); \
    \
    void TEST_CLASS_NAME(testCaseName, testName)::run()

#define TEST_ASSERT(expression) do { if (!(expression)) { TestsController::shared().testFailed(__FILE__, __LINE__, #expression); return; } } while (0)

} // namespace WebKitAPITest

#endif // Test_h
