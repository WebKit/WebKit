/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Mun Gwan-gyeong <elongbug@gmail.com>
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
 * \brief wayland Egl Display Factory.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxWaylandEglDisplayFactory.hpp"
#include "tcuLnxWayland.hpp"
#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"

namespace tcu
{
namespace lnx
{
namespace wayland
{
namespace egl
{

using std::string;

using de::MovePtr;
using de::UniquePtr;
using eglu::GLContextFactory;
using eglu::NativeDisplay;
using eglu::NativeDisplayFactory;
using eglu::NativePixmap;
using eglu::NativePixmapFactory;
using eglu::NativeWindow;
using eglu::NativeWindowFactory;
using eglu::WindowParams;
using glu::ContextFactory;
using tcu::TextureLevel;

class Display : public NativeDisplay
{
public:
    static const Capability CAPABILITIES =
        Capability(CAPABILITY_GET_DISPLAY_LEGACY | CAPABILITY_GET_DISPLAY_PLATFORM_EXT);

    Display(MovePtr<wayland::Display> waylandDisplay)
        : NativeDisplay(CAPABILITIES, EGL_PLATFORM_WAYLAND_KHR, "EGL_KHR_platform_wayland")
        , m_display(waylandDisplay)
        , m_library("libEGL.so")
    {
    }

    ~Display(void)
    {
    }
    wayland::Display &getWaylandDisplay(void)
    {
        return *m_display;
    }
    eglw::EGLNativeDisplayType getLegacyNative(void)
    {
        return reinterpret_cast<eglw::EGLNativeDisplayType>(m_display->getDisplay());
    }
    void *getPlatformNative(void)
    {
        return m_display->getDisplay();
    }
    const eglw::Library &getLibrary(void) const
    {
        return m_library;
    }
    const eglw::EGLAttrib *getPlatformAttributes(void) const
    {
        return nullptr;
    }

private:
    UniquePtr<wayland::Display> m_display;
    eglw::DefaultLibrary m_library;
};

class Window : public NativeWindow
{
public:
    static const Capability CAPABILITIES = Capability(CAPABILITY_CREATE_SURFACE_LEGACY | CAPABILITY_GET_SURFACE_SIZE |
                                                      CAPABILITY_SET_SURFACE_SIZE | CAPABILITY_GET_SCREEN_SIZE);

    Window(Display &display, const WindowParams &params);

    eglw::EGLNativeWindowType getLegacyNative(void)
    {
        return reinterpret_cast<eglw::EGLNativeWindowType>(m_window.getWindow());
    }
    IVec2 getSurfaceSize(void) const;
    void setSurfaceSize(IVec2 size);
    IVec2 getScreenSize(void) const
    {
        return getSurfaceSize();
    }

private:
    wayland::Window m_window;
};

Window::Window(Display &display, const WindowParams &params)
    : NativeWindow(CAPABILITIES)
    , m_window(display.getWaylandDisplay(), params.width, params.height)
{
}

IVec2 Window::getSurfaceSize(void) const
{
    IVec2 ret;
    m_window.getDimensions(&ret.x(), &ret.y());
    return ret;
}

void Window::setSurfaceSize(IVec2 size)
{
    m_window.setDimensions(size.x(), size.y());
}

class WindowFactory : public NativeWindowFactory
{
public:
    WindowFactory(void);

    NativeWindow *createWindow(NativeDisplay *nativeDisplay, const WindowParams &params) const;

    NativeWindow *createWindow(NativeDisplay *nativeDisplay, eglw::EGLDisplay display, eglw::EGLConfig config,
                               const eglw::EGLAttrib *attribList, const WindowParams &params) const;
};

WindowFactory::WindowFactory(void) : NativeWindowFactory("window", "Wayland Window", Window::CAPABILITIES)
{
}

NativeWindow *WindowFactory::createWindow(NativeDisplay *nativeDisplay, const WindowParams &params) const
{
    Display &display = *dynamic_cast<Display *>(nativeDisplay);

    return new Window(display, params);
}

NativeWindow *WindowFactory::createWindow(NativeDisplay *nativeDisplay, eglw::EGLDisplay eglDisplay,
                                          eglw::EGLConfig config, const eglw::EGLAttrib *attribList,
                                          const WindowParams &params) const
{
    DE_UNREF(eglDisplay);
    DE_UNREF(config);
    DE_UNREF(attribList);

    Display &display = *dynamic_cast<Display *>(nativeDisplay);

    return new Window(display, params);
}

class DisplayFactory : public NativeDisplayFactory
{
public:
    DisplayFactory(EventState &eventState);

    NativeDisplay *createDisplay(const eglw::EGLAttrib *attribList) const;

private:
    EventState &m_eventState;
};

DisplayFactory::DisplayFactory(EventState &eventState)
    : NativeDisplayFactory("Wayland", "Native Wayland Display", Display::CAPABILITIES, EGL_PLATFORM_WAYLAND_KHR,
                           "EGL_KHR_platform_wayland")
    , m_eventState(eventState)
{
    m_nativeWindowRegistry.registerFactory(new WindowFactory());
}

NativeDisplay *DisplayFactory::createDisplay(const eglw::EGLAttrib *attribList) const
{
    DE_UNREF(attribList);

    MovePtr<wayland::Display> waylandDisplay(new wayland::Display(m_eventState, nullptr));

    return new Display(waylandDisplay);
}

NativeDisplayFactory *createDisplayFactory(EventState &eventState)
{
    return new DisplayFactory(eventState);
}

} // namespace egl
} // namespace wayland
} // namespace lnx
} // namespace tcu
