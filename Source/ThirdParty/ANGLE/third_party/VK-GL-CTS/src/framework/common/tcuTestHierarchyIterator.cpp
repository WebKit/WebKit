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
 * \brief Test case hierarchy iterator.
 *//*--------------------------------------------------------------------*/

#include "tcuTestHierarchyIterator.hpp"
#include "tcuCommandLine.hpp"

namespace tcu
{

using std::string;
using std::vector;

// TestHierarchyInflater

TestHierarchyInflater::TestHierarchyInflater(void)
{
}

TestHierarchyInflater::~TestHierarchyInflater(void)
{
}

// DefaultHierarchyInflater

DefaultHierarchyInflater::DefaultHierarchyInflater(TestContext &testCtx) : m_testCtx(testCtx)
{
}

DefaultHierarchyInflater::~DefaultHierarchyInflater(void)
{
}

void DefaultHierarchyInflater::enterTestPackage(TestPackage *testPackage, vector<TestNode *> &children)
{
    {
        Archive *const pkgArchive = testPackage->getArchive();

        if (pkgArchive)
            m_testCtx.setCurrentArchive(*pkgArchive);
        else
            m_testCtx.setCurrentArchive(m_testCtx.getRootArchive());
    }

    testPackage->init();
    testPackage->getChildren(children);

    // write default session info if it was not done by package
    m_testCtx.writeSessionInfo();
}

void DefaultHierarchyInflater::leaveTestPackage(TestPackage *testPackage)
{
    m_testCtx.setCurrentArchive(m_testCtx.getRootArchive());
    testPackage->deinit();
}

void DefaultHierarchyInflater::enterGroupNode(TestCaseGroup *testGroup, vector<TestNode *> &children)
{
    testGroup->init();
    testGroup->getChildren(children);
}

void DefaultHierarchyInflater::leaveGroupNode(TestCaseGroup *testGroup)
{
    testGroup->deinit();
}

// TestHierarchyIterator

TestHierarchyIterator::TestHierarchyIterator(TestPackageRoot &rootNode, TestHierarchyInflater &inflater,
                                             const CaseListFilter &caseListFilter)
    : m_inflater(inflater)
    , m_caseListFilter(caseListFilter)
    , m_groupNumber(0)
{
    // Init traverse state and "seek" to first reportable node.
    NodeIter iter(&rootNode);
    iter.setState(NodeIter::NISTATE_ENTER); // Root is never reported
    m_sessionStack.push_back(iter);
    next();
}

TestHierarchyIterator::~TestHierarchyIterator(void)
{
    // Tear down inflated nodes in m_sessionStack
    for (vector<NodeIter>::reverse_iterator iter = m_sessionStack.rbegin(); iter != m_sessionStack.rend(); ++iter)
    {
        TestNode *const node        = iter->node;
        const TestNodeType nodeType = node->getNodeType();

        switch (nodeType)
        {
        case NODETYPE_ROOT: /* root is not de-initialized */
            break;
        case NODETYPE_PACKAGE:
            m_inflater.leaveTestPackage(static_cast<TestPackage *>(node));
            break;
        case NODETYPE_GROUP:
            m_inflater.leaveGroupNode(static_cast<TestCaseGroup *>(node));
            break;
        default:
            break;
        }
    }
}

TestHierarchyIterator::State TestHierarchyIterator::getState(void) const
{
    if (!m_sessionStack.empty())
    {
        const NodeIter &iter = m_sessionStack.back();

        DE_ASSERT(iter.getState() == NodeIter::NISTATE_ENTER || iter.getState() == NodeIter::NISTATE_LEAVE);

        return iter.getState() == NodeIter::NISTATE_ENTER ? STATE_ENTER_NODE : STATE_LEAVE_NODE;
    }
    else
        return STATE_FINISHED;
}

TestNode *TestHierarchyIterator::getNode(void) const
{
    DE_ASSERT(getState() != STATE_FINISHED);
    return m_sessionStack.back().node;
}

const std::string &TestHierarchyIterator::getNodePath(void) const
{
    DE_ASSERT(getState() != STATE_FINISHED);
    return m_nodePath;
}

std::string TestHierarchyIterator::buildNodePath(const vector<NodeIter> &nodeStack)
{
    string nodePath;
    for (size_t ndx = 1; ndx < nodeStack.size(); ndx++)
    {
        const NodeIter &iter = nodeStack[ndx];
        if (ndx > 1) // ignore root package
            nodePath += ".";
        nodePath += iter.node->getName();
    }
    return nodePath;
}

void TestHierarchyIterator::next(void)
{
    while (!m_sessionStack.empty())
    {
        NodeIter &iter       = m_sessionStack.back();
        TestNode *const node = iter.node;
        const bool isLeaf    = isTestNodeTypeExecutable(node->getNodeType());

        switch (iter.getState())
        {
        case NodeIter::NISTATE_INIT:
        {
            const std::string nodePath = buildNodePath(m_sessionStack);

            // Return to parent if name or runner type doesn't match filter.
            if (!(isLeaf ? (m_caseListFilter.checkRunnerType(node->getRunnerType()) &&
                            m_caseListFilter.checkTestCaseName(nodePath.c_str())) :
                           m_caseListFilter.checkTestGroupName(nodePath.c_str())))
            {
                m_sessionStack.pop_back();
                break;
            }

            m_nodePath = nodePath;
            iter.setState(NodeIter::NISTATE_ENTER);
            return; // Yield enter event
        }

        case NodeIter::NISTATE_ENTER:
        {
            if (isLeaf)
            {
                iter.setState(NodeIter::NISTATE_LEAVE);
                return; // Yield leave event
            }
            else
            {
                iter.setState(NodeIter::NISTATE_TRAVERSE_CHILDREN);
                iter.children.clear();

                switch (node->getNodeType())
                {
                case NODETYPE_ROOT:
                    static_cast<TestPackageRoot *>(node)->getChildren(iter.children);
                    break;
                case NODETYPE_PACKAGE:
                    m_inflater.enterTestPackage(static_cast<TestPackage *>(node), iter.children);
                    break;
                case NODETYPE_GROUP:
                    m_inflater.enterGroupNode(static_cast<TestCaseGroup *>(node), iter.children);
                    break;
                default:
                    DE_ASSERT(false);
                }
            }

            break;
        }

        case NodeIter::NISTATE_TRAVERSE_CHILDREN:
        {
            int numChildren = (int)iter.children.size();
            if (++iter.curChildNdx < numChildren)
            {
                // Push child to stack.
                TestNode *childNode = iter.children[iter.curChildNdx];

                // Check whether this is a bottom-level group (child is executable)
                // and whether that group should be filtered out.
                if (isTestNodeTypeExecutable(childNode->getNodeType()))
                {
                    const std::string testName = m_nodePath + "." + childNode->getName();
                    if (!m_caseListFilter.checkCaseFraction(m_groupNumber, testName, node->getUseFraction0()))
                        break;
                }
                m_sessionStack.push_back(NodeIter(childNode));
            }
            else
            {
                iter.setState(NodeIter::NISTATE_LEAVE);
                if (node->getNodeType() != NODETYPE_ROOT)
                    return; // Yield leave event
            }

            break;
        }

        case NodeIter::NISTATE_LEAVE:
        {
            // Leave node.
            if (!isLeaf)
            {
                switch (node->getNodeType())
                {
                case NODETYPE_ROOT: /* root is not de-initialized */
                    break;
                case NODETYPE_PACKAGE:
                    m_inflater.leaveTestPackage(static_cast<TestPackage *>(node));
                    break;
                case NODETYPE_GROUP:
                    m_inflater.leaveGroupNode(static_cast<TestCaseGroup *>(node));
                    break;
                default:
                    DE_ASSERT(false);
                }
                m_groupNumber++;
            }

            m_sessionStack.pop_back();
            m_nodePath = buildNodePath(m_sessionStack);
            break;
        }

        default:
            DE_ASSERT(false);
            return;
        }
    }

    DE_ASSERT(m_sessionStack.empty() && getState() == STATE_FINISHED);
}

} // namespace tcu
