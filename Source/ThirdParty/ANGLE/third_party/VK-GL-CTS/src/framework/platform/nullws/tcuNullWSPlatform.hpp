#ifndef _TCUNULLWSPLATFORM_HPP
#define _TCUNULLWSPLATFORM_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief
 *//*--------------------------------------------------------------------*/

#include "deDynamicLibrary.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuDefs.hpp"
#include "tcuPlatform.hpp"
#include "gluPlatform.hpp"
#include "egluPlatform.hpp"
#include "vkPlatform.hpp"

namespace tcu
{
namespace nullws
{
class VulkanLibrary : public vk::Library
{
public:
    VulkanLibrary(const char *libraryPath)
        : m_library(libraryPath != nullptr ? libraryPath : "libvulkan.so.1")
        , m_driver(m_library)
    {
    }

    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_driver;
    }
    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return m_library;
    }

private:
    const tcu::DynamicFunctionLibrary m_library;
    const vk::PlatformDriver m_driver;
};

class Platform : public tcu::Platform, private glu::Platform, private eglu::Platform, private vk::Platform
{
public:
    Platform();
    virtual ~Platform();

    virtual const glu::Platform &getGLPlatform() const
    {
        return static_cast<const glu::Platform &>(*this);
    }
    virtual const eglu::Platform &getEGLPlatform() const
    {
        return static_cast<const eglu::Platform &>(*this);
    }
    virtual const vk::Platform &getVulkanPlatform() const
    {
        return static_cast<const vk::Platform &>(*this);
    }

    vk::Library *createLibrary(const char *libraryPath) const
    {
        return new VulkanLibrary(libraryPath);
    }
};

} // namespace nullws
} // namespace tcu

#endif // _TCUNULLWSPLATFORM_HPP
