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
 * \brief EGL native window abstraction
 *//*--------------------------------------------------------------------*/

#include "egluNativeWindow.hpp"

namespace eglu
{

using namespace eglw;

// NativeWindow

NativeWindow::NativeWindow(Capability capabilities) : m_capabilities(capabilities)
{
}

EGLNativeWindowType NativeWindow::getLegacyNative(void)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CREATE_SURFACE_LEGACY) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support eglCreateWindowSurface()", nullptr, __FILE__,
                                 __LINE__);
}

void *NativeWindow::getPlatformExtension(void)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support eglCreatePlatformWindowSurfaceEXT()", nullptr,
                                 __FILE__, __LINE__);
}

void *NativeWindow::getPlatformNative(void)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CREATE_SURFACE_PLATFORM) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support eglCreatePlatformWindowSurface()", nullptr,
                                 __FILE__, __LINE__);
}

tcu::IVec2 NativeWindow::getSurfaceSize(void) const
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_GET_SURFACE_SIZE) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support querying the surface size", nullptr, __FILE__,
                                 __LINE__);
}

void NativeWindow::setSurfaceSize(tcu::IVec2 size)
{
    DE_UNREF(size);
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_SET_SURFACE_SIZE) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support resizing the surface", nullptr, __FILE__,
                                 __LINE__);
}

tcu::IVec2 NativeWindow::getScreenSize(void) const
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_GET_SCREEN_SIZE) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support querying the size of the window on the screen",
                                 nullptr, __FILE__, __LINE__);
}

void NativeWindow::readScreenPixels(tcu::TextureLevel *) const
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_READ_SCREEN_PIXELS) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support readScreenPixels", nullptr, __FILE__, __LINE__);
}

void NativeWindow::setVisibility(WindowParams::Visibility visibility)
{
    DE_UNREF(visibility);
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CHANGE_VISIBILITY) == 0);
    throw tcu::NotSupportedError("eglu::NativeWindow doesn't support changing visibility", nullptr, __FILE__, __LINE__);
}

// NativeWindowFactory

NativeWindowFactory::NativeWindowFactory(const std::string &name, const std::string &description,
                                         NativeWindow::Capability capabilities)
    : FactoryBase(name, description)
    , m_capabilities(capabilities)
{
}

NativeWindowFactory::~NativeWindowFactory(void)
{
}

NativeWindow *NativeWindowFactory::createWindow(NativeDisplay *nativeDisplay, EGLDisplay display, EGLConfig config,
                                                const EGLAttrib *attribList, const WindowParams &params) const
{
    DE_UNREF(display && config && attribList);
    return createWindow(nativeDisplay, params);
}

} // namespace eglu
