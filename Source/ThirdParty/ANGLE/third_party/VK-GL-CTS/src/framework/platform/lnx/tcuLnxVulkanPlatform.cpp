/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
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
 * \brief Linux Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxVulkanPlatform.hpp"
#include "tcuLnxPlatform.hpp"
#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuLibDrm.hpp"
#include "tcuLnx.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"

#include <sys/utsname.h>

using de::MovePtr;
using de::UniquePtr;
#if DEQP_SUPPORT_DRM && !defined(CTS_USES_VULKANSC)
using tcu::LibDrm;
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

#if defined(DEQP_SUPPORT_X11)
#include "tcuLnxX11.hpp"
#if defined(DEQP_SUPPORT_XCB)
#include "tcuLnxX11Xcb.hpp"
#endif // DEQP_SUPPORT_XCB
#define X11_DISPLAY ""
#endif // DEQP_SUPPORT_X11

#if defined(DEQP_SUPPORT_WAYLAND)
#include "tcuLnxWayland.hpp"
#define WAYLAND_DISPLAY nullptr
#endif // DEQP_SUPPORT_WAYLAND

#if !defined(DEQP_VULKAN_LIBRARY_PATH)
#ifdef CTS_USES_VULKANSC
#define DEQP_VULKAN_LIBRARY_PATH "libvulkansc.so.1"
#else
#define DEQP_VULKAN_LIBRARY_PATH "libvulkan.so.1"
#endif
#endif

namespace tcu
{
namespace lnx
{

#if defined(DEQP_SUPPORT_X11)

class VulkanWindowXlib : public vk::wsi::XlibWindowInterface
{
public:
    VulkanWindowXlib(MovePtr<x11::XlibWindow> window)
        : vk::wsi::XlibWindowInterface(vk::pt::XlibWindow(window->getXID()))
        , m_window(window)
    {
    }

    void setVisible(bool visible)
    {
        m_window->setVisibility(visible);
    }

    void resize(const UVec2 &newSize)
    {
        m_window->setDimensions((int)newSize.x(), (int)newSize.y());
    }

    void setMinimized(bool minimized)
    {
        DE_UNREF(minimized);
        TCU_THROW(NotSupportedError, "Minimized on X11 is not implemented");
    }

private:
    UniquePtr<x11::XlibWindow> m_window;
};

class VulkanDisplayXlib : public vk::wsi::XlibDisplayInterface
{
public:
    VulkanDisplayXlib(MovePtr<x11::DisplayBase> display)
        : vk::wsi::XlibDisplayInterface(vk::pt::XlibDisplayPtr(((x11::XlibDisplay *)display.get())->getXDisplay()))
        , m_display(display)
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &initialSize) const
    {
        x11::XlibDisplay *instance = (x11::XlibDisplay *)(m_display.get());
        const uint32_t height      = !initialSize ? (uint32_t)DEFAULT_WINDOW_HEIGHT : initialSize->y();
        const uint32_t width       = !initialSize ? (uint32_t)DEFAULT_WINDOW_WIDTH : initialSize->x();
        return new VulkanWindowXlib(
            MovePtr<x11::XlibWindow>(new x11::XlibWindow(*instance, (int)width, (int)height, instance->getVisual(0))));
    }

private:
    MovePtr<x11::DisplayBase> m_display;
};

#endif // DEQP_SUPPORT_X11

#if defined(DEQP_SUPPORT_XCB)

class VulkanWindowXcb : public vk::wsi::XcbWindowInterface
{
public:
    VulkanWindowXcb(MovePtr<x11::XcbWindow> window)
        : vk::wsi::XcbWindowInterface(vk::pt::XcbWindow(window->getXID()))
        , m_window(window)
    {
    }

    void setVisible(bool visible)
    {
        m_window->setVisibility(visible);
    }

    void resize(const UVec2 &newSize)
    {
        m_window->setDimensions((int)newSize.x(), (int)newSize.y());
    }

