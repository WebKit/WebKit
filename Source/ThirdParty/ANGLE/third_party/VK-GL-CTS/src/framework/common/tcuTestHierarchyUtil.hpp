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

// Helper function to retrieve a shared resource of type T from a TestCase that is a descendant of a
// TestCaseGroup. Meant to be called from TestCase::createInstance and passed to the TestInstance constructor.
// Nested groups may each hold a resource of a different type, so groups whose resource is not a T are
// skipped and the search continues towards the root.
template <typename T>
T &retrieveSharedResource(const tcu::TestNode *node)
{
    TCU_CHECK_INTERNAL(node);

    // Traverse ancestors to find a group that contains a shared resource of type T
    auto *ancestor = node->getParent();
    while (ancestor && (ancestor->getNodeType() == tcu::NODETYPE_GROUP))
    {
        auto *group = static_cast<TestCaseGroup *>(ancestor);
        if (auto *sharedData = dynamic_cast<T *>(group->getSharedResource()))
            return *sharedData;

        ancestor = ancestor->getParent();
    }

    // retrieveSharedResource should be called only from test cases that are descendants of a
    // TestCaseGroup containig shared resource of the requested type. Make sure setSharedResource
    // has been called on the ancestor group before calling this function.
    TCU_THROW(InternalError, "No ancestor holds a shared resource of the requested type");
}

// Call reset() on the shared resources owned by node and all of its ancestors. The resources
// themselves stay alive and must remain usable afterwards; see SharedResourceBase::reset().
// This function is used to drop shared device child objects at Vulkan SC batch boundaries.
void resetSharedResources(tcu::TestNode *node);

} // namespace tcu

#endif // _TCUTESTHIERARCHYUTIL_HPP
