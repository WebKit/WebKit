#ifndef _TCUTESTCASE_HPP
#define _TCUTESTCASE_HPP
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
#include "tcuTestContext.hpp"

#include <string>
#include <vector>

namespace tcu
{

enum TestNodeType
{
    NODETYPE_ROOT = 0,      //!< Root for all test packages.
    NODETYPE_PACKAGE,       //!< Test case package -- same as group, but is omitted from XML dump.
    NODETYPE_GROUP,         //!< Test case container -- cannot be executed.
    NODETYPE_SELF_VALIDATE, //!< Self-validating test case -- can be executed
    NODETYPE_PERFORMANCE,   //!< Performace test case -- can be executed
    NODETYPE_CAPABILITY,    //!< Capability score case -- can be executed
    NODETYPE_ACCURACY       //!< Accuracy test case -- can be executed
};

enum TestNodeClass
{
    NODECLASS_GROUP = 0,  //!< Root or non-leaf in the test hierarchy tree
    NODECLASS_EXECUTABLE, //!< Non-root leaf in the test hierarchy tree

    NODECLASS_LAST
};

enum TestRunnerType
{
    RUNNERTYPE_ANY   = 0u,
    RUNNERTYPE_NONE  = (1u << 0),
    RUNNERTYPE_AMBER = (1u << 1)
};

inline TestNodeClass getTestNodeTypeClass(TestNodeType type)
{
    switch (type)
    {
    case NODETYPE_ROOT:
        return NODECLASS_GROUP;
    case NODETYPE_PACKAGE:
        return NODECLASS_GROUP;
    case NODETYPE_GROUP:
        return NODECLASS_GROUP;
    case NODETYPE_SELF_VALIDATE:
        return NODECLASS_EXECUTABLE;
    case NODETYPE_PERFORMANCE:
        return NODECLASS_EXECUTABLE;
    case NODETYPE_CAPABILITY:
        return NODECLASS_EXECUTABLE;
    case NODETYPE_ACCURACY:
        return NODECLASS_EXECUTABLE;
    default:
        DE_ASSERT(false);
        return NODECLASS_LAST;
    }
}

inline bool isTestNodeTypeExecutable(TestNodeType type)
{
    return getTestNodeTypeClass(type) == NODECLASS_EXECUTABLE;
}

inline bool isValidTestCaseNameChar(char c)
{
    return de::inRange(c, 'a', 'z') || de::inRange(c, 'A', 'Z') || de::inRange(c, '0', '9') || c == '_' || c == '-';
}

class TestCaseGroup;
class CaseListFilter;

/*--------------------------------------------------------------------*//*!
 * \brief Test case hierarchy node
 *
 * Test node forms the backbone of the test case hierarchy. All objects
 * in the hierarchy are derived from this class.
 *
 * Each test node has a type and all except the root node have name and
 * description. Root and test group nodes have a list of children.
 *
 * During test execution TestExecutor iterates the hierarchy. Upon entering
 * the node (both groups and test cases) init() is called. When exiting the
 * node deinit() is called respectively.
 *//*--------------------------------------------------------------------*/
class TestNode
{
public:
    enum IterateResult
    {
        STOP     = 0,
        CONTINUE = 1
    };

    // Methods.
    TestNode(TestContext &testCtx, TestNodeType nodeType, const char *name);
    TestNode(TestContext &testCtx, TestNodeType nodeType, const char *name, const std::vector<TestNode *> &children);
    virtual ~TestNode(void);

    TestNodeType getNodeType(void) const
    {
        return m_nodeType;
    }
    TestContext &getTestContext(void) const
    {
        return m_testCtx;
    }
    const char *getName(void) const
    {
        return m_name.c_str();
    }
#if defined(DEQP_LOG_NODE_SOURCE)
    const char *getSource(void) const
    {
        return m_source.c_str();
    }
#endif
    void getChildren(std::vector<TestNode *> &children) const;
    void addRootChild(const std::string &groupName, const CaseListFilter *caseListFilter,
                      TestCaseGroup *(*createTestGroup)(tcu::TestContext &testCtx, const std::string &name));
    void addChild(TestNode *node);
    bool empty() const
    {
        return m_children.empty();
    }

