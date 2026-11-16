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
 * \brief GL context factory using EGL.
 *//*--------------------------------------------------------------------*/

#include "egluGLContextFactory.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

#include "gluDefs.hpp"

#include "egluDefs.hpp"
#include "egluUtil.hpp"
#include "egluGLUtil.hpp"
#include "egluNativeWindow.hpp"
#include "egluNativePixmap.hpp"
#include "egluStrUtil.hpp"

#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

#include "glwInitFunctions.hpp"
#include "glwInitES20Direct.hpp"
#include "glwInitES30Direct.hpp"
#include "glwInitES31Direct.hpp"
#include "glwInitES32Direct.hpp"

#include "deDynamicLibrary.hpp"
#include "deSTLUtil.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <string>
#include <sstream>

using std::string;
using std::vector;

// \todo [2014-03-12 pyry] Use command line arguments for libraries?

// Default library names
#if !defined(DEQP_GLES2_LIBRARY_PATH)
#if (DE_OS == DE_OS_WIN32)
#define DEQP_GLES2_LIBRARY_PATH "libGLESv2.dll"
#else
#define DEQP_GLES2_LIBRARY_PATH "libGLESv2.so"
#endif
#endif

#if !defined(DEQP_GLES3_LIBRARY_PATH)
#define DEQP_GLES3_LIBRARY_PATH DEQP_GLES2_LIBRARY_PATH
#endif

#if !defined(DEQP_OPENGL_LIBRARY_PATH)
#if (DE_OS == DE_OS_WIN32)
#define DEQP_OPENGL_LIBRARY_PATH "opengl32.dll"
#else
#define DEQP_OPENGL_LIBRARY_PATH "libGL.so"
#endif
#endif

namespace eglu
{

using namespace eglw;

namespace
{

enum
{
    DEFAULT_OFFSCREEN_WIDTH  = 512,
    DEFAULT_OFFSCREEN_HEIGHT = 512
};

class GetProcFuncLoader : public glw::FunctionLoader
{
public:
    GetProcFuncLoader(const Library &egl) : m_egl(egl)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return (glw::GenericFuncType)m_egl.getProcAddress(name);
    }

protected:
    const Library &m_egl;
};

class DynamicFuncLoader : public glw::FunctionLoader
{
public:
    DynamicFuncLoader(de::DynamicLibrary *library) : m_library(library)
    {
    }

    glw::GenericFuncType get(const char *name) const
    {
        return (glw::GenericFuncType)m_library->getFunction(name);
    }

private:
    de::DynamicLibrary *m_library;
};

class RenderContext : public GLRenderContext
{
public:
    RenderContext(const NativeDisplayFactory *displayFactory, const NativeWindowFactory *windowFactory,
                  const NativePixmapFactory *pixmapFactory, const glu::RenderConfig &config,
                  const glu::RenderContext *sharedContext = nullptr);
    virtual ~RenderContext(void);

    virtual glu::ContextType getType(void) const
    {
        return m_renderConfig.type;
    }
    virtual const glw::Functions &getFunctions(void) const
    {
        return m_glFunctions;
    }
    virtual const tcu::RenderTarget &getRenderTarget(void) const
    {
        return m_glRenderTarget;
    }
    virtual void postIterate(void);

    virtual EGLDisplay getEGLDisplay(void) const
    {
        return m_eglDisplay;
    }
    virtual EGLContext getEGLContext(void) const
    {
        return m_eglContext;
    }
    virtual EGLConfig getEGLConfig(void) const
    {
        return m_eglConfig;
    }
    virtual const eglw::Library &getLibrary(void) const
    {
        return m_display->getLibrary();
    }

    virtual eglw::GenericFuncType getProcAddress(const char *name) const;

    virtual void makeCurrent(void);

private:
    void create(const NativeDisplayFactory *displayFactory, const NativeWindowFactory *windowFactory,
                const NativePixmapFactory *pixmapFactory, const glu::RenderConfig &config,
                const glu::RenderContext *sharedContext);
    void destroy(void);

    const glu::RenderConfig m_renderConfig;
    const NativeWindowFactory *const m_nativeWindowFactory; // Stored in case window must be re-created

