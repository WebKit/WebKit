/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Test case.
 *//*--------------------------------------------------------------------*/

#include "xeTestCase.hpp"

using std::vector;

namespace xe
{

const char *getTestCaseTypeName(TestCaseType caseType)
{
    switch (caseType)
    {
    case TESTCASETYPE_SELF_VALIDATE:
        return "SelfValidate";
    case TESTCASETYPE_CAPABILITY:
        return "Capability";
    case TESTCASETYPE_ACCURACY:
        return "Accuracy";
    case TESTCASETYPE_PERFORMANCE:
        return "Performance";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static inline int getFirstComponentLength(const char *path)
{
    int compLen = 0;
    while (path[compLen] != 0 && path[compLen] != '.')
        compLen++;
    return compLen;
}

static bool compareNameToPathComponent(const char *name, const char *path, int compLen)
{
    for (int pos = 0; pos < compLen; pos++)
    {
        if (name[pos] != path[pos])
            return false;
    }

    if (name[compLen] != 0)
        return false;

    return true;
}

static void splitPath(const char *path, std::vector<std::string> &components)
{
    std::string pathStr(path);
    int compStart = 0;

    for (int pos = 0; pos < (int)pathStr.length(); pos++)
    {
        if (pathStr[pos] == '.')
        {
            components.push_back(pathStr.substr(compStart, pos - compStart));
            compStart = pos + 1;
        }
    }

    DE_ASSERT(compStart < (int)pathStr.length());
    components.push_back(pathStr.substr(compStart));
}

// TestNode

TestNode::TestNode(TestGroup *parent, TestNodeType nodeType, const char *name)
    : m_parent(parent)
    , m_nodeType(nodeType)
    , m_name(name)
{
    if (m_parent)
    {
        // Verify that the name is unique.
        if (parent->m_childNames.find(name) != parent->m_childNames.end())
            throw Error(std::string("Duplicate node '") + name + "' in '" + parent->getFullPath());

        m_parent->m_children.push_back(this);
        m_parent->m_childNames.insert(name);
    }
}

void TestNode::getFullPath(std::string &dst) const
{
    dst.clear();

    int nameLen             = 0;
    const TestNode *curNode = this;

    for (;;)
    {
        nameLen += (int)curNode->m_name.length();

        DE_ASSERT(curNode->m_parent);
        if (curNode->m_parent->getNodeType() != TESTNODETYPE_ROOT)
        {
            nameLen += 1;
            curNode = curNode->m_parent;
        }
        else
            break;
    }

    dst.resize(nameLen);

    curNode = this;
    int pos = nameLen;

    for (;;)
    {
        std::copy(curNode->m_name.begin(), curNode->m_name.end(), dst.begin() + (pos - curNode->m_name.length()));
        pos -= (int)curNode->m_name.length();

        DE_ASSERT(curNode->m_parent);
        if (curNode->m_parent->getNodeType() != TESTNODETYPE_ROOT)
        {
            dst[--pos] = '.';
            curNode    = curNode->m_parent;
        }
        else
            break;
    }
}

const TestNode *TestNode::find(const char *path) const
{
    if (m_nodeType == TESTNODETYPE_ROOT)
    {
        // Don't need to consider current node.
        return static_cast<const TestGroup *>(this)->findChildNode(path);
    }
    else
    {
        // Check if first component matches this node.
        int compLen = getFirstComponentLength(path);
        XE_CHECK(compLen > 0);

        if (compareNameToPathComponent(getName(), path, compLen))
        {
            if (path[compLen] == 0)
                return this;
            else if (getNodeType() == TESTNODETYPE_GROUP)
                return static_cast<const TestGroup *>(this)->findChildNode(path + compLen + 1);
            else
                return nullptr;
        }
        else
            return nullptr;
    }
}

TestNode *TestNode::find(const char *path)
{
    return const_cast<TestNode *>(const_cast<const TestNode *>(this)->find(path));
}

// TestGroup

TestGroup::TestGroup(TestGroup *parent, TestNodeType nodeType, const char *name) : TestNode(parent, nodeType, name)
{
    DE_ASSERT(nodeType == TESTNODETYPE_GROUP || nodeType == TESTNODETYPE_ROOT);
    DE_ASSERT(!parent == (nodeType == TESTNODETYPE_ROOT));
}

TestGroup::~TestGroup(void)
{
    for (std::vector<TestNode *>::iterator i = m_children.begin(); i != m_children.end(); i++)
        delete *i;
}

TestGroup *TestGroup::createGroup(const char *name)
{
    return new TestGroup(this, TESTNODETYPE_GROUP, name);
}

TestCase *TestGroup::createCase(TestCaseType caseType, const char *name)
{
    return TestCase::createAsChild(this, caseType, name);
}

const TestNode *TestGroup::findChildNode(const char *path) const
{
    int compLen = getFirstComponentLength(path);
    XE_CHECK(compLen > 0);

    // Try to find matching children.
    const TestNode *matchingNode = nullptr;
    for (vector<TestNode *>::const_iterator iter = m_children.begin(); iter != m_children.end(); iter++)
    {
        if (compareNameToPathComponent((*iter)->getName(), path, compLen))
        {
            matchingNode = *iter;
            break;
        }
    }

    if (matchingNode)
    {
        if (path[compLen] == 0)
            return matchingNode; // Last element in path, return matching node.
        else if (matchingNode->getNodeType() == TESTNODETYPE_GROUP)
            return static_cast<const TestGroup *>(matchingNode)->findChildNode(path + compLen + 1);
        else
            return nullptr;
    }
    else
        return nullptr;
}

// TestRoot

TestRoot::TestRoot(void) : TestGroup(nullptr, TESTNODETYPE_ROOT, "")
{
}

// TestCase

TestCase *TestCase::createAsChild(TestGroup *parent, TestCaseType caseType, const char *name)
{
    return new TestCase(parent, caseType, name);
}

TestCase::TestCase(TestGroup *parent, TestCaseType caseType, const char *name)
    : TestNode(parent, TESTNODETYPE_TEST_CASE, name)
    , m_caseType(caseType)
{
}

TestCase::~TestCase(void)
{
}

// TestHierarchyBuilder helpers

void addChildGroupsToMap(std::map<std::string, TestGroup *> &groupMap, TestGroup *group)
{
    for (int ndx = 0; ndx < group->getNumChildren(); ndx++)
    {
        TestNode *node = group->getChild(ndx);
        if (node->getNodeType() == TESTNODETYPE_GROUP)
        {
            TestGroup *childGroup = static_cast<TestGroup *>(node);
            std::string fullPath;
            childGroup->getFullPath(fullPath);

            groupMap.insert(std::make_pair(fullPath, childGroup));
            addChildGroupsToMap(groupMap, childGroup);
        }
    }
}

// TestHierarchyBuilder

TestHierarchyBuilder::TestHierarchyBuilder(TestRoot *root) : m_root(root)
{
    addChildGroupsToMap(m_groupMap, root);
}

TestHierarchyBuilder::~TestHierarchyBuilder(void)
{
}

TestCase *TestHierarchyBuilder::createCase(const char *path, TestCaseType caseType)
{
    // \todo [2012-09-05 pyry] This can be done with less string manipulations.
    std::vector<std::string> components;
    splitPath(path, components);
    DE_ASSERT(!components.empty());

    // Create all parents if necessary.
    TestGroup *curGroup = m_root;
    std::string curGroupPath;
    for (int ndx = 0; ndx < (int)components.size() - 1; ndx++)
    {
        if (!curGroupPath.empty())
            curGroupPath += ".";
        curGroupPath += components[ndx];

        std::map<std::string, TestGroup *>::const_iterator groupPos = m_groupMap.find(curGroupPath);
        if (groupPos == m_groupMap.end())
        {
            TestGroup *newGroup = curGroup->createGroup(components[ndx].c_str());
            m_groupMap.insert(std::make_pair(curGroupPath, newGroup));
            curGroup = newGroup;
        }
        else
            curGroup = groupPos->second;
    }

    return curGroup->createCase(caseType, components.back().c_str());
}

// TestSet helpers

static void addNodeAndParents(std::set<const TestNode *> &nodeSet, const TestNode *node)
{
    while (node != nullptr)
    {
        nodeSet.insert(node);
        node = node->getParent();
    }
}

static void addChildren(std::set<const TestNode *> &nodeSet, const TestGroup *group)
{
    for (int ndx = 0; ndx < group->getNumChildren(); ndx++)
    {
        const TestNode *child = group->getChild(ndx);
        nodeSet.insert(child);

        if (child->getNodeType() == TESTNODETYPE_GROUP)
            addChildren(nodeSet, static_cast<const TestGroup *>(child));
    }
}

static void removeChildren(std::set<const TestNode *> &nodeSet, const TestGroup *group)
{
    for (int ndx = 0; ndx < group->getNumChildren(); ndx++)
    {
        const TestNode *child = group->getChild(ndx);
        nodeSet.erase(child);

        if (child->getNodeType() == TESTNODETYPE_GROUP)
            removeChildren(nodeSet, static_cast<const TestGroup *>(child));
    }
}

static bool hasChildrenInSet(const std::set<const TestNode *> &nodeSet, const TestGroup *group)
{
    for (int ndx = 0; ndx < group->getNumChildren(); ndx++)
    {
        if (nodeSet.find(group->getChild(ndx)) != nodeSet.end())
            return true;
    }
    return false;
}

static void removeEmptyGroups(std::set<const TestNode *> &nodeSet, const TestGroup *group)
{
    if (!hasChildrenInSet(nodeSet, group))
    {
        nodeSet.erase(group);
        if (group->getParent() != nullptr)
            removeEmptyGroups(nodeSet, group->getParent());
    }
}

// TestSet

void TestSet::add(const TestNode *node)
{
    if (node->getNodeType() == TESTNODETYPE_TEST_CASE)
        addCase(static_cast<const TestCase *>(node));
    else
    {
        XE_CHECK(node->getNodeType() == TESTNODETYPE_GROUP || node->getNodeType() == TESTNODETYPE_ROOT);
        addGroup(static_cast<const TestGroup *>(node));
    }
}

void TestSet::addCase(const TestCase *testCase)
{
    addNodeAndParents(m_set, testCase);
}

void TestSet::addGroup(const TestGroup *testGroup)
{
    addNodeAndParents(m_set, testGroup);
    addChildren(m_set, testGroup);
}

void TestSet::remove(const TestNode *node)
{
    if (node->getNodeType() == TESTNODETYPE_TEST_CASE)
        removeCase(static_cast<const TestCase *>(node));
    else
    {
        XE_CHECK(node->getNodeType() == TESTNODETYPE_GROUP || node->getNodeType() == TESTNODETYPE_ROOT);
        removeGroup(static_cast<const TestGroup *>(node));
    }
}

void TestSet::removeCase(const TestCase *testCase)
{
    if (m_set.find(testCase) != m_set.end())
    {
        m_set.erase(testCase);
        removeEmptyGroups(m_set, testCase->getParent());
    }
}

void TestSet::removeGroup(const TestGroup *testGroup)
{
    if (m_set.find(testGroup) != m_set.end())
    {
        m_set.erase(testGroup);
        removeChildren(m_set, testGroup);
        if (testGroup->getParent() != nullptr)
            removeEmptyGroups(m_set, testGroup->getParent());
    }
}

// ConstTestNodeIterator

ConstTestNodeIterator::ConstTestNodeIterator(const TestNode *root) : m_root(root)
{
}

ConstTestNodeIterator ConstTestNodeIterator::begin(const TestNode *root)
{
    ConstTestNodeIterator iter(root);
    iter.m_iterStack.push_back(GroupState(nullptr));
    return iter;
}

ConstTestNodeIterator ConstTestNodeIterator::end(const TestNode *root)
{
    DE_UNREF(root);
    return ConstTestNodeIterator(root);
}

ConstTestNodeIterator &ConstTestNodeIterator::operator++(void)
{
    DE_ASSERT(!m_iterStack.empty());

    const TestNode *curNode  = **this;
    TestNodeType curNodeType = curNode->getNodeType();

    if ((curNodeType == TESTNODETYPE_GROUP || curNodeType == TESTNODETYPE_ROOT) &&
        static_cast<const TestGroup *>(curNode)->getNumChildren() > 0)
    {
        m_iterStack.push_back(GroupState(static_cast<const TestGroup *>(curNode)));
    }
    else
    {
        for (;;)
        {
            const TestGroup *group = m_iterStack.back().group;
            int &childNdx          = m_iterStack.back().childNdx;
            int numChildren        = group ? group->getNumChildren() : 1;

            childNdx += 1;
            if (childNdx == numChildren)
            {
                m_iterStack.pop_back();
                if (m_iterStack.empty())
                    break;
            }
            else
                break;
        }
    }

    return *this;
}

ConstTestNodeIterator ConstTestNodeIterator::operator++(int)
{
    ConstTestNodeIterator copy(*this);
    ++(*this);
    return copy;
}

const TestNode *ConstTestNodeIterator::operator*(void) const
{
    DE_ASSERT(!m_iterStack.empty());
    if (m_iterStack.size() == 1)
    {
        DE_ASSERT(m_iterStack[0].group == nullptr && m_iterStack[0].childNdx == 0);
        return m_root;
    }
    else
        return m_iterStack.back().group->getChild(m_iterStack.back().childNdx);
}

bool ConstTestNodeIterator::operator!=(const ConstTestNodeIterator &other) const
{
    return m_root != other.m_root || m_iterStack != other.m_iterStack;
}

} // namespace xe