    void setMinimized(bool minimized)
    {
        DE_UNREF(minimized);
        TCU_THROW(NotSupportedError, "Minimized on xcb is not implemented");
    }

private:
    UniquePtr<x11::XcbWindow> m_window;
};

class VulkanDisplayXcb : public vk::wsi::XcbDisplayInterface
{
public:
    VulkanDisplayXcb(MovePtr<x11::DisplayBase> display)
        : vk::wsi::XcbDisplayInterface(vk::pt::XcbConnectionPtr(((x11::XcbDisplay *)display.get())->getConnection()))
        , m_display(display)
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &initialSize) const
    {
        x11::XcbDisplay *instance = (x11::XcbDisplay *)(m_display.get());
        const uint32_t height     = !initialSize ? (uint32_t)DEFAULT_WINDOW_HEIGHT : initialSize->y();
        const uint32_t width      = !initialSize ? (uint32_t)DEFAULT_WINDOW_WIDTH : initialSize->x();
        return new VulkanWindowXcb(
            MovePtr<x11::XcbWindow>(new x11::XcbWindow(*instance, (int)width, (int)height, nullptr)));
    }

private:
    MovePtr<x11::DisplayBase> m_display;
};
#endif // DEQP_SUPPORT_XCB

#if defined(DEQP_SUPPORT_WAYLAND)
class VulkanWindowWayland : public vk::wsi::WaylandWindowInterface
{
public:
    VulkanWindowWayland(MovePtr<wayland::Window> window)
        : vk::wsi::WaylandWindowInterface(vk::pt::WaylandSurfacePtr(window->getSurface()))
        , m_window(window)
    {
    }

    void setVisible(bool visible)
    {
        m_window->setVisibility(visible);
    }

    void resize(const UVec2 &newSize)
    {
        m_window->setDimensions((int)newSize.x(), (int)newSize.y());
    }

    void setMinimized(bool minimized)
    {
        DE_UNREF(minimized);
        TCU_THROW(NotSupportedError, "Minimized on wayland is not implemented");
    }

private:
    UniquePtr<wayland::Window> m_window;
};

class VulkanDisplayWayland : public vk::wsi::WaylandDisplayInterface
{
public:
    VulkanDisplayWayland(MovePtr<wayland::Display> display)
        : vk::wsi::WaylandDisplayInterface(vk::pt::WaylandDisplayPtr(display->getDisplay()))
        , m_display(display)
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &initialSize) const
    {
        const uint32_t height = !initialSize ? (uint32_t)DEFAULT_WINDOW_HEIGHT : initialSize->y();
        const uint32_t width  = !initialSize ? (uint32_t)DEFAULT_WINDOW_WIDTH : initialSize->x();
        return new VulkanWindowWayland(
            MovePtr<wayland::Window>(new wayland::Window(*m_display, (int)width, (int)height)));
    }

private:
    MovePtr<wayland::Display> m_display;
};
#endif // DEQP_SUPPORT_WAYLAND

#if defined(DEQP_SUPPORT_HEADLESS)

struct VulkanWindowHeadless : public vk::wsi::Window
{
public:
    void setVisible(bool visible) override
    {
        DE_UNREF(visible);
    }

    bool setForeground(void) override
    {
        return true;
    }

    void setMinimized(bool minimized) override
    {
        DE_UNREF(minimized);
    }

    void resize(const UVec2 &) override
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

#endif // DEQP_SUPPORT_HEADLESS

#if !defined(CTS_USES_VULKANSC)
#if DEQP_SUPPORT_DRM

struct VulkanWindowDirectDrm : public vk::wsi::Window
{
public:
    void setVisible(bool visible) override
    {
        DE_UNREF(visible);
    }
    void resize(const UVec2 &) override
    {
    }
};

class VulkanDisplayDirectDrm : public vk::wsi::DirectDrmDisplayInterface
{
public:
    VulkanDisplayDirectDrm(void) : m_vki(nullptr), m_physDevice(VK_NULL_HANDLE), m_initialized(false)
    {
    }

    ~VulkanDisplayDirectDrm(void)
    {
        if (m_initialized && m_vki != nullptr && m_native != VK_NULL_HANDLE)
        {
            m_vki->releaseDisplayEXT(m_physDevice, m_native);
        }
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &) const override
    {
        return new VulkanWindowDirectDrm();
    }