    de::SharedPtr<NativeDisplay> m_display;
    NativeWindow *m_window;
    NativePixmap *m_pixmap;

    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    EGLSurface m_eglSurface;
    EGLContext m_eglContext;
    EGLContext m_eglSharedContext;

    tcu::RenderTarget m_glRenderTarget;
    de::DynamicLibrary *m_dynamicGLLibrary;
    glw::Functions m_glFunctions;
};

RenderContext::RenderContext(const NativeDisplayFactory *displayFactory, const NativeWindowFactory *windowFactory,
                             const NativePixmapFactory *pixmapFactory, const glu::RenderConfig &config,
                             const glu::RenderContext *sharedContext)
    : m_renderConfig(config)
    , m_nativeWindowFactory(windowFactory)
    , m_display(nullptr)
    , m_window(nullptr)
    , m_pixmap(nullptr)

    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglSurface(EGL_NO_SURFACE)
    , m_eglContext(EGL_NO_CONTEXT)
    , m_eglSharedContext(EGL_NO_CONTEXT)

    , m_dynamicGLLibrary(nullptr)
{
    DE_ASSERT(displayFactory);

    try
    {
        create(displayFactory, windowFactory, pixmapFactory, config, sharedContext);
    }
    catch (...)
    {
        destroy();
        throw;
    }
}

RenderContext::~RenderContext(void)
{
    try
    {
        destroy();
    }
    catch (...)
    {
        // destroy() calls EGL functions that are checked and may throw exceptions
    }

    delete m_window;
    delete m_pixmap;
    delete m_dynamicGLLibrary;
}

static WindowParams::Visibility getNativeWindowVisibility(glu::RenderConfig::Visibility visibility)
{
    using glu::RenderConfig;

    switch (visibility)
    {
    case RenderConfig::VISIBILITY_HIDDEN:
        return WindowParams::VISIBILITY_HIDDEN;
    case RenderConfig::VISIBILITY_VISIBLE:
        return WindowParams::VISIBILITY_VISIBLE;
    case RenderConfig::VISIBILITY_FULLSCREEN:
        return WindowParams::VISIBILITY_FULLSCREEN;
    default:
        DE_ASSERT((int)visibility == RenderConfig::DONT_CARE);
        return WindowParams::VISIBILITY_DONT_CARE;
    }
}

typedef std::pair<NativeWindow *, EGLSurface> WindowSurfacePair;
typedef std::pair<NativePixmap *, EGLSurface> PixmapSurfacePair;

WindowSurfacePair createWindow(NativeDisplay *nativeDisplay, const NativeWindowFactory *windowFactory,
                               EGLDisplay eglDisplay, EGLConfig eglConfig, const glu::RenderConfig &config)
{
    const int width  = (config.width == glu::RenderConfig::DONT_CARE ? WindowParams::SIZE_DONT_CARE : config.width);
    const int height = (config.height == glu::RenderConfig::DONT_CARE ? WindowParams::SIZE_DONT_CARE : config.height);
    const WindowParams::Visibility visibility = getNativeWindowVisibility(config.windowVisibility);
    NativeWindow *nativeWindow                = nullptr;
    EGLSurface surface                        = EGL_NO_SURFACE;
    const EGLAttrib attribList[]              = {EGL_NONE};

    nativeWindow = windowFactory->createWindow(nativeDisplay, eglDisplay, eglConfig, &attribList[0],
                                               WindowParams(width, height, visibility));

    try
    {
        surface = eglu::createWindowSurface(*nativeDisplay, *nativeWindow, eglDisplay, eglConfig, attribList);
    }
    catch (...)
    {
        delete nativeWindow;
        throw;
    }

    return WindowSurfacePair(nativeWindow, surface);
}

PixmapSurfacePair createPixmap(NativeDisplay *nativeDisplay, const NativePixmapFactory *pixmapFactory,
                               EGLDisplay eglDisplay, EGLConfig eglConfig, const glu::RenderConfig &config)
{
    const int width  = (config.width == glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_WIDTH : config.width);
    const int height = (config.height == glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_HEIGHT : config.height);
    NativePixmap *nativePixmap   = nullptr;
    EGLSurface surface           = EGL_NO_SURFACE;
    const EGLAttrib attribList[] = {EGL_NONE};

    nativePixmap = pixmapFactory->createPixmap(nativeDisplay, eglDisplay, eglConfig, &attribList[0], width, height);

    try
    {
        surface = eglu::createPixmapSurface(*nativeDisplay, *nativePixmap, eglDisplay, eglConfig, attribList);
    }
    catch (...)
    {
        delete nativePixmap;
        throw;
    }

    return PixmapSurfacePair(nativePixmap, surface);
}

EGLSurface createPBuffer(const Library &egl, EGLDisplay display, EGLConfig eglConfig, const glu::RenderConfig &config)
{
    const int width  = (config.width == glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_WIDTH : config.width);
    const int height = (config.height == glu::RenderConfig::DONT_CARE ? DEFAULT_OFFSCREEN_HEIGHT : config.height);
    EGLSurface surface;
    const EGLint attribList[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};

    surface = egl.createPbufferSurface(display, eglConfig, &(attribList[0]));
    EGLU_CHECK_MSG(egl, "eglCreatePbufferSurface()");

    return surface;
}

void RenderContext::makeCurrent(void)
{
    const Library &egl = m_display->getLibrary();

    EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));
}

