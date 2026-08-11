#ifndef _XETESTLOGWRITER_HPP
#define _XETESTLOGWRITER_HPP
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
 * \brief Test log writer.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "xeBatchResult.hpp"
#include "xeTestCaseResult.hpp"

#include <ostream>

namespace xe
{

namespace xml
{
class Writer;
}

void writeTestLog(const BatchResult &batchResult, std::ostream &stream);
void writeBatchResultToFile(const BatchResult &batchResult, const char *filename);

void writeTestResult(const TestCaseResult &result, xe::xml::Writer &writer);
void writeTestResult(const TestCaseResult &result, std::ostream &stream);
void writeTestResultToFile(const TestCaseResult &result, const char *filename);

} // namespace xe

#endif // _XETESTLOGWRITER_HPP
