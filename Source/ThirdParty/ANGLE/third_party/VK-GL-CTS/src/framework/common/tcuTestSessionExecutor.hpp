#ifndef _TCUTESTSESSIONEXECUTOR_HPP
#define _TCUTESTSESSIONEXECUTOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Test executor.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestPackage.hpp"
#include "tcuTestHierarchyIterator.hpp"
#include "deUniquePtr.hpp"
#include <map>

namespace tcu
{

class TestSessionExecutor
{
public:
    TestSessionExecutor(TestPackageRoot &root, TestContext &testCtx);
    ~TestSessionExecutor(void);

    bool iterate(void);

    bool isInTestCase(void) const
    {
        return m_isInTestCase;
    }
    const TestRunStatus &getStatus(void) const
    {
        return m_status;
    }

private:
    void enterTestPackage(TestPackage *testPackage);
    void leaveTestPackage(TestPackage *testPackage);

    void enterTestGroup(const std::string &casePath);
    void leaveTestGroup(const std::string &casePath);

    bool enterTestCase(TestCase *testCase, const std::string &casePath);
    TestCase::IterateResult iterateTestCase(TestCase *testCase);
    void leaveTestCase(TestCase *testCase);

    enum State
    {
        STATE_TRAVERSE_HIERARCHY = 0,
        STATE_EXECUTE_TEST_CASE,

        STATE_LAST
    };

    TestContext &m_testCtx;

    DefaultHierarchyInflater m_inflater;
    de::MovePtr<CaseListFilter> m_caseListFilter;
    TestHierarchyIterator m_iterator;

    de::MovePtr<TestCaseExecutor> m_caseExecutor;
    TestRunStatus m_status;
    State m_state;
    bool m_abortSession;
    bool m_isInTestCase;
    uint64_t m_testStartTime;
    uint64_t m_packageStartTime;
    std::map<std::string, uint64_t> m_groupsDurationTime;
};

} // namespace tcu

#endif // _TCUTESTSESSIONEXECUTOR_HPP
