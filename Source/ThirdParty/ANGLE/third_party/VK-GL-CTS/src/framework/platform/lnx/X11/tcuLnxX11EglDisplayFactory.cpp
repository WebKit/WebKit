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
 * \brief X11Egl Display Factory.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxX11EglDisplayFactory.hpp"
#include "tcuLnxX11.hpp"
#include "egluGLContextFactory.hpp"
#include "eglwLibrary.hpp"
#include "eglwFunctions.hpp"
#include "eglwEnums.hpp"
#include "deUniquePtr.hpp"

namespace tcu
{
namespace lnx
{
namespace x11
{
namespace egl
{

typedef ::Display *EGLNativeDisplayType;
typedef ::Pixmap EGLNativePixmapType;
typedef ::Window EGLNativeWindowType;

DE_STATIC_ASSERT(sizeof(EGLNativeDisplayType) <= sizeof(eglw::EGLNativeDisplayType));
DE_STATIC_ASSERT(sizeof(EGLNativePixmapType) <= sizeof(eglw::EGLNativePixmapType));
DE_STATIC_ASSERT(sizeof(EGLNativeWindowType) <= sizeof(eglw::EGLNativeWindowType));

extern "C"
{

    typedef EGLW_APICALL eglw::EGLDisplay(EGLW_APIENTRY *eglX11GetDisplayFunc)(EGLNativeDisplayType display_id);
    typedef EGLW_APICALL eglw::EGLBoolean(EGLW_APIENTRY *eglX11CopyBuffersFunc)(eglw::EGLDisplay dpy,
                                                                                eglw::EGLSurface surface,
                                                                                EGLNativePixmapType target);
    typedef EGLW_APICALL eglw::EGLSurface(EGLW_APIENTRY *eglX11CreatePixmapSurfaceFunc)(
        eglw::EGLDisplay dpy, eglw::EGLConfig config, EGLNativePixmapType pixmap, const eglw::EGLint *attrib_list);
    typedef EGLW_APICALL eglw::EGLSurface(EGLW_APIENTRY *eglX11CreateWindowSurfaceFunc)(
        eglw::EGLDisplay dpy, eglw::EGLConfig config, EGLNativeWindowType win, const eglw::EGLint *attrib_list);
}

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

class Library : public eglw::DefaultLibrary
{
public:
    Library(void) : eglw::DefaultLibrary("libEGL.so")
    {
    }

    eglw::EGLBoolean copyBuffers(eglw::EGLDisplay dpy, eglw::EGLSurface surface, eglw::EGLNativePixmapType target) const
    {
        return (m_egl.copyBuffers)(dpy, surface, reinterpret_cast<EGLNativePixmapType *>(target));
    }

    eglw::EGLSurface createPixmapSurface(eglw::EGLDisplay dpy, eglw::EGLConfig config, eglw::EGLNativePixmapType pixmap,
                                         const eglw::EGLint *attrib_list) const
    {
        return (m_egl.createPixmapSurface)(dpy, config, reinterpret_cast<EGLNativePixmapType *>(pixmap), attrib_list);
    }

    eglw::EGLSurface createWindowSurface(eglw::EGLDisplay dpy, eglw::EGLConfig config, eglw::EGLNativeWindowType win,
                                         const eglw::EGLint *attrib_list) const
    {
        return (m_egl.createWindowSurface)(dpy, config, reinterpret_cast<EGLNativeWindowType *>(win), attrib_list);
    }

    eglw::EGLDisplay getDisplay(eglw::EGLNativeDisplayType display_id) const
    {
        return ((eglX11GetDisplayFunc)m_egl.getDisplay)(reinterpret_cast<EGLNativeDisplayType>(display_id));
    }
};

class Display : public NativeDisplay
{
public:
    static const Capability CAPABILITIES =
        Capability(CAPABILITY_GET_DISPLAY_LEGACY | CAPABILITY_GET_DISPLAY_PLATFORM_EXT);

    Display(MovePtr<XlibDisplay> x11Display)
        : NativeDisplay(CAPABILITIES, EGL_PLATFORM_X11_EXT, "EGL_EXT_platform_x11")
        , m_display(x11Display)
    {
    }

    void *getPlatformNative(void)
    {
        return m_display->getXDisplay();
    }
    eglw::EGLNativeDisplayType getPlatformExtension(void)
    {
        return reinterpret_cast<eglw::EGLNativeDisplayType>(m_display->getXDisplay());
    }
    eglw::EGLNativeDisplayType getLegacyNative(void)
    {
        return reinterpret_cast<eglw::EGLNativeDisplayType>(m_display->getXDisplay());
    }

    XlibDisplay &getX11Display(void)
    {
        return *m_display;
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
    Library m_library;
    UniquePtr<XlibDisplay> m_display;
};

class Window : public NativeWindow
{
public:
    static const Capability CAPABILITIES =
        Capability(CAPABILITY_CREATE_SURFACE_LEGACY | CAPABILITY_CREATE_SURFACE_PLATFORM |
                   CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION | CAPABILITY_GET_SURFACE_SIZE |
                   CAPABILITY_SET_SURFACE_SIZE | CAPABILITY_GET_SCREEN_SIZE);

