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
 * \brief Raspberry PI platform.
 *//*--------------------------------------------------------------------*/

#include "tcuRaspiPlatform.hpp"
#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluGLContextFactory.hpp"
#include "deMemory.h"

tcu::Platform *createPlatform(void)
{
    return new tcu::rpi::Platform();
}

namespace tcu
{
namespace rpi
{

enum
{
    DEFAULT_WINDOW_WIDTH  = 400,
    DEFAULT_WINDOW_HEIGHT = 300
};

static const eglu::NativeDisplay::Capability DISPLAY_CAPABILITIES = eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY;
static const eglu::NativeWindow::Capability WINDOW_CAPABILITIES = eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY;

class Display : public eglu::NativeDisplay
{
public:
    Display(void) : eglu::NativeDisplay(DISPLAY_CAPABILITIES)
    {
    }
    ~Display(void)
    {
    }

    EGLNativeDisplayType getLegacyNative(void)
    {
        return EGL_DEFAULT_DISPLAY;
    }
};

class DisplayFactory : public eglu::NativeDisplayFactory
{
public:
    DisplayFactory(void);
    ~DisplayFactory(void)
    {
    }

    eglu::NativeDisplay *createDisplay(const EGLAttrib *attribList) const;
};

class Window : public eglu::NativeWindow
{
public:
    Window(int width, int height);
    ~Window(void);

    EGLNativeWindowType getLegacyNative(void)
    {
        return &m_nativeWindow;
    }

    IVec2 getSize(void) const;

private:
    DISPMANX_DISPLAY_HANDLE_T m_dispmanDisplay;
    DISPMANX_ELEMENT_HANDLE_T m_dispmanElement;
    EGL_DISPMANX_WINDOW_T m_nativeWindow;
};

class WindowFactory : public eglu::NativeWindowFactory
{
public:
    WindowFactory(void) : eglu::NativeWindowFactory("dispman", "Dispman Window", WINDOW_CAPABILITIES)
    {
    }
    ~WindowFactory(void)
    {
    }

    eglu::NativeWindow *createWindow(eglu::NativeDisplay *display, const eglu::WindowParams &params) const;
};

// DisplayFactory

DisplayFactory::DisplayFactory(void)
    : eglu::NativeDisplayFactory("default", "EGL_DEFAULT_DISPLAY", DISPLAY_CAPABILITIES)
{
    m_nativeWindowRegistry.registerFactory(new WindowFactory());
}

eglu::NativeDisplay *DisplayFactory::createDisplay(const EGLAttrib *) const
{
    return new Display();
}

// WindowFactory

eglu::NativeWindow *WindowFactory::createWindow(eglu::NativeDisplay *, const eglu::WindowParams &params) const
{
    const int width  = params.width != eglu::WindowParams::SIZE_DONT_CARE ? params.width : DEFAULT_WINDOW_WIDTH;
    const int height = params.height != eglu::WindowParams::SIZE_DONT_CARE ? params.height : DEFAULT_WINDOW_HEIGHT;

    return new Window(width, height);
}

// Window

Window::Window(int width, int height)
    : eglu::NativeWindow(WINDOW_CAPABILITIES)
    , m_dispmanDisplay(0)
    , m_dispmanElement(0)
{
    DISPMANX_UPDATE_HANDLE_T dispmanUpdate = 0;

    // \todo [pyry] Error handling.
    deMemset(&m_nativeWindow, 0, sizeof(m_nativeWindow));

    VC_RECT_T dstRect, srcRect;

    dstRect.x      = 0;
    dstRect.y      = 0;
    dstRect.width  = width;
    dstRect.height = height;

    srcRect.x      = 0;
    srcRect.y      = 0;
    srcRect.width  = width << 16;
    srcRect.height = height << 16;

    m_dispmanDisplay = vc_dispmanx_display_open(0);
    TCU_CHECK(m_dispmanDisplay);

    dispmanUpdate = vc_dispmanx_update_start(0);
    TCU_CHECK(dispmanUpdate);

    m_dispmanElement =
        vc_dispmanx_element_add(dispmanUpdate, m_dispmanDisplay, 0 /*layer*/, &dstRect, 0 /*src*/, &srcRect,
                                DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0 /*clamp*/, DISPMANX_NO_ROTATE);
    TCU_CHECK(m_dispmanElement);

    vc_dispmanx_update_submit_sync(dispmanUpdate);

    m_nativeWindow.element = m_dispmanElement;
    m_nativeWindow.width   = width;
    m_nativeWindow.height  = height;
}

Window::~Window(void)
{
    DISPMANX_UPDATE_HANDLE_T dispmanUpdate = 0;
    dispmanUpdate                          = vc_dispmanx_update_start(0);
    if (dispmanUpdate)
    {
        vc_dispmanx_element_remove(dispmanUpdate, m_dispmanElement);
        vc_dispmanx_update_submit_sync(dispmanUpdate);
    }

    vc_dispmanx_display_close(m_dispmanDisplay);
}

IVec2 Window::getSize(void) const
{
    return IVec2(m_nativeWindow.width, m_nativeWindow.height);
}

// Platform

Platform::Platform(void)
{
    bcm_host_init();

    m_nativeDisplayFactoryRegistry.registerFactory(new DisplayFactory());
    m_contextFactoryRegistry.registerFactory(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
}

Platform::~Platform(void)
{
}

} // namespace rpi
} // namespace tcu
