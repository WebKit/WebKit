/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2022 Google, Inc.
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
 * \brief Fuchsia Platform definition.
 *//*--------------------------------------------------------------------*/

#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "vkPlatform.hpp"

class FuchsiaVkLibrary : public vk::Library
{
public:
    FuchsiaVkLibrary(const char *library_path)
        : library_(library_path ? library_path : "libvulkan.so")
        , driver_(library_)
    {
    }

    const vk::PlatformInterface &getPlatformInterface() const
    {
        return driver_;
    }
    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return library_;
    }

private:
    const tcu::DynamicFunctionLibrary library_;
    const vk::PlatformDriver driver_;
};

class FuchsiaVkPlatform : public vk::Platform
{
public:
    vk::Library *createLibrary(const char *library_path) const
    {
        return new FuchsiaVkLibrary(library_path);
    }

    void describePlatform(std::ostream &dst) const
    {
        dst << "OS: Fuchsia\n";
        const char *cpu = "Unknown";
#if defined(__x86_64__)
        cpu = "x86_64";
#elif defined(__aarch64__)
        cpu = "aarch64";
#endif
        dst << "CPU: " << cpu << "\n";
    }

    void getMemoryLimits(tcu::PlatformMemoryLimits &limits) const
    {
        // Copied from tcuX11VulkanPlatform.cpp
        limits.totalSystemMemory                 = 256 * 1024 * 1024;
        limits.totalDeviceLocalMemory            = 0; // unified memory
        limits.deviceMemoryAllocationGranularity = 64 * 1024;
        limits.devicePageSize                    = 4096;
        limits.devicePageTableEntrySize          = 8;
        limits.devicePageTableHierarchyLevels    = 3;
    }
};

class FuchsiaPlatform : public tcu::Platform
{
public:
    ~FuchsiaPlatform()
    {
    }

    const vk::Platform &getVulkanPlatform() const
    {
        return vk_platform_;
    }

private:
    FuchsiaVkPlatform vk_platform_;
};

tcu::Platform *createPlatform()
{
    return new FuchsiaPlatform();
}