    Window(Display &display, const WindowParams &params, Visual *visual);

    eglw::EGLNativeWindowType getLegacyNative(void)
    {
        return reinterpret_cast<eglw::EGLNativeWindowType>(m_window.getXID());
    }
    void *getPlatformExtension(void)
    {
        return &m_window.getXID();
    }
    void *getPlatformNative(void)
    {
        return &m_window.getXID();
    }

    IVec2 getSurfaceSize(void) const;
    void setSurfaceSize(IVec2 size);
    IVec2 getScreenSize(void) const
    {
        return getSurfaceSize();
    }

private:
    XlibWindow m_window;
};

Window::Window(Display &display, const WindowParams &params, Visual *visual)
    : NativeWindow(CAPABILITIES)
    , m_window(display.getX11Display(), params.width, params.height, visual)
{
    m_window.setVisibility((params.visibility != WindowParams::VISIBILITY_HIDDEN));
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

WindowFactory::WindowFactory(void) : NativeWindowFactory("window", "X11 Window", Window::CAPABILITIES)
{
}

NativeWindow *WindowFactory::createWindow(NativeDisplay *nativeDisplay, const WindowParams &params) const
{
    Display &display = *dynamic_cast<Display *>(nativeDisplay);

    return new Window(display, params, nullptr);
}

NativeWindow *WindowFactory::createWindow(NativeDisplay *nativeDisplay, eglw::EGLDisplay eglDisplay,
                                          eglw::EGLConfig config, const eglw::EGLAttrib *attribList,
                                          const WindowParams &params) const
{
    DE_UNREF(attribList);

    Display &display      = *dynamic_cast<Display *>(nativeDisplay);
    eglw::EGLint visualID = 0;
    ::Visual *visual      = nullptr;
    nativeDisplay->getLibrary().getConfigAttrib(eglDisplay, config, EGL_NATIVE_VISUAL_ID, &visualID);

    if (visualID != 0)
        visual = display.getX11Display().getVisual(visualID);

    return new Window(display, params, visual);
}

#if 0
class Pixmap : public NativePixmap
{
public:
    enum {
        CAPABILITIES = (CAPABILITY_CREATE_SURFACE_LEGACY |
                        CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION |
                        CAPABILITY_READ_PIXELS)
    };

                            Pixmap                (MovePtr<x11::Pixmap> x11Pixmap)
                                : NativePixmap    (CAPABILITIES)
                                , m_pixmap        (x11Pixmap) {}

    void*                    getPlatformNative    (void) { return &m_pixmap.getXID(); }
    void                    readPixels            (TextureLevel* dst);

private:
    UniquePtr<x11::Pixmap>    m_pixmap;
};

class PixmapFactory : public NativePixmapFactory
{
public:
                    PixmapFactory    (void)
                        : NativePixmapFactory ("pixmap", "X11 Pixmap", Pixmap::CAPABILITIES) {}

    NativePixmap*    createPixmap    (NativeDisplay* nativeDisplay,
                                     int            width,
                                     int            height) const;
};

NativePixmap* PixmapFactory::createPixmap (NativeDisplay* nativeDisplay,
                                           int            width,
                                           int            height) const

{
    Display*                display = dynamic_cast<Display*>(nativeDisplay);
    MovePtr<x11::Pixmap>    x11Pixmap    (new x11::Pixmap(display->getX11Display(),
                                                         width, height));
    return new Pixmap(x11Pixmap);
}
#endif

class DisplayFactory : public NativeDisplayFactory
{
public:
    DisplayFactory(EventState &eventState);

    NativeDisplay *createDisplay(const eglw::EGLAttrib *attribList) const;

private:
    EventState &m_eventState;
};

DisplayFactory::DisplayFactory(EventState &eventState)
    : NativeDisplayFactory("x11", "Native X11 Display", Display::CAPABILITIES, EGL_PLATFORM_X11_SCREEN_EXT,
                           "EGL_EXT_platform_x11")
    , m_eventState(eventState)
{
    m_nativeWindowRegistry.registerFactory(new WindowFactory());
    // m_nativePixmapRegistry.registerFactory(new PixmapFactory());
}

NativeDisplay *DisplayFactory::createDisplay(const eglw::EGLAttrib *attribList) const
{
    DE_UNREF(attribList);

    //! \todo [2014-03-18 lauri] Somehow make the display configurable from command line
    MovePtr<XlibDisplay> x11Display(new XlibDisplay(m_eventState, nullptr));

    return new Display(x11Display);
}

NativeDisplayFactory *createDisplayFactory(EventState &eventState)
{
    return new DisplayFactory(eventState);
}

} // namespace egl
} // namespace x11
} // namespace lnx
} // namespace tcu