    void initializeDisplay(const vk::InstanceInterface &vki, vk::VkInstance instance,
                           const tcu::CommandLine &cmdLine) override
    {
        if (m_initialized)
            return;

        vk::VkPhysicalDevice physDevice = vk::chooseDevice(vki, instance, cmdLine);

        /* Get a Drm fd that matches the device. */

        vk::VkPhysicalDeviceProperties2 deviceProperties2;
        vk::VkPhysicalDeviceDrmPropertiesEXT deviceDrmProperties;

        deMemset(&deviceDrmProperties, 0, sizeof(deviceDrmProperties));
        deviceDrmProperties.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
        deviceDrmProperties.pNext = nullptr;

        deMemset(&deviceProperties2, 0, sizeof(deviceProperties2));
        deviceProperties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties2.pNext = &deviceDrmProperties;

        vki.getPhysicalDeviceProperties2(physDevice, &deviceProperties2);

        if (!deviceDrmProperties.hasPrimary)
            TCU_THROW(NotSupportedError, "No DRM primary device.");

        LibDrm libDrm;
        int numDrmDevices;
        drmDevicePtr *drmDevices = libDrm.getDevices(&numDrmDevices);
        const char *drmNode      = libDrm.findDeviceNode(drmDevices, numDrmDevices, deviceDrmProperties.primaryMajor,
                                                         deviceDrmProperties.primaryMinor);

        if (!drmNode)
        {
            libDrm.freeDevices(drmDevices, numDrmDevices);
            TCU_THROW(NotSupportedError, "No DRM node.");
        }

        m_fdPtr = libDrm.openFd(drmNode).move();
        libDrm.freeDevices(drmDevices, numDrmDevices);
        if (!m_fdPtr)
            TCU_THROW(NotSupportedError, "Could not open DRM.");
        int fd = *m_fdPtr;

        /* Get a connector to the display. */

        LibDrm::ResPtr res = libDrm.getResources(fd);
        if (!res)
            TCU_THROW(NotSupportedError, "Could not get DRM resources.");

        uint32_t connectorId = 0;
        for (int i = 0; i < res->count_connectors; ++i)
        {
            LibDrm::ConnectorPtr conn = libDrm.getConnector(fd, res->connectors[i]);

            if (conn && conn->connection == DRM_MODE_CONNECTED)
            {
                connectorId = res->connectors[i];
                break;
            }
        }
        if (!connectorId)
            TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

        /* Get and acquire the display for the connector. */

        vk::VkDisplayKHR *display = const_cast<vk::VkDisplayKHR *>(&m_native);
        VK_CHECK_SUPPORTED(vki.getDrmDisplayEXT(physDevice, fd, connectorId, display));

        if (m_native == VK_NULL_HANDLE)
            TCU_THROW(NotSupportedError, "vkGetDrmDisplayEXT did not set display.");

        vk::VkResult result = vki.acquireDrmDisplayEXT(physDevice, fd, m_native);
        if (result != vk::VK_SUCCESS)
            TCU_THROW(NotSupportedError, "vkAcquireDrmDisplayEXT failed.");

        m_vki         = &vki;
        m_physDevice  = physDevice;
        m_initialized = true;
    }

private:
    const vk::InstanceInterface *m_vki;
    vk::VkPhysicalDevice m_physDevice;
    MovePtr<LibDrm::FdPtr::element_type, LibDrm::FdPtr::deleter_type> m_fdPtr;
    bool m_initialized;
};
#endif // DEQP_SUPPORT_DRM
struct VulkanWindowDirect : public vk::wsi::Window
{
public:
    void setVisible(bool visible) override
    {
        DE_UNREF(visible);
    }
    void resize(const UVec2 &) override
    {
    }
};

class VulkanDisplayDirect : public vk::wsi::DirectDisplayInterface
{
public:
    VulkanDisplayDirect()
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &) const override
    {
        return new VulkanWindowDirect();
    }

    void initializeDisplay(const vk::InstanceInterface &vki, vk::VkInstance instance,
                           const tcu::CommandLine &cmdLine) override
    {
        if (m_initialized)
            return;

        vk::VkPhysicalDevice physDevice = vk::chooseDevice(vki, instance, cmdLine);

        uint32_t count = 0;
        VK_CHECK(vki.getPhysicalDeviceDisplayPropertiesKHR(physDevice, &count, nullptr));
        if (count == 0)
            TCU_THROW(NotSupportedError, "No displays available");

        std::vector<vk::VkDisplayPropertiesKHR> props(count);
        VK_CHECK(vki.getPhysicalDeviceDisplayPropertiesKHR(physDevice, &count, props.data()));
        vk::VkDisplayKHR *display = const_cast<vk::VkDisplayKHR *>(&m_native);
        *display                  = props[0].display;

        if (m_native == VK_NULL_HANDLE)
            TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceDisplayPropertiesKHR did not return display.");

        m_initialized = true;
    }

