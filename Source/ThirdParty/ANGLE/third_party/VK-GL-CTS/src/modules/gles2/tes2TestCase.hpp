#ifndef _TES2TESTCASE_HPP
#define _TES2TESTCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
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
 * \brief OpenGL ES 2.0 test case.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tes2Context.hpp"

namespace deqp
{
namespace gles2
{

class TestCaseGroup : public tcu::TestCaseGroup
{
public:
    TestCaseGroup(Context &context, const char *name, const char *description);
    TestCaseGroup(Context &context, const char *name, const char *description, const std::vector<TestNode *> &children);
    virtual ~TestCaseGroup(void)
    {
    }

    Context &getContext(void)
    {
        return m_context;
    }

protected:
    Context &m_context;
};

class TestCase : public tcu::TestCase
{
public:
    TestCase(Context &context, const char *name, const char *description);
    TestCase(Context &context, tcu::TestNodeType type, const char *name, const char *description);
    virtual ~TestCase(void)
    {
    }

protected:
    Context &m_context;
};

inline TestCaseGroup::TestCaseGroup(Context &context, const char *name, const char *description)
    : tcu::TestCaseGroup(context.getTestContext(), name, description)
    , m_context(context)
{
}

inline TestCaseGroup::TestCaseGroup(Context &context, const char *name, const char *description,
                                    const std::vector<TestNode *> &children)
    : tcu::TestCaseGroup(context.getTestContext(), name, description, children)
    , m_context(context)
{
}

inline TestCase::TestCase(Context &context, const char *name, const char *description)
    : tcu::TestCase(context.getTestContext(), name, description)
    , m_context(context)
{
}

inline TestCase::TestCase(Context &context, tcu::TestNodeType type, const char *name, const char *description)
    : tcu::TestCase(context.getTestContext(), type, name, description)
    , m_context(context)
{
}

} // namespace gles2
} // namespace deqp

#endif // _TES2TESTCASE_HPP
