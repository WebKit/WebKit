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

#include "tcuTestPackage.hpp"
#include "tcuPlatform.hpp"

#include "deString.h"

using std::vector;

namespace tcu
{

// TestPackage

TestPackage::TestPackage(TestContext &testCtx, const char *name, const char *description)
    : TestNode(testCtx, NODETYPE_PACKAGE, name)
    , m_caseListFilter(nullptr)
{
    DE_UNREF(description);
}

TestPackage::~TestPackage(void)
{
}

void TestPackage::setCaseListFilter(const CaseListFilter *caseListFilter)
{
    m_caseListFilter = caseListFilter;
}

TestNode::IterateResult TestPackage::iterate(void)
{
    DE_ASSERT(false); // should never be here!
    throw InternalError("TestPackage::iterate() called!", "", __FILE__, __LINE__);
}

// TestPackageRegistry

TestPackageRegistry::TestPackageRegistry(void)
{
}

TestPackageRegistry::~TestPackageRegistry(void)
{
    for (int i = 0; i < (int)m_packageInfos.size(); i++)
        delete m_packageInfos[i];
}

TestPackageRegistry *TestPackageRegistry::getSingleton(void)
{
    return TestPackageRegistry::getOrDestroy(true);
}

void TestPackageRegistry::destroy(void)
{
    TestPackageRegistry::getOrDestroy(false);
}

TestPackageRegistry *TestPackageRegistry::getOrDestroy(bool isCreate)
{
    static TestPackageRegistry *s_ptr = nullptr;

    if (isCreate)
    {
        if (!s_ptr)
            s_ptr = new TestPackageRegistry();

        return s_ptr;
    }
    else
    {
        if (s_ptr)
        {
            delete s_ptr;
            s_ptr = nullptr;
        }

        return nullptr;
    }
}

void TestPackageRegistry::registerPackage(const char *name, TestPackageCreateFunc createFunc)
{
    DE_ASSERT(getPackageInfoByName(name) == nullptr);
    m_packageInfos.push_back(new PackageInfo(name, createFunc));
}

const std::vector<TestPackageRegistry::PackageInfo *> &TestPackageRegistry::getPackageInfos(void) const
{
    return m_packageInfos;
}

TestPackageRegistry::PackageInfo *TestPackageRegistry::getPackageInfoByName(const char *packageName) const
{
    for (int i = 0; i < (int)m_packageInfos.size(); i++)
    {
        if (m_packageInfos[i]->name == packageName)
            return m_packageInfos[i];
    }

    return nullptr;
}

TestPackage *TestPackageRegistry::createPackage(const char *name, TestContext &testCtx) const
{
    PackageInfo *info = getPackageInfoByName(name);
    return info ? info->createFunc(testCtx) : nullptr;
}

// TestPackageDescriptor

TestPackageDescriptor::TestPackageDescriptor(const char *name, TestPackageCreateFunc createFunc)
{
    TestPackageRegistry::getSingleton()->registerPackage(name, createFunc);
}

TestPackageDescriptor::~TestPackageDescriptor(void)
{
    TestPackageRegistry::destroy();
}

// TestPackageRoot

TestPackageRoot::TestPackageRoot(TestContext &testCtx) : TestNode(testCtx, NODETYPE_ROOT, "")
{
}

TestPackageRoot::TestPackageRoot(TestContext &testCtx, const vector<TestNode *> &children)
    : TestNode(testCtx, NODETYPE_ROOT, "", children)
{
}

TestPackageRoot::TestPackageRoot(TestContext &testCtx, const TestPackageRegistry *packageRegistry)
    : TestNode(testCtx, NODETYPE_ROOT, "")
{
    const vector<TestPackageRegistry::PackageInfo *> &packageInfos = packageRegistry->getPackageInfos();

    for (int i = 0; i < (int)packageInfos.size(); i++)
        addChild(packageInfos[i]->createFunc(testCtx));
}

TestPackageRoot::~TestPackageRoot(void)
{
}

TestCase::IterateResult TestPackageRoot::iterate(void)
{
    DE_ASSERT(false); // should never be here!
    throw InternalError("TestPackageRoot::iterate() called!", "", __FILE__, __LINE__);
}

} // namespace tcu
