/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief OSX Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuOSXVulkanPlatform.hpp"
#include "tcuOSXPlatform.hpp"
#include "tcuOSXMetalView.hpp"
#include "vkWsiPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"

#include <sys/utsname.h>

using de::MovePtr;
using de::UniquePtr;

namespace tcu
{
namespace osx
{

class VulkanWindow : public vk::wsi::MetalWindowInterface
{
public:
    VulkanWindow(MovePtr<osx::MetalView> view) : vk::wsi::MetalWindowInterface(view->getLayer()), m_view(view)
    {
    }

    void setVisible(bool)
    {
    }

    void resize(const UVec2 &newSize)
    {
        m_view->setSize(newSize.x(), newSize.y());
    }

    void setMinimized(bool minimized)
    {
        DE_UNREF(minimized);
        TCU_THROW(NotSupportedError, "Minimized on osx is not implemented");
    }

private:
    UniquePtr<osx::MetalView> m_view;
};

class VulkanDisplay : public vk::wsi::Display
{
public:
    VulkanDisplay()
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &initialSize) const
    {
        const uint32_t width  = !initialSize ? 400 : initialSize->x();
        const uint32_t height = !initialSize ? 300 : initialSize->y();
        return new VulkanWindow(MovePtr<osx::MetalView>(new osx::MetalView(width, height)));
    }
};

class VulkanLibrary : public vk::Library
{
public:
    VulkanLibrary(const char *libraryPath)
        : m_library(libraryPath != nullptr ? libraryPath : "libvulkan.dylib")
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
    const DynamicFunctionLibrary m_library;
    const vk::PlatformDriver m_driver;
};

struct VulkanWindowHeadless : public vk::wsi::Window
{
public:
    void setVisible(bool /* visible */)
    {
    }

    void resize(const UVec2 &)
    {
    }
};

class VulkanDisplayHeadless : public vk::wsi::Display
{
public:
    VulkanDisplayHeadless()
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &) const
    {
        return new VulkanWindowHeadless();
    }
};

VulkanPlatform::VulkanPlatform()
{
}

vk::wsi::Display *VulkanPlatform::createWsiDisplay(vk::wsi::Type wsiType) const
{
    switch (wsiType)
    {
    case vk::wsi::TYPE_METAL:
        return new VulkanDisplay();
    case vk::wsi::TYPE_HEADLESS:
        return new VulkanDisplayHeadless();
    default:
        TCU_THROW(NotSupportedError, "WSI type not supported");
    }
}

bool VulkanPlatform::hasDisplay(vk::wsi::Type wsiType) const
{
    switch (wsiType)
    {
    case vk::wsi::TYPE_METAL:
    case vk::wsi::TYPE_HEADLESS:
        return true;
    default:
        return false;
    }
}
vk::Library *VulkanPlatform::createLibrary(const char *libraryPath) const
{
    return new VulkanLibrary(libraryPath);
}

void VulkanPlatform::describePlatform(std::ostream &dst) const
{
    utsname sysInfo;
    deMemset(&sysInfo, 0, sizeof(sysInfo));

    if (uname(&sysInfo) != 0)
        throw std::runtime_error("uname() failed");

    dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " " << sysInfo.version << "\n";
    dst << "CPU: " << sysInfo.machine << "\n";
}

} // namespace osx
} // namespace tcu
