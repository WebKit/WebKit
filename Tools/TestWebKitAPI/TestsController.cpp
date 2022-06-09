/*
 * Copyright (C) 2010, 2020 Apple Inc. All rights reserved.
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
#include "TestsController.h"

#include <wtf/MainThread.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomString.h>

namespace TestWebKitAPI {

class Printer : public ::testing::EmptyTestEventListener {
    virtual void OnTestPartResult(const ::testing::TestPartResult& test_part_result)
    {
        if (!test_part_result.failed())
            return;

        std::stringstream stream;
        stream << "\n";
        if (test_part_result.file_name())
            stream << test_part_result.file_name() << ":" << test_part_result.line_number();
        else
            stream << "File name unavailable";
        stream << "\n" << test_part_result.summary() << "\n\n";
        failures += stream.str();
    }

    virtual void OnTestEnd(const ::testing::TestInfo& test_info)
    {
        if (test_info.result()->Passed())
            std::cout << "**PASS** " << test_info.test_case_name() << "." << test_info.name() << "\n";
        else
            std::cout << "**FAIL** " << test_info.test_case_name() << "." << test_info.name() << "\n" << failures;

        failures = std::string();
    }
    
    std::string failures;
};

TestsController& TestsController::singleton()
{
    static NeverDestroyed<TestsController> shared;
    return shared;
}

TestsController::TestsController()
{
    // FIXME: We currently initialize threading here to avoid assertion failures from
    // the ThreadRestrictionVerifier - https://bugs.webkit.org/show_bug.cgi?id=66112
    // We should make sure that all objects tested either initialize threading or inherit from
    // ThreadSafeRefCounted so that we don't have to initialize threading at all here.
    WTF::initializeMainThread();
    WTF::setProcessPrivileges(allPrivileges());
    AtomString::init();
}

bool TestsController::run(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new Printer);

    return !RUN_ALL_TESTS();
}

} // namespace TestWebKitAPI
