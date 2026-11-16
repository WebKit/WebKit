/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
* \brief Subprocess test executor shared memory implementation file.
*//*--------------------------------------------------------------------*/

#include "tcuSubprocessTestExecutor.hpp"
#define NOT_SUPPORTED_MESSAGE "Device fault tests execution not supported in Linux-like OSs"

namespace tcu
{

bool SubprocessTestExecutor::SharedMemory::allocate(size_t, unsigned long &, bool)
{
    TCU_THROW(NotSupportedError, NOT_SUPPORTED_MESSAGE);
    return false;
}

bool SubprocessTestExecutor::SharedMemory::open(size_t, unsigned long &, bool)
{
    TCU_THROW(NotSupportedError, NOT_SUPPORTED_MESSAGE);
    return false;
}

void SubprocessTestExecutor::SharedMemory::close()
{
    // This method is always called in the destructor, so we avoid throwing.
    //TCU_THROW(NotSupportedError, NOT_SUPPORTED_MESSAGE);
}

} // namespace tcu
