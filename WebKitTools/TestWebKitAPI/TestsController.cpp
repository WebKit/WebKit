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

#include "TestsController.h"

#include "Test.h"
#include <algorithm>
#include <assert.h>

namespace TestWebKitAPI {

TestsController& TestsController::shared()
{
    static TestsController& shared = *new TestsController;
    return shared;
}

TestsController::TestsController()
    : m_testFailed(false)
    , m_currentTest(0)
{
}

bool TestsController::runTestNamed(const std::string& identifier)
{
    CreateTestFunction createTestFunction = m_createTestFunctions[identifier];
    if (!createTestFunction) {
        printf("ERROR: Test not found - %s\n", identifier.c_str());
        return false;
    }

    m_currentTest = createTestFunction(identifier);
    m_currentTest->run();

    if (!m_testFailed)
        printf("PASS: %s\n", m_currentTest->name().c_str());

    delete m_currentTest;
    m_currentTest = 0;

    return !m_testFailed;
}

void TestsController::testFailed(const char* file, int line, const char* message)
{
    m_testFailed = true;
    printf("FAIL: %s\n\t%s (%s:%d)\n", m_currentTest->name().c_str(), message, file, line);
}

void TestsController::registerCreateTestFunction(const std::string& identifier, CreateTestFunction createTestFunction)
{
    m_createTestFunctions[identifier] = createTestFunction;
}

} // namespace TestWebKitAPI