    bool m_initialized = false;
};
#endif // !defined (CTS_USES_VULKANSC)

class VulkanLibrary : public vk::Library
{
public:
    VulkanLibrary(const char *libraryPath)
        : m_library(libraryPath != nullptr ? libraryPath : DEQP_VULKAN_LIBRARY_PATH)
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

VulkanPlatform::VulkanPlatform(EventState &eventState) : m_eventState(eventState)
{
}

vk::wsi::Display *VulkanPlatform::createWsiDisplay(vk::wsi::Type wsiType) const
{
    if (!hasDisplay(wsiType))
    {
        throw NotSupportedError("This display type is not available: ", NULL, __FILE__, __LINE__);
    }

    switch (wsiType)
    {
#if defined(DEQP_SUPPORT_X11)
    case vk::wsi::TYPE_XLIB:
        return new VulkanDisplayXlib(MovePtr<x11::DisplayBase>(new x11::XlibDisplay(m_eventState, X11_DISPLAY)));
#endif // DEQP_SUPPORT_X11
#if defined(DEQP_SUPPORT_XCB)
    case vk::wsi::TYPE_XCB:
        return new VulkanDisplayXcb(MovePtr<x11::DisplayBase>(new x11::XcbDisplay(m_eventState, X11_DISPLAY)));
#endif // DEQP_SUPPORT_XCB
#if defined(DEQP_SUPPORT_WAYLAND)
    case vk::wsi::TYPE_WAYLAND:
        return new VulkanDisplayWayland(MovePtr<wayland::Display>(new wayland::Display(m_eventState, WAYLAND_DISPLAY)));
#endif // DEQP_SUPPORT_WAYLAND
#if defined(DEQP_SUPPORT_HEADLESS)
    case vk::wsi::TYPE_HEADLESS:
        return new VulkanDisplayHeadless();
#endif // DEQP_SUPPORT_HEADLESS
#if !defined(CTS_USES_VULKANSC)
#if DEQP_SUPPORT_DRM
    case vk::wsi::TYPE_DIRECT_DRM:
        return new VulkanDisplayDirectDrm();
#endif // DEQP_SUPPORT_DRM
    case vk::wsi::TYPE_DIRECT:
        return new VulkanDisplayDirect();
#endif //!defined (CTS_USES_VULKANSC)

    default:
        TCU_THROW(NotSupportedError, "WSI type not supported");
    }
}
bool VulkanPlatform::hasDisplay(vk::wsi::Type wsiType) const
{
    switch (wsiType)
    {
#if defined(DEQP_SUPPORT_X11)
    case vk::wsi::TYPE_XLIB:
        return x11::XlibDisplay::hasDisplay(X11_DISPLAY);
#endif // DEQP_SUPPORT_X11
#if defined(DEQP_SUPPORT_XCB)
    case vk::wsi::TYPE_XCB:
        return x11::XcbDisplay::hasDisplay(X11_DISPLAY);
#endif // DEQP_SUPPORT_XCB
#if defined(DEQP_SUPPORT_WAYLAND)
    case vk::wsi::TYPE_WAYLAND:
        return wayland::Display::hasDisplay(WAYLAND_DISPLAY);
#endif // DEQP_SUPPORT_WAYLAND
#if defined(DEQP_SUPPORT_HEADLESS)
    case vk::wsi::TYPE_HEADLESS:
        return true;
#endif // DEQP_SUPPORT_HEADLESS
#if !defined(CTS_USES_VULKANSC)
#if DEQP_SUPPORT_DRM
    case vk::wsi::TYPE_DIRECT_DRM:
        return true;
#endif // DEQP_SUPPORT_DRM
    case vk::wsi::TYPE_DIRECT:
        return true;
#endif // !defined (CTS_USES_VULKANSC)
    default:
        return false;
    }
}

vk::Library *VulkanPlatform::createLibrary(LibraryType libraryType, const char *libraryPath) const
{
    switch (libraryType)
    {
    case LIBRARY_TYPE_VULKAN:
        return new VulkanLibrary(libraryPath);

    default:
        TCU_THROW(InternalError, "Unknown library type requested");
    }
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

} // namespace lnx
} // namespace tcu
