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
 * \brief EGL native pixmap abstraction
 *//*--------------------------------------------------------------------*/

#include "egluNativePixmap.hpp"

namespace eglu
{

using namespace eglw;

// NativePixmap

NativePixmap::NativePixmap(Capability capabilities) : m_capabilities(capabilities)
{
}

EGLNativePixmapType NativePixmap::getLegacyNative(void)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CREATE_SURFACE_LEGACY) == 0);
    throw tcu::NotSupportedError("eglu::NativePixmap doesn't support eglCreatePixmapSurface()", nullptr, __FILE__,
                                 __LINE__);
}

void *NativePixmap::getPlatformExtension(void)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION) == 0);
    throw tcu::NotSupportedError("eglu::NativePixmap doesn't support eglCreatePlatformPixmapSurfaceEXT()", nullptr,
                                 __FILE__, __LINE__);
}

void *NativePixmap::getPlatformNative(void)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_CREATE_SURFACE_PLATFORM) == 0);
    throw tcu::NotSupportedError("eglu::NativePixmap doesn't support eglCreatePlatformPixmapSurface()", nullptr,
                                 __FILE__, __LINE__);
}

void NativePixmap::readPixels(tcu::TextureLevel *)
{
    TCU_CHECK_INTERNAL((m_capabilities & CAPABILITY_READ_PIXELS) == 0);
    throw tcu::NotSupportedError("eglu::NativePixmap doesn't support readPixels", nullptr, __FILE__, __LINE__);
}

// NativePixmapFactory

NativePixmapFactory::NativePixmapFactory(const std::string &name, const std::string &description,
                                         NativePixmap::Capability capabilities)
    : FactoryBase(name, description)
    , m_capabilities(capabilities)
{
}

NativePixmapFactory::~NativePixmapFactory(void)
{
}

NativePixmap *NativePixmapFactory::createPixmap(NativeDisplay *nativeDisplay, EGLDisplay display, EGLConfig config,
                                                const EGLAttrib *attribList, int width, int height) const
{
    DE_UNREF(display && config && attribList);
    return createPixmap(nativeDisplay, width, height);
}

} // namespace eglu
