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
 * \brief QNX Screen Platform implementation.
 *//*--------------------------------------------------------------------*/

#include "tcuQnxScreenPlatform.hpp"

// Define pointers to libscreen functions
extern "C"
{
    typedef int (*PFNSCREENCREATECONTEXT)(screen_context_t *, int);
    typedef int (*PFNSCREENDESTROYCONTEXT)(screen_context_t);
    typedef int (*PFNSCREENCREATEWINDOW)(screen_window_t *, screen_context_t);
    typedef int (*PFNSCREENSETWINDOWPROPERTYIV)(screen_window_t, int, const int *);
    typedef int (*PFNXSCREENCREATEWINDOWBUFFERS)(screen_window_t, int);
    typedef int (*PFNSCREENDESTROYWINDOW)(screen_window_t);
};

static PFNSCREENCREATECONTEXT pscreen_create_context               = NULL;
static PFNSCREENDESTROYCONTEXT pscreen_destroy_context             = NULL;
static PFNSCREENCREATEWINDOW pscreen_create_window                 = NULL;
static PFNSCREENSETWINDOWPROPERTYIV pscreen_set_window_property_iv = NULL;
static PFNXSCREENCREATEWINDOWBUFFERS pscreen_create_window_buffers = NULL;
static PFNSCREENDESTROYWINDOW pscreen_destroy_window               = NULL;

static bool loadedLibscreen{false};
static screen_context_t screenContext{nullptr};
static void *libScreen{nullptr};

tcu::Platform *createPlatform(void)
{
    return new tcu::QnxScreen::Platform();
}