glw::GenericFuncType RenderContext::getProcAddress(const char *name) const
{
    return (glw::GenericFuncType)m_display->getLibrary().getProcAddress(name);
}

void RenderContext::create(const NativeDisplayFactory *displayFactory, const NativeWindowFactory *windowFactory,
                           const NativePixmapFactory *pixmapFactory, const glu::RenderConfig &config,
                           const glu::RenderContext *sharedContext)
{
    glu::RenderConfig::SurfaceType surfaceType = config.surfaceType;

    DE_ASSERT(displayFactory);

    if (nullptr == sharedContext)
        m_display = de::SharedPtr<NativeDisplay>(displayFactory->createDisplay());
    else
    {
        const RenderContext *context = dynamic_cast<const RenderContext *>(sharedContext);
        m_eglSharedContext           = context->m_eglContext;
        m_display                    = context->m_display;
    }

    m_eglDisplay       = eglu::getDisplay(*m_display);
    const Library &egl = m_display->getLibrary();

    {
        EGLint major = 0;
        EGLint minor = 0;
        EGLU_CHECK_CALL(egl, initialize(m_eglDisplay, &major, &minor));
    }

    m_eglConfig = chooseConfig(egl, m_eglDisplay, config);

    if (surfaceType == glu::RenderConfig::SURFACETYPE_DONT_CARE)
    {
        // Choose based on what selected configuration supports
        const EGLint supportedTypes = eglu::getConfigAttribInt(egl, m_eglDisplay, m_eglConfig, EGL_SURFACE_TYPE);

        if ((supportedTypes & EGL_WINDOW_BIT) != 0)
            surfaceType = glu::RenderConfig::SURFACETYPE_WINDOW;
        else if ((supportedTypes & EGL_PBUFFER_BIT) != 0)
            surfaceType = glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC;
        else if ((supportedTypes & EGL_PIXMAP_BIT) != 0)
            surfaceType = glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE;
        else
            throw tcu::NotSupportedError("Selected EGL config doesn't support any surface types", nullptr, __FILE__,
                                         __LINE__);
    }

    switch (surfaceType)
    {
    case glu::RenderConfig::SURFACETYPE_WINDOW:
    {
        if (windowFactory)
        {
            const WindowSurfacePair windowSurface =
                createWindow(m_display.get(), windowFactory, m_eglDisplay, m_eglConfig, config);
            m_window     = windowSurface.first;
            m_eglSurface = windowSurface.second;
        }
        else
            throw tcu::NotSupportedError("EGL platform doesn't support windows", nullptr, __FILE__, __LINE__);
        break;
    }

    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
    {
        if (pixmapFactory)
        {
            const PixmapSurfacePair pixmapSurface =
                createPixmap(m_display.get(), pixmapFactory, m_eglDisplay, m_eglConfig, config);
            m_pixmap     = pixmapSurface.first;
            m_eglSurface = pixmapSurface.second;
        }
        else
            throw tcu::NotSupportedError("EGL platform doesn't support pixmaps", nullptr, __FILE__, __LINE__);
        break;
    }

    case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
        m_eglSurface = createPBuffer(egl, m_eglDisplay, m_eglConfig, config);
        break;

    default:
        throw tcu::InternalError("Invalid surface type");
    }

    m_eglContext = createGLContext(egl, m_eglDisplay, m_eglConfig, config.type, m_eglSharedContext,
                                   config.resetNotificationStrategy);

    EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

    // Init core functions

    if (hasExtension(egl, m_eglDisplay, "EGL_KHR_get_all_proc_addresses"))
    {
        // Use eglGetProcAddress() for core functions
        GetProcFuncLoader funcLoader(egl);
        glu::initCoreFunctions(&m_glFunctions, &funcLoader, config.type.getAPI());
    }
#if defined(DEQP_GLES2_DIRECT_LINK)
    else if (config.type.getAPI() == glu::ApiType::es(2, 0))
    {
        glw::initES20Direct(&m_glFunctions);
    }
#endif
#if defined(DEQP_GLES3_DIRECT_LINK)
    else if (config.type.getAPI() == glu::ApiType::es(3, 0))
    {
        glw::initES30Direct(&m_glFunctions);
    }
#endif
#if defined(DEQP_GLES31_DIRECT_LINK)
    else if (config.type.getAPI() == glu::ApiType::es(3, 1))
    {
        glw::initES31Direct(&m_glFunctions);
    }
#endif
#if defined(DEQP_GLES32_DIRECT_LINK)
    else if (config.type.getAPI() == glu::ApiType::es(3, 2))
    {
        glw::initES32Direct(&m_glFunctions);
    }
#endif
    else
    {
        const char *libraryPath = nullptr;

        if (glu::isContextTypeES(config.type))
        {
            if (config.type.getMinorVersion() <= 2)
                libraryPath = DEQP_GLES2_LIBRARY_PATH;
            else
                libraryPath = DEQP_GLES3_LIBRARY_PATH;
        }
        else
            libraryPath = DEQP_OPENGL_LIBRARY_PATH;

        m_dynamicGLLibrary = new de::DynamicLibrary(libraryPath);

        DynamicFuncLoader funcLoader(m_dynamicGLLibrary);
        glu::initCoreFunctions(&m_glFunctions, &funcLoader, config.type.getAPI());
    }

    // Init extension functions
    {
        GetProcFuncLoader extLoader(egl);
        glu::initExtensionFunctions(&m_glFunctions, &extLoader, config.type.getAPI());
    }

    {
        EGLint width, height, depthBits, stencilBits, numSamples;
        tcu::PixelFormat pixelFmt;

        egl.querySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH, &width);
        egl.querySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT, &height);

        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_RED_SIZE, &pixelFmt.redBits);
        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_GREEN_SIZE, &pixelFmt.greenBits);
        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_BLUE_SIZE, &pixelFmt.blueBits);
        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_ALPHA_SIZE, &pixelFmt.alphaBits);

        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_DEPTH_SIZE, &depthBits);
        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_STENCIL_SIZE, &stencilBits);
        egl.getConfigAttrib(m_eglDisplay, m_eglConfig, EGL_SAMPLES, &numSamples);

        EGLU_CHECK_MSG(egl, "Failed to query config attributes");

        m_glRenderTarget = tcu::RenderTarget(width, height, pixelFmt, depthBits, stencilBits, numSamples);
    }

    egl.swapInterval(m_eglDisplay, 0);
}

