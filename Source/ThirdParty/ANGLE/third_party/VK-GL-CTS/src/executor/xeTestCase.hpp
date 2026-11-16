#ifndef _XETESTCASE_HPP
#define _XETESTCASE_HPP
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

#include "xeDefs.hpp"

#include <string>
#include <vector>
#include <set>
#include <map>

namespace xe
{

enum TestCaseType
{
    TESTCASETYPE_SELF_VALIDATE,
    TESTCASETYPE_CAPABILITY,
    TESTCASETYPE_ACCURACY,
    TESTCASETYPE_PERFORMANCE,

    TESTCASETYPE_LAST
};

const char *getTestCaseTypeName(TestCaseType caseType);

enum TestNodeType
{
    TESTNODETYPE_ROOT,
    TESTNODETYPE_GROUP,
    TESTNODETYPE_TEST_CASE,

    TESTNODETYPE_LAST
};

class TestGroup;
class TestCase;

class TestNode
{
public:
    virtual ~TestNode(void)
    {
    }

    TestNodeType getNodeType(void) const
    {
        return m_nodeType;
    }
    const char *getName(void) const
    {
        return m_name.c_str();
    }
    const TestGroup *getParent(void) const
    {
        return m_parent;
    }

    void getFullPath(std::string &path) const;
    std::string getFullPath(void) const
    {
        std::string str;
        getFullPath(str);
        return str;
    }

    const TestNode *find(const char *path) const;
    TestNode *find(const char *path);

protected:
    TestNode(TestGroup *parent, TestNodeType nodeType, const char *name);

private:
    TestNode(const TestNode &other);
    TestNode &operator=(const TestNode &other);

    TestGroup *m_parent;
    TestNodeType m_nodeType;
    std::string m_name;
    std::string m_description;
};

class TestGroup : public TestNode
{
public:
    ~TestGroup(void);

    int getNumChildren(void) const
    {
        return (int)m_children.size();
    }
    TestNode *getChild(int ndx)
    {
        return m_children[ndx];
    }
    const TestNode *getChild(int ndx) const
    {
        return m_children[ndx];
    }

    TestNode *findChildNode(const char *path);
    const TestNode *findChildNode(const char *path) const;

    TestGroup *createGroup(const char *name);
    TestCase *createCase(TestCaseType caseType, const char *name);

protected:
    TestGroup(TestGroup *parent, TestNodeType nodeType, const char *name);

private:
    std::vector<TestNode *> m_children;
    std::set<std::string> m_childNames; //!< Used for checking for duplicate test case names.

    // For adding TestCase to m_children. \todo [2012-06-15 pyry] Is the API broken perhaps?
    friend class TestNode;
};

class TestRoot : public TestGroup
{
public:
    TestRoot(void);
};

class TestCase : public TestNode
{
public:
    ~TestCase(void);

    TestCaseType getCaseType(void) const
    {
        return m_caseType;
    }

    static TestCase *createAsChild(TestGroup *parent, TestCaseType caseType, const char *name);

protected:
    TestCase(TestGroup *parent, TestCaseType caseType, const char *name);

private:
    TestCaseType m_caseType;
};

// Helper class for efficiently constructing TestCase hierarchy from test case list.
class TestHierarchyBuilder
{
public:
    TestHierarchyBuilder(TestRoot *root);
    ~TestHierarchyBuilder(void);

    TestCase *createCase(const char *path, TestCaseType caseType);

private:
    TestHierarchyBuilder(const TestHierarchyBuilder &other);
    TestHierarchyBuilder &operator=(const TestHierarchyBuilder &other);

    TestRoot *m_root;
    std::map<std::string, TestGroup *> m_groupMap;
};

// Helper class for computing and iterating test sets.
class TestSet
{
public:
    TestSet(void)
    {
    }
    ~TestSet(void)
    {
    }

    bool empty(void) const
    {
        return m_set.empty();
    }

    void add(const TestNode *node);
    void addCase(const TestCase *testCase);
    void addGroup(const TestGroup *testGroup);

    void remove(const TestNode *node);
    void removeCase(const TestCase *testCase);
    void removeGroup(const TestGroup *testGroup);

    bool hasNode(const TestNode *node) const
    {
        return m_set.find(node) != m_set.end();
    }

private:
    std::set<const TestNode *> m_set;
};

class ConstTestNodeIterator
{
public:
    static ConstTestNodeIterator begin(const TestNode *root);
    static ConstTestNodeIterator end(const TestNode *root);

    ConstTestNodeIterator &operator++(void);
    ConstTestNodeIterator operator++(int);

    const TestNode *operator*(void) const;

    bool operator!=(const ConstTestNodeIterator &other) const;

protected:
    ConstTestNodeIterator(const TestNode *root);

private:
    struct GroupState
    {
        GroupState(const TestGroup *group_) : group(group_), childNdx(0)
        {
        }

        const TestGroup *group;
        int childNdx;

        bool operator!=(const GroupState &other) const
        {
            return group != other.group || childNdx != other.childNdx;
        }

        bool operator==(const GroupState &other) const
        {
            return group == other.group && childNdx == other.childNdx;
        }
    };

    const TestNode *m_root;
    std::vector<GroupState> m_iterStack;
};

// \todo [2012-06-19 pyry] Implement following iterators:
//  - TestNodeIterator
//  - ConstTestSetIterator
//  - TestSetIterator

} // namespace xe

#endif // _XETESTCASE_HPP
