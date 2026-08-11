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

#include "tcuTestCase.hpp"
#include "tcuCommandLine.hpp"

#include "deString.h"

#include <algorithm>

#if defined(DEQP_LOG_NODE_SOURCE)
#include <stacktrace>
#endif

namespace tcu
{

using namespace std;

// TestNode.

inline bool isValidCaseName(const char *name)
{
    for (const char *p = name; *p != '\0'; p++)
    {
        if (!isValidTestCaseNameChar(*p))
            return false;
    }
    return true;
}

TestNode::TestNode(TestContext &testCtx, TestNodeType nodeType, const char *name)
    : m_testCtx(testCtx)
    , m_name(name)
    , m_nodeType(nodeType)
    , m_children()
    , m_duplicateCheck(testCtx.getCommandLine().checkDuplicateCaseNames())
{
    DE_ASSERT(isValidCaseName(name));
}

TestNode::TestNode(TestContext &testCtx, TestNodeType nodeType, const char *name, const vector<TestNode *> &children)
    : m_testCtx(testCtx)
    , m_name(name)
    , m_nodeType(nodeType)
    , m_children()
    , m_duplicateCheck(testCtx.getCommandLine().checkDuplicateCaseNames())
{
    DE_ASSERT(isValidCaseName(name));
    for (int i = 0; i < (int)children.size(); i++)
        addChild(children[i]);
}

TestNode::~TestNode(void)
{
    TestNode::deinit();
}

void TestNode::getChildren(vector<TestNode *> &res) const
{
    res.clear();
    for (int i = 0; i < (int)m_children.size(); i++)
        res.push_back(m_children[i]);
}

void TestNode::addRootChild(const std::string &groupName, const CaseListFilter *caseListFilter,
                            TestCaseGroup *(*createTestGroup)(tcu::TestContext &testCtx, const std::string &name))
{
    // Skip tests not in case list
    if (caseListFilter && !caseListFilter->checkTestGroupName((m_name + "." + groupName).c_str()))
        return;

    return addChild(createTestGroup(m_testCtx, groupName));
}

void TestNode::addChild(TestNode *node)
{
    // children only in group nodes
    DE_ASSERT(getTestNodeTypeClass(m_nodeType) == NODECLASS_GROUP);

    // children must have the same class
    if (!m_children.empty())
        DE_ASSERT(getTestNodeTypeClass(m_children.front()->getNodeType()) == getTestNodeTypeClass(node->getNodeType()));

    // Sorted insert by child name. Keeping m_children in name order makes
    // every consumer of the tree (case-list export, hierarchy iterator,
    // mustpass) see children in deterministic alphabetical order regardless
    // of the order test code calls addChild().
    auto cmp = [](const TestNode *a, const TestNode *b) { return strcmp(a->getName(), b->getName()) < 0; };
    auto pos = std::lower_bound(m_children.begin(), m_children.end(), node, cmp);

    if (m_duplicateCheck && pos != m_children.end() && strcmp((*pos)->getName(), node->getName()) == 0)
        throw tcu::InternalError(std::string("Test case with non-unique name '") + node->getName() +
                                 "' added to group '" + getName() + "'.");

#if defined(DEQP_LOG_NODE_SOURCE)
    static const char *kHelperNames[] = {
        "addFunctionCase",
        "addInstanceTestCaseWithPrograms",
    };

    auto currentStacktrace = std::stacktrace::current();

    // Find the first parent stack frame that is not a helper, which should be the one triggering the addChild call.
    for (unsigned short i = 1; i < currentStacktrace.size(); ++i)
    {
        const auto &entry = currentStacktrace.at(i);
        const auto desc   = entry.description();
        bool isHelper     = false;

        for (const auto &helperName : kHelperNames)
        {
            if (desc.contains(helperName))
            {
                isHelper = true;
                break;
            }
        }

        if (isHelper)
            continue;

        node->setSource(entry.source_file() + ":" + std::to_string(entry.source_line()));
        break;
    }
#endif
    m_children.insert(pos, node);
}

void TestNode::init(void)
{
}

void TestNode::deinit(void)
{
    for (int i = 0; i < (int)m_children.size(); i++)
        delete m_children[i];
    m_children.clear();
}

// TestCaseGroup

TestCaseGroup::TestCaseGroup(TestContext &testCtx, const char *name) : TestNode(testCtx, NODETYPE_GROUP, name)
{
}

TestCaseGroup::TestCaseGroup(TestContext &testCtx, const char *name, const vector<TestNode *> &children)
    : TestNode(testCtx, NODETYPE_GROUP, name, children)
{
}

// Deprecated constructor with an ignored description argument. These shouldn't really be used
// in new code but are retained to avoid changing every test group construction at once.
TestCaseGroup::TestCaseGroup(TestContext &testCtx, const char *name, const char *description)
    : TestCaseGroup(testCtx, name)
{
    DE_UNREF(description);
}

TestCaseGroup::TestCaseGroup(TestContext &testCtx, const char *name, const char *description,
                             const vector<TestNode *> &children)
    : TestCaseGroup(testCtx, name, children)
{
    DE_UNREF(description);
}

TestCaseGroup::~TestCaseGroup(void)
{
}

TestCase::IterateResult TestCaseGroup::iterate(void)
{
    DE_ASSERT(false); // should never be here!
    throw InternalError("TestCaseGroup::iterate() called!", "", __FILE__, __LINE__);
}

// TestCase

TestCase::TestCase(TestContext &testCtx, const char *name) : TestNode(testCtx, NODETYPE_SELF_VALIDATE, name)
{
}

TestCase::TestCase(TestContext &testCtx, TestNodeType nodeType, const char *name) : TestNode(testCtx, nodeType, name)
{
    DE_ASSERT(isTestNodeTypeExecutable(nodeType));
}

// Deprecated constructor with an ignored description argument. These shouldn't really be used
// in new code but are retained to avoid changing every test case construction at once.
TestCase::TestCase(TestContext &testCtx, const char *name, const char *description) : TestCase(testCtx, name)
{
    DE_UNREF(description);
}

TestCase::TestCase(TestContext &testCtx, TestNodeType nodeType, const char *name, const char *description)
    : TestCase(testCtx, nodeType, name)
{
    DE_UNREF(description);
}

TestCase::~TestCase(void)
{
}

} // namespace tcu
