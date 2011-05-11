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
#include <assert.h>
#include <gtest/gtest.h>
#include <cstdio>

namespace TestWebKitAPI {

TestsController& TestsController::shared()
{
    static TestsController& shared = *new TestsController;
    return shared;
}

TestsController::TestsController()
{
    int argc = 0;
    ::testing::InitGoogleTest(&argc, (char**)0);
}

void TestsController::dumpTestNames()
{
    ::testing::UnitTest* unit_test = ::testing::UnitTest::GetInstance();

    for (int i = 0; i < unit_test->total_test_case_count(); i++) {
        const ::testing::TestCase* test_case = unit_test->GetTestCase(i);
        for (int j = 0; j < test_case->total_test_count(); j++) {
          const ::testing::TestInfo* test_info = test_case->GetTestInfo(j);
          printf("%s.%s\n", test_case->name(), test_info->name());
        }
    }
}

bool TestsController::runTestNamed(const std::string& identifier)
{
    ::testing::GTEST_FLAG(filter) = identifier;
    return !RUN_ALL_TESTS();
}

bool TestsController::runAllTests()
{
    return !RUN_ALL_TESTS();
}

} // namespace TestWebKitAPI