namespace tcu
{
namespace QnxScreen
{

enum
{
    DEFAULT_WINDOW_WIDTH  = 400,
    DEFAULT_WINDOW_HEIGHT = 300
};

// Macro to load function pointers from library
#define LOAD_FUNC_PTR(lib, name, type)                                              \
    do                                                                              \
    {                                                                               \
        p##name = (type)dlsym(lib, #name);                                          \
        if (!p##name)                                                               \
        {                                                                           \
            throw ResourceError("Could not load required function from libscreen"); \
        }                                                                           \
    } while (0)

DisplayFactory::DisplayFactory(void)
    : eglu::NativeDisplayFactory("QNX", "QNX Screen Display", eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY)
{
    m_nativeWindowRegistry.registerFactory(new WindowFactory());
}

eglu::NativeDisplay *DisplayFactory::createDisplay(const eglw::EGLAttrib *) const
{
    return new Display();
}

eglu::NativeWindow *WindowFactory::createWindow(eglu::NativeDisplay *display, const eglu::WindowParams &params) const
{
    const int width  = params.width != eglu::WindowParams::SIZE_DONT_CARE ? params.width : DEFAULT_WINDOW_WIDTH;
    const int height = params.height != eglu::WindowParams::SIZE_DONT_CARE ? params.height : DEFAULT_WINDOW_HEIGHT;

    return new Window(display, width, height, NULL, NULL);
}

eglu::NativeWindow *WindowFactory::createWindow(eglu::NativeDisplay *display, eglw::EGLDisplay eglDisplay,
                                                eglw::EGLConfig config, const eglw::EGLAttrib *attribList,
                                                const eglu::WindowParams &params) const
{
    DE_UNREF(attribList);

    const int width  = params.width != eglu::WindowParams::SIZE_DONT_CARE ? params.width : DEFAULT_WINDOW_WIDTH;
    const int height = params.height != eglu::WindowParams::SIZE_DONT_CARE ? params.height : DEFAULT_WINDOW_HEIGHT;

    return new Window(display, width, height, eglDisplay, config);
}

void Display::LoadLibscreen(void)
{
    if (!loadedLibscreen)
    {
        if (!libScreen)
        {
            libScreen = dlopen("libscreen.so", RTLD_LAZY);
            if (!libScreen)
            {
                libScreen = dlopen("libscreen.so.1", RTLD_LAZY);
            }
            if (!libScreen)
            {
                throw ResourceError("Could not find / open libscreen");
            }
        }
        LOAD_FUNC_PTR(libScreen, screen_create_context, PFNSCREENCREATECONTEXT);
        LOAD_FUNC_PTR(libScreen, screen_destroy_context, PFNSCREENDESTROYCONTEXT);
        LOAD_FUNC_PTR(libScreen, screen_create_window, PFNSCREENCREATEWINDOW);
        LOAD_FUNC_PTR(libScreen, screen_set_window_property_iv, PFNSCREENSETWINDOWPROPERTYIV);
        LOAD_FUNC_PTR(libScreen, screen_create_window_buffers, PFNXSCREENCREATEWINDOWBUFFERS);
        LOAD_FUNC_PTR(libScreen, screen_destroy_window, PFNSCREENDESTROYWINDOW);
        loadedLibscreen = true;
    }
}

void Display::CreateScreenContext(void)
{
    if (!screenContext)
    {
        const int ret{pscreen_create_context(&screenContext, 0)};
        if (ret)
        {
            throw ResourceError("Failed to create QNX Screen context");
        }
    }
}

void Display::DestroyScreenContext(void)
{
    if (screenContext)
    {
        pscreen_destroy_context(screenContext);
        screenContext = nullptr;
    }
}

Display::Display(void) : eglu::NativeDisplay(eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY), m_display(0)
{
    try
    {
        LoadLibscreen();
        CreateScreenContext();
    }
    catch (...)
    {
        if (loadedLibscreen)
        {
            DestroyScreenContext();
        }
        throw; // Re-throw caught exception
    }
}

Display::~Display(void)
{
    DestroyScreenContext();
}

Window::Window(eglu::NativeDisplay *display, int width, int height, eglw::EGLDisplay eglDisplay, eglw::EGLConfig config)
    : eglu::NativeWindow(eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY)
    , m_screenWindow(nullptr)
{
    // offset is 0
    // sizeX and sizeY is width and height
    eglw::EGLint abits{};
    eglw::EGLint rbits{};
    eglw::EGLint gbits{};
    eglw::EGLint bbits{};

    display->getLibrary().getConfigAttrib(eglDisplay, config, EGL_ALPHA_SIZE, &abits);
    display->getLibrary().getConfigAttrib(eglDisplay, config, EGL_RED_SIZE, &rbits);
    display->getLibrary().getConfigAttrib(eglDisplay, config, EGL_GREEN_SIZE, &gbits);
    display->getLibrary().getConfigAttrib(eglDisplay, config, EGL_BLUE_SIZE, &bbits);

    try
    {
        int rc{pscreen_create_window(&m_screenWindow, screenContext)};
        if (rc)
        {
            throw ResourceError("Failed to create QNX Screen window.");
        }

        int screenFormat{};
        if ((rbits == 8) && (gbits == 8) && (bbits == 8))
        {
            // RGB888. Check for alpha
            if (abits == 8)
            {
                screenFormat = SCREEN_FORMAT_RGBA8888;
            }
            else
            {
                screenFormat = SCREEN_FORMAT_RGBX8888;
            }
        }
        else if ((rbits == 5) && (gbits == 6) && (bbits == 5))
        {
            screenFormat = SCREEN_FORMAT_RGB565;
        }
        else if ((rbits == 4) && (gbits == 4) && (bbits == 4))
        {
            // RGB444. Check for alpha
            if (abits == 4)
            {
                screenFormat = SCREEN_FORMAT_RGBA4444;
            }
            else
            {
                screenFormat = SCREEN_FORMAT_RGBX4444;
            }
        }
        else if ((rbits == 5) && (gbits == 5) && (bbits == 5))
        {
            // RGB555. Check for alpha
            if (abits == 1)
            {
                screenFormat = SCREEN_FORMAT_RGBA5551;
            }
            else
            {
                screenFormat = SCREEN_FORMAT_RGBX5551;
            }
        }
        else
        {
            throw ResourceError("Unsupported SCREEN_PROPERTY_FORMAT requested");
        }

        rc = pscreen_set_window_property_iv(m_screenWindow, SCREEN_PROPERTY_FORMAT, (const int *)&screenFormat);
        if (rc)
        {
            throw ResourceError("Failed to set SCREEN_PROPERTY_FORMAT");
        }

        int usage{SCREEN_USAGE_OPENGL_ES2};
        rc = pscreen_set_window_property_iv(m_screenWindow, SCREEN_PROPERTY_USAGE, &usage);
        if (rc)
        {
            throw ResourceError("Failed to set SCREEN_PROPERTY_USAGE");
        }

        int interval{1};
        rc = pscreen_set_window_property_iv(m_screenWindow, SCREEN_PROPERTY_SWAP_INTERVAL, &interval);
        if (rc)
        {
            throw ResourceError("Failed to set SCREEN_PROPERTY_SWAP_INTERVAL");
        }

        int size[]{width, height};
        rc = pscreen_set_window_property_iv(m_screenWindow, SCREEN_PROPERTY_SIZE, (const int *)size);
        if (rc)
        {
            throw ResourceError("Failed to set SCREEN_PROPERTY_SIZE");
        }

        int offset[]{0, 0};
        rc = pscreen_set_window_property_iv(m_screenWindow, SCREEN_PROPERTY_POSITION, offset);
        if (rc)
        {
            throw ResourceError("Failed to set SCREEN_PROPERTY_POSITION");
        }

        rc = pscreen_create_window_buffers(m_screenWindow, 2);
        if (rc)
        {
            throw ResourceError("Failed to create QNX Screen window buffers");
        }

        m_nativeWindow = (eglw::EGLNativeWindowType)(void *)m_screenWindow;
    }
    catch (...)
    {
        if (m_screenWindow)
        {
            pscreen_destroy_window(m_screenWindow);
            m_screenWindow = nullptr;
        }
        throw;
    }
}

Window::~Window(void)
{
    if (m_screenWindow)
    {
        pscreen_destroy_window(m_screenWindow);
        m_screenWindow = nullptr;
    }
}

Platform::Platform(void)
{
    m_nativeDisplayFactoryRegistry.registerFactory(new DisplayFactory());
    m_contextFactoryRegistry.registerFactory(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
}

Platform::~Platform(void)
{
    if (screenContext)
    {
        pscreen_destroy_context(screenContext);
    }
    screenContext = nullptr;
}

} // namespace QnxScreen
} // namespace tcu
