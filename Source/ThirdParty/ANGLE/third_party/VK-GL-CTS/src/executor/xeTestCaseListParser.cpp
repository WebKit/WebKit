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
 * \brief Test case list parser.
 *//*--------------------------------------------------------------------*/

#include "xeTestCaseListParser.hpp"
#include "deString.h"

using std::string;
using std::vector;

namespace xe
{

static TestCaseType getTestCaseType(const char *caseType)
{
    // \todo [2012-06-11 pyry] Use hashes for speedup.
    static const struct
    {
        const char *name;
        TestCaseType caseType;
    } s_caseTypeMap[] = {{"SelfValidate", TESTCASETYPE_SELF_VALIDATE},
                         {"Capability", TESTCASETYPE_CAPABILITY},
                         {"Accuracy", TESTCASETYPE_ACCURACY},
                         {"Performance", TESTCASETYPE_PERFORMANCE}};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_caseTypeMap); ndx++)
    {
        if (strcmp(caseType, s_caseTypeMap[ndx].name) == 0)
            return s_caseTypeMap[ndx].caseType;
    }

    XE_FAIL((string("Unknown test case type '") + caseType + "'").c_str());
}

TestCaseListParser::TestCaseListParser(void) : m_root(nullptr)
{
}

TestCaseListParser::~TestCaseListParser(void)
{
}

void TestCaseListParser::clear(void)
{
    m_xmlParser.clear();
    m_nodeStack.clear();
    m_root = nullptr;
}

void TestCaseListParser::init(TestGroup *rootGroup)
{
    clear();
    m_root = rootGroup;
}

void TestCaseListParser::parse(const uint8_t *bytes, int numBytes)
{
    DE_ASSERT(m_root);
    m_xmlParser.feed(bytes, numBytes);

    for (;;)
    {
        xml::Element element = m_xmlParser.getElement();

        if (element == xml::ELEMENT_INCOMPLETE || element == xml::ELEMENT_END_OF_STRING)
            break;

        if (element == xml::ELEMENT_START || element == xml::ELEMENT_END)
        {
            bool isStart         = element == xml::ELEMENT_START;
            const char *elemName = m_xmlParser.getElementName();

            if (strcmp(elemName, "TestCase") == 0)
            {
                if (isStart)
                {
                    XE_CHECK_MSG(!m_nodeStack.empty(), "<TestCase> outside of <TestCaseList>");

                    TestNode *parent = m_nodeStack.back();
                    const char *name = m_xmlParser.hasAttribute("Name") ? m_xmlParser.getAttribute("Name") : nullptr;
                    const char *description =
                        m_xmlParser.hasAttribute("Description") ? m_xmlParser.getAttribute("Description") : nullptr;
                    const char *caseType =
                        m_xmlParser.hasAttribute("CaseType") ? m_xmlParser.getAttribute("CaseType") : nullptr;

                    XE_CHECK_MSG(name && description && caseType, "Missing attribute in <TestCase>");
                    XE_CHECK_MSG(parent->getNodeType() == TESTNODETYPE_GROUP,
                                 "Only TestGroups are allowed to have child nodes");

                    bool isGroup = strcmp(caseType, "TestGroup") == 0;
                    TestNode *node =
                        isGroup ? static_cast<TestNode *>(static_cast<TestGroup *>(parent)->createGroup(name)) :
                                  static_cast<TestNode *>(
                                      static_cast<TestGroup *>(parent)->createCase(getTestCaseType(caseType), name));

                    m_nodeStack.push_back(node);
                }
                else
                {
                    XE_CHECK_MSG(m_nodeStack.size() >= 2, "Unexpected </TestCase>");
                    m_nodeStack.pop_back();
                }
            }
            else if (strcmp(elemName, "TestCaseList") == 0)
            {
                if (isStart)
                {
                    XE_CHECK_MSG(m_nodeStack.empty(), "Unexpected <TestCaseList>");
                    m_nodeStack.push_back(m_root);
                }
                else
                {
                    XE_CHECK_MSG(m_nodeStack.size() == 1, "Unexpected </TestCaseList>");
                    m_nodeStack.pop_back();
                }
            }
            else
                XE_FAIL((string("Unexpected <") + elemName + ">").c_str());
        }
        else if (element != xml::ELEMENT_DATA)
            DE_ASSERT(false); // \note Data elements are just ignored, they should be whitespace anyway.

        m_xmlParser.advance();
    }
}

} // namespace xe
