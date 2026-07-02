#ifndef _XETESTCASELISTPARSER_HPP
#define _XETESTCASELISTPARSER_HPP
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

#include "xeDefs.hpp"
#include "xeTestCase.hpp"
#include "xeXMLParser.hpp"

#include <vector>

namespace xe
{

class TestCaseListParser
{
public:
    TestCaseListParser(void);
    ~TestCaseListParser(void);

    void init(TestGroup *rootGroup);
    void parse(const uint8_t *bytes, int numBytes);

private:
    TestCaseListParser(const TestCaseListParser &other);
    TestCaseListParser &operator=(const TestCaseListParser &other);

    void clear(void);

    xml::Parser m_xmlParser;
    TestGroup *m_root;

    std::vector<TestNode *> m_nodeStack;
};

} // namespace xe

#endif // _XETESTCASELISTPARSER_HPP
