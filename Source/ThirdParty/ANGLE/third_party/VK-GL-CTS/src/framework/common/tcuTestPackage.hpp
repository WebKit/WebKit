#ifndef _TCUTESTPACKAGE_HPP
#define _TCUTESTPACKAGE_HPP
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
 * \brief Base class for a test case.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include <map>

namespace tcu
{

//! Test run summary.
class TestRunStatus
{
public:
    TestRunStatus(void)
    {
        clear();
    }

    void clear(void)
    {
        numExecuted     = 0;
        numPassed       = 0;
        numFailed       = 0;
        numDeviceLost   = 0;
        numNotSupported = 0;
        numWarnings     = 0;
        numWaived       = 0;
        isComplete      = false;
    }

    int numExecuted;     //!< Total number of cases executed.
    int numPassed;       //!< Number of cases passed.
    int numFailed;       //!< Number of cases failed.
    int numNotSupported; //!< Number of cases not supported.
    int numWarnings;     //!< Number of QualityWarning / CompatibilityWarning results.
    int numWaived;       //!< Number of waived tests.
    int numDeviceLost;   //!< Number of cases that caused a device lost
    bool isComplete;     //!< Is run complete.
};

/*--------------------------------------------------------------------*//*!
 * \brief Test case execution interface.
 *
 * TestCaseExecutor provides package-specific resources & initialization
 * for test cases.
 *
 * \todo [2015-03-18 pyry] Replace with following API:
 *
 * class TestInstance
 * {
 * public:
 *     TestInstance (TestContext& testCtx);
 *     tcu::TestResult iterate (void);
 * };
 *
 * class TestInstanceFactory (???)
 * {
 * public:
 *     TestInstance* createInstance (const TestCase* testCase, const std::string& path);
 * };
 *//*--------------------------------------------------------------------*/
class TestCaseExecutor
{
public:
    virtual ~TestCaseExecutor(void)
    {
    }

    virtual void init(TestCase *testCase, const std::string &path) = 0;
    virtual void deinit(TestCase *testCase)                        = 0;
    virtual TestNode::IterateResult iterate(TestCase *testCase)    = 0;
    virtual void deinitTestPackage(TestContext &testCtx)
    {
        DE_UNREF(testCtx);
    };
    virtual bool usesLocalStatus(TestContext &testCtx)
    {
        DE_UNREF(testCtx);
        return false;
    }
    virtual void updateGlobalStatus(tcu::TestRunStatus &status)
    {
        DE_UNREF(status);
    }
    virtual void reportDurations(tcu::TestContext &testCtx, const std::string &packageName, const int64_t &duration,
                                 const std::map<std::string, uint64_t> &groupsDurationTime)
    {
        DE_UNREF(testCtx);
        DE_UNREF(packageName);
        DE_UNREF(duration);
        DE_UNREF(groupsDurationTime);
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Base class for test packages.
 *
 * Test packages are root-level test groups. They also provide package-
 * specific test case executor, see TestCaseExecutor.
 *//*--------------------------------------------------------------------*/
class TestPackage : public TestNode
{
public:
    TestPackage(TestContext &testCtx, const char *name, const char *description);
    virtual ~TestPackage(void);

    virtual TestCaseExecutor *createExecutor(void) const = 0;

    // Deprecated
    virtual Archive *getArchive(void)
    {
        return nullptr;
    }

    virtual IterateResult iterate(void);

    void setCaseListFilter(const CaseListFilter *caseListFilter);

protected:
    const CaseListFilter *m_caseListFilter;
};

// TestPackageRegistry

typedef TestPackage *(*TestPackageCreateFunc)(TestContext &testCtx);

class TestPackageRegistry
{
public:
    struct PackageInfo
    {
        PackageInfo(std::string name_, TestPackageCreateFunc createFunc_) : name(name_), createFunc(createFunc_)
        {
        }

        std::string name;
        TestPackageCreateFunc createFunc;
    };

    static TestPackageRegistry *getSingleton(void);
    static void destroy(void);

    void registerPackage(const char *name, TestPackageCreateFunc createFunc);
    const std::vector<PackageInfo *> &getPackageInfos(void) const;
    PackageInfo *getPackageInfoByName(const char *name) const;
    TestPackage *createPackage(const char *name, TestContext &testCtx) const;

private:
    TestPackageRegistry(void);
    ~TestPackageRegistry(void);

    static TestPackageRegistry *getOrDestroy(bool isCreate);

    // Member variables.
    std::vector<PackageInfo *> m_packageInfos;
};

// TestPackageDescriptor

class TestPackageDescriptor
{
public:
    TestPackageDescriptor(const char *name, TestPackageCreateFunc createFunc);
    ~TestPackageDescriptor(void);
};

// TestPackageRoot

class TestPackageRoot : public TestNode
{
public:
    TestPackageRoot(TestContext &testCtx);
    TestPackageRoot(TestContext &testCtx, const std::vector<TestNode *> &children);
    TestPackageRoot(TestContext &testCtx, const TestPackageRegistry *packageRegistry);
    virtual ~TestPackageRoot(void);

    virtual IterateResult iterate(void);
};

} // namespace tcu

#endif // _TCUTESTPACKAGE_HPP
