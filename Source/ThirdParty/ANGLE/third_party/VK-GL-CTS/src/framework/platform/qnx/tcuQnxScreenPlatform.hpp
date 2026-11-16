#ifndef _TCUQNXSCREENPLATFORM_HPP
#define _TCUQNXSCREENPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2024 NVIDIA CORPORATION.
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
 * \brief QNX Screen Platform definition.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "egluPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuPlatform.hpp"
#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"

#include <dlfcn.h>
#include <screen/screen.h>

namespace tcu
{
namespace QnxScreen
{

class Platform : public tcu::Platform, private eglu::Platform, private glu::Platform
{
public:
    Platform(void);
    ~Platform(void);

    virtual const glu::Platform &getGLPlatform(void) const
    {
        return static_cast<const glu::Platform &>(*this);
    }

    virtual const eglu::Platform &getEGLPlatform(void) const
    {
        return static_cast<const eglu::Platform &>(*this);
    }
};

class Library : public eglw::DefaultLibrary
{
public:
    Library(void) : eglw::DefaultLibrary("libEGL.so")
    {
    }
};

class Display : public eglu::NativeDisplay
{
private:
    void LoadLibscreen(void);
    void CreateScreenContext(void);
    void DestroyScreenContext(void);

public:
    Display(void);
    ~Display(void);

    eglw::EGLNativeDisplayType getLegacyNative(void)
    {
        return m_display;
    }
    const eglw::Library &getLibrary(void) const
    {
        return m_library;
    }
    eglw::EGLNativeDisplayType *m_display;
    Library m_library;
};

class DisplayFactory : public eglu::NativeDisplayFactory
{
public:
    DisplayFactory(void);
    ~DisplayFactory(void)
    {
    }

    eglu::NativeDisplay *createDisplay(const eglw::EGLAttrib *attribList) const;
};

class Window : public eglu::NativeWindow
{
public:
    Window(eglu::NativeDisplay *display, int width, int height, eglw::EGLDisplay eglDisplay, eglw::EGLConfig config);
    ~Window(void);

    eglw::EGLNativeWindowType getLegacyNative(void)
    {
        return m_nativeWindow;
    }

private:
    eglw::EGLNativeWindowType m_nativeWindow;
    screen_window_t m_screenWindow{nullptr};
};

class WindowFactory : public eglu::NativeWindowFactory
{
public:
    WindowFactory(void)
        : eglu::NativeWindowFactory("window", "QNX Screen Window", eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY)
    {
    }
    ~WindowFactory(void)
    {
    }

    eglu::NativeWindow *createWindow(eglu::NativeDisplay *display, const eglu::WindowParams &params) const;

    eglu::NativeWindow *createWindow(eglu::NativeDisplay *display, eglw::EGLDisplay eglDisplay, eglw::EGLConfig config,
                                     const eglw::EGLAttrib *attribList, const eglu::WindowParams &params) const;
};

} // namespace QnxScreen
} // namespace tcu

#endif // _TCUQNXSCREENPLATFORM_HPP