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
 * \brief Platform (OS) specific services.
 *//*--------------------------------------------------------------------*/

#include "tcuPlatform.hpp"

namespace tcu
{

Platform::Platform(void)
{
}

Platform::~Platform(void)
{
}

bool Platform::processEvents(void)
{
    return true;
}

const glu::Platform &Platform::getGLPlatform(void) const
{
    TCU_THROW(NotSupportedError, "OpenGL (ES) is not supported");
}

const eglu::Platform &Platform::getEGLPlatform(void) const
{
    TCU_THROW(NotSupportedError, "EGL is not supported");
}

const vk::Platform &Platform::getVulkanPlatform(void) const
{
    TCU_THROW(NotSupportedError, "Vulkan is not supported");
}

void Platform::getMemoryLimits(PlatformMemoryLimits &limits) const
{
    // Default values. Platforms can override these values.
    limits.totalSystemMemory                 = 256 * 1024 * 1024;
    limits.totalDeviceLocalMemory            = 128 * 1024 * 1024;
    limits.deviceMemoryAllocationGranularity = 64 * 1024;
    limits.devicePageSize                    = 4096;
    limits.devicePageTableEntrySize          = 8;
    limits.devicePageTableHierarchyLevels    = 3;
}

} // namespace tcu
