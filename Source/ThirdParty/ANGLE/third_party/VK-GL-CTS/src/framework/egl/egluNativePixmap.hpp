#ifndef _EGLUNATIVEPIXMAP_HPP
#define _EGLUNATIVEPIXMAP_HPP
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

#include "tcuDefs.hpp"
#include "tcuFactoryRegistry.hpp"
#include "eglwDefs.hpp"
#include "tcuVector.hpp"

namespace tcu
{
class TextureLevel;
}

namespace eglu
{

class NativeDisplay;

class NativePixmap
{
public:
    enum Capability
    {
        CAPABILITY_CREATE_SURFACE_LEGACY = (1 << 0), //!< EGL surface can be created with eglCreatePixmapSurface()
        CAPABILITY_CREATE_SURFACE_PLATFORM =
            (1 << 1), //!< EGL surface can be created with eglCreatePlatformPixmapSurface()
        CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION =
            (1 << 2), //!< EGL surface can be created with eglCreatePlatformPixmapSurfaceEXT()
        CAPABILITY_READ_PIXELS = (1 << 3)
    };

    virtual ~NativePixmap(void)
    {
    }

    //! Return EGLNativePixmapType that can be used with eglCreatePixmapSurface(). Default implementation throws tcu::NotSupportedError().
    virtual eglw::EGLNativePixmapType getLegacyNative(void);

    //! Return native pointer that can be used with eglCreatePlatformPixmapSurface(). Default implementation throws tcu::NotSupportedError().
    virtual void *getPlatformNative(void);

    //! Return native pointer that can be used with eglCreatePlatformPixmapSurfaceEXT(). Default implementation throws tcu::NotSupportedError().
    virtual void *getPlatformExtension(void);

    // Read pixels from pixmap. Default implementation throws tcu::NotSupportedError()
    virtual void readPixels(tcu::TextureLevel *dst);

    // These values are initialized in constructor.
    Capability getCapabilities(void) const
    {
        return m_capabilities;
    }

protected:
    NativePixmap(Capability capabilities);

private:
    NativePixmap(const NativePixmap &);
    NativePixmap &operator=(const NativePixmap &);

    const Capability m_capabilities;
};

class NativePixmapFactory : public tcu::FactoryBase
{
public:
    virtual ~NativePixmapFactory(void);

    //! Create generic pixmap.
    virtual NativePixmap *createPixmap(NativeDisplay *nativeDisplay, int width, int height) const = 0;

    //! Create pixmap that matches given EGL config. Defaults to generic createPixmap().
    virtual NativePixmap *createPixmap(NativeDisplay *nativeDisplay, eglw::EGLDisplay display, eglw::EGLConfig config,
                                       const eglw::EGLAttrib *attribList, int width, int height) const;

    NativePixmap::Capability getCapabilities(void) const
    {
        return m_capabilities;
    }

protected:
    NativePixmapFactory(const std::string &name, const std::string &description, NativePixmap::Capability capabilities);

private:
    NativePixmapFactory(const NativePixmapFactory &);
    NativePixmapFactory &operator=(const NativePixmapFactory &);

    const NativePixmap::Capability m_capabilities;
};

typedef tcu::FactoryRegistry<NativePixmapFactory> NativePixmapFactoryRegistry;

} // namespace eglu

#endif // _EGLUNATIVEPIXMAP_HPP
