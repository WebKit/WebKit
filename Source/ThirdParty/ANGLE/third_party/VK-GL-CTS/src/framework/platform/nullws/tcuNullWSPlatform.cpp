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

#include "tcuNullWSPlatform.hpp"
#include "eglwEnums.hpp"
#include "eglwLibrary.hpp"
#include "egluGLContextFactory.hpp"

namespace tcu
{
namespace nullws
{

class Window : public eglu::NativeWindow
{
public:
    static const Capability CAPABILITIES = CAPABILITY_CREATE_SURFACE_LEGACY;

    Window(eglu::NativeDisplay *nativeDisplay, const eglu::WindowParams &params) : NativeWindow(CAPABILITIES)
    {
    }

    eglw::EGLNativeWindowType getLegacyNative()
    {
        return nullptr;
    }
};

class WindowFactory : public eglu::NativeWindowFactory
{
public:
    WindowFactory() : NativeWindowFactory("nullws", "NullWS Window", Window::CAPABILITIES)
    {
    }

    eglu::NativeWindow *createWindow(eglu::NativeDisplay *nativeDisplay, const eglu::WindowParams &params) const
    {
        return new Window(nativeDisplay, params);
    }
};

class Pixmap : public eglu::NativePixmap
{
public:
    static const Capability CAPABILITIES = CAPABILITY_CREATE_SURFACE_LEGACY;

    Pixmap() : NativePixmap(CAPABILITIES)
    {
    }

    eglw::EGLNativePixmapType getLegacyNative()
    {
        return nullptr;
    }
};

class PixmapFactory : public eglu::NativePixmapFactory
{
public:
    PixmapFactory() : NativePixmapFactory("nullws", "NullWS Pixmap", Pixmap::CAPABILITIES)
    {
    }

    eglu::NativePixmap *createPixmap(eglu::NativeDisplay *, int, int) const
    {
        return new Pixmap();
    }
};

class Display : public eglu::NativeDisplay
{
public:
    static const Capability CAPABILITIES = CAPABILITY_GET_DISPLAY_LEGACY;

    Display() : eglu::NativeDisplay(CAPABILITIES)
    {
    }

    eglw::EGLNativeDisplayType getLegacyNative()
    {
        return EGL_DEFAULT_DISPLAY;
    }

    const eglw::Library &getLibrary() const
    {
        return m_library;
    }

private:
    eglw::DefaultLibrary m_library;
};

class DisplayFactory : public eglu::NativeDisplayFactory
{
public:
    DisplayFactory() : eglu::NativeDisplayFactory("nullws", "NullWS Display", Display::CAPABILITIES)
    {
        m_nativeWindowRegistry.registerFactory(new WindowFactory());
        m_nativePixmapRegistry.registerFactory(new PixmapFactory());
    }

    eglu::NativeDisplay *createDisplay(const eglw::EGLAttrib *attribList = nullptr) const
    {
        return new Display();
    }
};

Platform::Platform()
{
    m_nativeDisplayFactoryRegistry.registerFactory(new DisplayFactory());
    m_contextFactoryRegistry.registerFactory(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
}

Platform::~Platform()
{
}

} // namespace nullws
} // namespace tcu

tcu::Platform *createPlatform()
{
    return new tcu::nullws::Platform();
}
