#ifndef _EGLUNATIVEWINDOW_HPP
#define _EGLUNATIVEWINDOW_HPP
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

class NativePixmap;
class NativeDisplay;

struct WindowParams
{
    enum Visibility
    {
        VISIBILITY_HIDDEN = 0,
        VISIBILITY_VISIBLE,
        VISIBILITY_FULLSCREEN,
        VISIBILITY_DONT_CARE,

        VISIBILITY_LAST
    };

    enum
    {
        SIZE_DONT_CARE = -1
    };

    int width;             //!< Positive size, or SIZE_DONT_CARE
    int height;            //!< Positive size, or SIZE_DONT_CARE
    Visibility visibility; //!< Visibility for window

    WindowParams(void) : width(SIZE_DONT_CARE), height(SIZE_DONT_CARE), visibility(VISIBILITY_DONT_CARE)
    {
    }
    WindowParams(int width_, int height_, Visibility visibility_)
        : width(width_)
        , height(height_)
        , visibility(visibility_)
    {
    }
};

class WindowDestroyedError : public tcu::ResourceError
{
public:
    WindowDestroyedError(const std::string &message) : tcu::ResourceError(message)
    {
    }
};

class NativeWindow
{
public:
    enum Capability
    {
        CAPABILITY_CREATE_SURFACE_LEGACY = (1 << 0), //!< EGL surface can be created with eglCreateWindowSurface()
        CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION =
            (1 << 1), //!< EGL surface can be created with eglCreatePlatformWindowSurfaceEXT()
        CAPABILITY_CREATE_SURFACE_PLATFORM =
            (1 << 2), //!< EGL surface can be created with eglCreatePlatformWindowSurface()
        CAPABILITY_GET_SURFACE_SIZE   = (1 << 3),
        CAPABILITY_SET_SURFACE_SIZE   = (1 << 4),
        CAPABILITY_GET_SCREEN_SIZE    = (1 << 5),
        CAPABILITY_READ_SCREEN_PIXELS = (1 << 6),
        CAPABILITY_CHANGE_VISIBILITY  = (1 << 7)
    };

    virtual ~NativeWindow(void)
    {
    }

    //! Return EGLNativeWindowType that can be used with eglCreateWindowSurface(). Default implementation throws tcu::NotSupportedError().
    virtual eglw::EGLNativeWindowType getLegacyNative(void);

    //! Return native pointer that can be used with eglCreatePlatformWindowSurfaceEXT(). Default implementation throws tcu::NotSupportedError().
    virtual void *getPlatformExtension(void);

    //! Return native pointer that can be used with eglCreatePlatformWindowSurface(). Default implementation throws tcu::NotSupportedError().
    virtual void *getPlatformNative(void);

    // Process window events. Defaults to empty implementation, that does nothing.
    virtual void processEvents(void)
    {
    }

    // Get current size of window's logical surface. Default implementation throws tcu::NotSupportedError()
    virtual tcu::IVec2 getSurfaceSize(void) const;

    // Set the size of the window's logical surface. Default implementation throws tcu::NotSupportedError()
    virtual void setSurfaceSize(tcu::IVec2 size);

    // Get the size of the window in screen pixels. Default implementation throws tcu::NotSupportedError()
    virtual tcu::IVec2 getScreenSize(void) const;

    // Read screen (visible) pixels from window. Default implementation throws tcu::NotSupportedError()
    virtual void readScreenPixels(tcu::TextureLevel *dst) const;

    // Change window visibility. Default throws tcu::NotSupportedError().
    virtual void setVisibility(WindowParams::Visibility visibility);

    Capability getCapabilities(void) const
    {
        return m_capabilities;
    }

protected:
    NativeWindow(Capability capabilities);

private:
    NativeWindow(const NativeWindow &);
    NativeWindow &operator=(const NativeWindow &);

    const Capability m_capabilities;
};

class NativeWindowFactory : public tcu::FactoryBase
{
public:
    virtual ~NativeWindowFactory(void);

    //! Create generic NativeWindow
    virtual NativeWindow *createWindow(NativeDisplay *nativeDisplay, const WindowParams &params) const = 0;

    //! Create NativeWindow that matches given config. Defaults to generic createWindow().
    virtual NativeWindow *createWindow(NativeDisplay *nativeDisplay, eglw::EGLDisplay display, eglw::EGLConfig config,
                                       const eglw::EGLAttrib *attribList, const WindowParams &params) const;

    NativeWindow::Capability getCapabilities(void) const
    {
        return m_capabilities;
    }

protected:
    NativeWindowFactory(const std::string &name, const std::string &description, NativeWindow::Capability capabilities);

private:
    NativeWindowFactory(const NativeWindowFactory &);
    NativeWindowFactory &operator=(const NativeWindowFactory &);

    const NativeWindow::Capability m_capabilities;
};

typedef tcu::FactoryRegistry<NativeWindowFactory> NativeWindowFactoryRegistry;

} // namespace eglu

#endif // _EGLUNATIVEWINDOW_HPP
