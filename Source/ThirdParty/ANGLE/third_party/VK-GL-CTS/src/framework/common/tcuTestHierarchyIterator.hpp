#ifndef _TCUTESTHIERARCHYITERATOR_HPP
#define _TCUTESTHIERARCHYITERATOR_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestPackage.hpp"

#include <vector>

namespace tcu
{

class CaseListFilter;

/*--------------------------------------------------------------------*//*!
 * \brief Test hierarchy inflater
 *
 * This interface is used by TestHierarchyIterator to materialize, and clean
 * up, test hierarchy on-demand while walking through it.
 *//*--------------------------------------------------------------------*/
class TestHierarchyInflater
{
public:
    TestHierarchyInflater(void);

    virtual void enterTestPackage(TestPackage *testPackage, std::vector<TestNode *> &children) = 0;
    virtual void leaveTestPackage(TestPackage *testPackage)                                    = 0;

    virtual void enterGroupNode(TestCaseGroup *testGroup, std::vector<TestNode *> &children) = 0;
    virtual void leaveGroupNode(TestCaseGroup *testGroup)                                    = 0;

protected:
    ~TestHierarchyInflater(void);
};

// \todo [2015-02-26 pyry] Hierarchy traversal should not depend on TestContext
class DefaultHierarchyInflater : public TestHierarchyInflater
{
public:
    DefaultHierarchyInflater(TestContext &testCtx);
    ~DefaultHierarchyInflater(void);

    virtual void enterTestPackage(TestPackage *testPackage, std::vector<TestNode *> &children);
    virtual void leaveTestPackage(TestPackage *testPackage);

    virtual void enterGroupNode(TestCaseGroup *testGroup, std::vector<TestNode *> &children);
    virtual void leaveGroupNode(TestCaseGroup *testGroup);

protected:
    TestContext &m_testCtx;
};

/*--------------------------------------------------------------------*//*!
 * \brief Test hierarchy iterator
 *
 * Test hierarchy iterator allows walking test case hierarchy in depth-first
 * order. The walked sub-tree is limited by command line parameters.
 *
 * Iterator signals current state with getState(), which initally, and after
 * each increment (next()) may report one of the following:
 *
 * STATE_ENTER_NODE: A test node has been entered to for the first time.
 *   Node can be queried with getNode() and its full path with getNodePath().
 *   For group nodes the iterator will next enter first matching child node.
 *   For executable (test case) nodes STATE_LEAVE_NODE will always be reported
 *   immediately after entering that node.
 *
 * STATE_LEAVE_NODE: Iterator is leaving a node. In case of group nodes this
 *   means that all child nodes and their children have been processed. For
 *   executable nodes the iterator will either move on to the next sibling,
 *   or leave the parent group if the reported node was last child of that
 *   group.
 *
 * Root node is never reported, but instead iteration will start on first
 * matching test package node, if there is any.
 *
 * Test hierarchy is created on demand with help of TestHierarchyInflater.
 * Upon entering a group node, after STATE_ENTER_NODE has been signaled,
 * inflater is called to construct the list of child nodes for that group.
 * Upon exiting a group node, before STATE_LEAVE_NODE is called, inflater
 * is asked to clean up any resources by calling leaveGroupNode() or
 * leaveTestPackage() depending on the type of the node.
 *//*--------------------------------------------------------------------*/
class TestHierarchyIterator
{
public:
    TestHierarchyIterator(TestPackageRoot &rootNode, TestHierarchyInflater &inflater,
                          const CaseListFilter &caseListFilter);
    ~TestHierarchyIterator(void);

    enum State
    {
        STATE_ENTER_NODE = 0,
        STATE_LEAVE_NODE,
        STATE_FINISHED,

        STATE_LAST
    };

    State getState(void) const;

    TestNode *getNode(void) const;
    const std::string &getNodePath(void) const;

    void next(void);

private:
    struct NodeIter
    {
        enum State
        {
            NISTATE_INIT = 0,
            NISTATE_ENTER,
            NISTATE_TRAVERSE_CHILDREN,
            NISTATE_LEAVE,

            NISTATE_LAST
        };

        NodeIter(void) : node(nullptr), curChildNdx(-1), m_state(NISTATE_LAST)
        {
        }

        NodeIter(TestNode *node_) : node(node_), curChildNdx(-1), m_state(NISTATE_INIT)
        {
        }

        State getState(void) const
        {
            return m_state;
        }

        void setState(State newState)
        {
            switch (newState)
            {
            case NISTATE_TRAVERSE_CHILDREN:
                curChildNdx = -1;
                break;

            default:
                break;
            }

            m_state = newState;
        }

        TestNode *node;
        std::vector<TestNode *> children;
        int curChildNdx;

    private:
        State m_state;
    };

    TestHierarchyIterator(const TestHierarchyIterator &);            // not allowed!
    TestHierarchyIterator &operator=(const TestHierarchyIterator &); // not allowed!

    bool matchFolderName(const std::string &folderName) const;
    bool matchCaseName(const std::string &caseName) const;

    static std::string buildNodePath(const std::vector<NodeIter> &nodeStack);

    TestHierarchyInflater &m_inflater;
    const CaseListFilter &m_caseListFilter;

    // Current session state.
    std::vector<NodeIter> m_sessionStack;
    std::string m_nodePath;

    // Counter that increments by one for each bottom-level test group
    int m_groupNumber;
};

} // namespace tcu

#endif // _TCUTESTHIERARCHYITERATOR_HPP
