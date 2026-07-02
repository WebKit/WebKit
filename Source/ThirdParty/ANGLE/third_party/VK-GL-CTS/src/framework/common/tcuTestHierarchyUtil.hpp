#ifndef _TCUTESTHIERARCHYUTIL_HPP
#define _TCUTESTHIERARCHYUTIL_HPP
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
 * \brief Test hierarchy utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestHierarchyIterator.hpp"

namespace tcu
{

// \todo [2015-02-26 pyry] Remove TestContext requirement
void writeXmlCaselistsToFiles(TestPackageRoot &root, TestContext &testCtx, const CommandLine &cmdLine);
void writeTxtCaselistsToFiles(TestPackageRoot &root, TestContext &testCtx, const CommandLine &cmdLine,
                              bool printPrefix = true, bool skipGroups = false);

} // namespace tcu

#endif // _TCUTESTHIERARCHYUTIL_HPP