void RenderContext::destroy(void)
{
    if (m_eglDisplay != EGL_NO_DISPLAY)
    {
        const Library &egl = m_display->getLibrary();

        EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

        if (m_eglSurface != EGL_NO_SURFACE)
            EGLU_CHECK_CALL(egl, destroySurface(m_eglDisplay, m_eglSurface));

        if (m_eglContext != EGL_NO_CONTEXT)
            EGLU_CHECK_CALL(egl, destroyContext(m_eglDisplay, m_eglContext));

        if (m_eglSharedContext == EGL_NO_CONTEXT)
            EGLU_CHECK_CALL(egl, terminate(m_eglDisplay));

        m_eglDisplay = EGL_NO_DISPLAY;
        m_eglSurface = EGL_NO_SURFACE;
        m_eglContext = EGL_NO_CONTEXT;
    }

    delete m_window;
    delete m_pixmap;
    delete m_dynamicGLLibrary;

    m_window           = nullptr;
    m_pixmap           = nullptr;
    m_dynamicGLLibrary = nullptr;
}

void RenderContext::postIterate(void)
{
    const Library &egl = m_display->getLibrary();

    if (m_window)
    {
        EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

        EGLBoolean swapOk    = egl.swapBuffers(m_eglDisplay, m_eglSurface);
        EGLint error         = egl.getError();
        const bool badWindow = error == EGL_BAD_SURFACE || error == EGL_BAD_NATIVE_WINDOW;

        if (!swapOk && !badWindow)
            throw tcu::ResourceError(string("eglSwapBuffers() failed: ") + getErrorStr(error).toString());

        try
        {
            m_window->processEvents();
        }
        catch (const WindowDestroyedError &)
        {
            tcu::print("Warning: Window destroyed, recreating...\n");

            EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
            EGLU_CHECK_CALL(egl, destroySurface(m_eglDisplay, m_eglSurface));
            m_eglSurface = EGL_NO_SURFACE;

            delete m_window;
            m_window = nullptr;

            try
            {
                WindowSurfacePair windowSurface =
                    createWindow(m_display.get(), m_nativeWindowFactory, m_eglDisplay, m_eglConfig, m_renderConfig);
                m_window     = windowSurface.first;
                m_eglSurface = windowSurface.second;

                EGLU_CHECK_CALL(egl, makeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext));

                swapOk = EGL_TRUE;
                error  = EGL_SUCCESS;
            }
            catch (const std::exception &e)
            {
                if (m_eglSurface)
                {
                    egl.makeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    egl.destroySurface(m_eglDisplay, m_eglSurface);
                    m_eglSurface = EGL_NO_SURFACE;
                }

                delete m_window;
                m_window = nullptr;

                throw tcu::ResourceError(string("Failed to re-create window: ") + e.what());
            }
        }

        if (!swapOk)
        {
            DE_ASSERT(badWindow);
            throw tcu::ResourceError(string("eglSwapBuffers() failed: ") + getErrorStr(error).toString());
        }

        // Refresh dimensions
        {
            int newWidth  = 0;
            int newHeight = 0;

            egl.querySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH, &newWidth);
            egl.querySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT, &newHeight);
            EGLU_CHECK_MSG(egl, "Failed to query window size");

            if (newWidth != m_glRenderTarget.getWidth() || newHeight != m_glRenderTarget.getHeight())
            {
                tcu::print("Warning: Window size changed (%dx%d -> %dx%d), test results might be invalid!\n",
                           m_glRenderTarget.getWidth(), m_glRenderTarget.getHeight(), newWidth, newHeight);

                m_glRenderTarget = tcu::RenderTarget(newWidth, newHeight, m_glRenderTarget.getPixelFormat(),
                                                     m_glRenderTarget.getDepthBits(), m_glRenderTarget.getStencilBits(),
                                                     m_glRenderTarget.getNumSamples());
            }
        }
    }
    else
        m_glFunctions.flush();
}

} // namespace

GLContextFactory::GLContextFactory(const NativeDisplayFactoryRegistry &displayFactoryRegistry)
    : glu::ContextFactory("egl", "EGL OpenGL Context")
    , m_displayFactoryRegistry(displayFactoryRegistry)
{
}

glu::RenderContext *GLContextFactory::createContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine,
                                                    const glu::RenderContext *sharedContext) const
{
    const NativeDisplayFactory &displayFactory = selectNativeDisplayFactory(m_displayFactoryRegistry, cmdLine);

    const NativeWindowFactory *windowFactory;
    const NativePixmapFactory *pixmapFactory;

    try
    {
        windowFactory = &selectNativeWindowFactory(displayFactory, cmdLine);
    }
    catch (const tcu::NotSupportedError &)
    {
        windowFactory = nullptr;
    }

    try
    {
        pixmapFactory = &selectNativePixmapFactory(displayFactory, cmdLine);
    }
    catch (const tcu::NotSupportedError &)
    {
        pixmapFactory = nullptr;
    }

    return new RenderContext(&displayFactory, windowFactory, pixmapFactory, config, sharedContext);
}

} // namespace eglu