    virtual void init(void);
    virtual void deinit(void);
    virtual IterateResult iterate(void) = 0;
    virtual TestRunnerType getRunnerType(void) const
    {
        return RUNNERTYPE_NONE;
    }
    virtual bool validateRequirements()
    {
        return true;
    }
#if defined(DEQP_LOG_NODE_SOURCE)
    virtual void setSource(const std::string &source)
    {
        m_source = source;
    }
#endif

    // Set useFraction0 to true to force a test group to run in fraction zero.
    // Must be applied to the bottom-level group.
    void setUseFraction0(bool u)
    {
        m_useFraction0 = u;
    }
    bool getUseFraction0() const
    {
        return m_useFraction0;
    }

protected:
    TestContext &m_testCtx;
    std::string m_name;
#if defined(DEQP_LOG_NODE_SOURCE)
    std::string m_source;
#endif

private:
    const TestNodeType m_nodeType;
    std::vector<TestNode *> m_children;
    bool m_duplicateCheck;
    bool m_useFraction0{false};
};

/*--------------------------------------------------------------------*//*!
 * \brief Test case group node
 *
 * Test case group implementations must inherit this class. To save resources
 * during test execution the group must delay creation of any child groups
 * until init() is called.
 *
 * Default deinit() for test group will destroy all child nodes.
 *//*--------------------------------------------------------------------*/
class TestCaseGroup : public TestNode
{
public:
    TestCaseGroup(TestContext &testCtx, const char *name);
    TestCaseGroup(TestContext &testCtx, const char *name, const std::vector<TestNode *> &children);

    // Deprecated constructors
    TestCaseGroup(TestContext &testCtx, const char *name, const char *description);
    TestCaseGroup(TestContext &testCtx, const char *name, const char *description,
                  const std::vector<TestNode *> &children);

    virtual ~TestCaseGroup(void);

    virtual IterateResult iterate(void);
};

/*--------------------------------------------------------------------*//*!
 * \brief Test case class
 *
 * Test case implementations must inherit this class.
 *
 * Test case objects are usually constructed when TestExecutor enters parent
 * group. Allocating any non-parameter resources, especially target API objects
 * must be delayed to init().
 *
 * Upon entering the test case TestExecutor calls init(). If initialization
 * is successful (no exception is thrown) the executor will then call iterate()
 * until test case returns STOP. After that deinit() will be called.
 *
 * Before exiting the execution phase (i.e. at returning STOP from iterate())
 * the test case must set valid status code to test context (m_testCtx).
 *
 * Test case can also signal error condition by throwing an exception. In
 * that case the framework will set result code and details based on the
 * exception.
 *//*--------------------------------------------------------------------*/
class TestCase : public TestNode
{
public:
    TestCase(TestContext &testCtx, const char *name);
    TestCase(TestContext &testCtx, TestNodeType nodeType, const char *name);

    // Deprecated constructors
    TestCase(TestContext &testCtx, const char *name, const char *description);
    TestCase(TestContext &testCtx, TestNodeType nodeType, const char *name, const char *description);

    virtual ~TestCase(void);
};

class TestStatus
{
public:
    TestStatus(qpTestResult code, const std::string &description) : m_code(code), m_description(description)
    {
    }

    bool isComplete(void) const
    {
        return m_code != QP_TEST_RESULT_LAST;
    }
    bool isFail(void) const
    {
        return m_code == QP_TEST_RESULT_FAIL;
    }
    qpTestResult getCode(void) const
    {
        DE_ASSERT(isComplete());
        return m_code;
    }
    const std::string &getDescription(void) const
    {
        DE_ASSERT(isComplete());
        return m_description;
    }

    static TestStatus pass(const std::string &description)
    {
        return TestStatus(QP_TEST_RESULT_PASS, description);
    }
    static TestStatus fail(const std::string &description)
    {
        return TestStatus(QP_TEST_RESULT_FAIL, description);
    }
    static TestStatus incomplete(void)
    {
        return TestStatus(QP_TEST_RESULT_LAST, "");
    }

private:
    qpTestResult m_code;
    std::string m_description;
} DE_WARN_UNUSED_TYPE;

} // namespace tcu

#endif // _TCUTESTCASE_HPP
