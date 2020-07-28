/*
 * Copyright (C) 2012 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GLContextEGL.h"

#if USE(EGL)

#include "GraphicsContextGLOpenGL.h"
#include "Logging.h"
#include "PlatformDisplay.h"

#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if USE(CAIRO)
#include <cairo.h>
#endif

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "OpenGLShims.h"
#endif

#if ENABLE(ACCELERATED_2D_CANVAS)
// cairo-gl.h includes some definitions from GLX that conflict with
// the ones provided by us. Since GLContextEGL doesn't use any GLX
// functions we can safely disable them.
#undef CAIRO_HAS_GLX_FUNCTIONS
#include <cairo-gl.h>
#endif

#include <wtf/Vector.h>

namespace WebCore {

#if USE(OPENGL_ES)
static const char* gEGLAPIName = "OpenGL ES";
static const EGLenum gEGLAPIVersion = EGL_OPENGL_ES_API;
#else
static const char* gEGLAPIName = "OpenGL";
static const EGLenum gEGLAPIVersion = EGL_OPENGL_API;
#endif

const char* GLContextEGL::errorString(int statusCode)
{
    static_assert(sizeof(int) >= sizeof(EGLint), "EGLint must not be wider than int");
    switch (statusCode) {
#define CASE_RETURN_STRING(name) case name: return #name
        // https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglGetError.xhtml
        CASE_RETURN_STRING(EGL_SUCCESS);
        CASE_RETURN_STRING(EGL_NOT_INITIALIZED);
        CASE_RETURN_STRING(EGL_BAD_ACCESS);
        CASE_RETURN_STRING(EGL_BAD_ALLOC);
        CASE_RETURN_STRING(EGL_BAD_ATTRIBUTE);
        CASE_RETURN_STRING(EGL_BAD_CONTEXT);
        CASE_RETURN_STRING(EGL_BAD_CONFIG);
        CASE_RETURN_STRING(EGL_BAD_CURRENT_SURFACE);
        CASE_RETURN_STRING(EGL_BAD_DISPLAY);
        CASE_RETURN_STRING(EGL_BAD_SURFACE);
        CASE_RETURN_STRING(EGL_BAD_MATCH);
        CASE_RETURN_STRING(EGL_BAD_PARAMETER);
        CASE_RETURN_STRING(EGL_BAD_NATIVE_PIXMAP);
        CASE_RETURN_STRING(EGL_BAD_NATIVE_WINDOW);
        CASE_RETURN_STRING(EGL_CONTEXT_LOST);
#undef CASE_RETURN_STRING
    default: return "Unknown EGL error";
    }
}

const char* GLContextEGL::lastErrorString()
{
    return errorString(eglGetError());
}

bool GLContextEGL::getEGLConfig(EGLDisplay display, EGLConfig* config, EGLSurfaceType surfaceType)
{
    std::array<EGLint, 4> rgbaSize = { 8, 8, 8, 8 };
    if (const char* environmentVariable = getenv("WEBKIT_EGL_PIXEL_LAYOUT")) {
        if (!strcmp(environmentVariable, "RGB565"))
            rgbaSize = { 5, 6, 5, 0 };
        else
            WTFLogAlways("Unknown pixel layout %s, falling back to RGBA8888", environmentVariable);
    }

    EGLint attributeList[] = {
#if USE(OPENGL_ES)
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
        EGL_RED_SIZE, rgbaSize[0],
        EGL_GREEN_SIZE, rgbaSize[1],
        EGL_BLUE_SIZE, rgbaSize[2],
        EGL_ALPHA_SIZE, rgbaSize[3],
        EGL_STENCIL_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_NONE,
        EGL_NONE
    };

    switch (surfaceType) {
    case GLContextEGL::PbufferSurface:
        attributeList[13] = EGL_PBUFFER_BIT;
        break;
    case GLContextEGL::PixmapSurface:
        attributeList[13] = EGL_PIXMAP_BIT;
        break;
    case GLContextEGL::WindowSurface:
    case GLContextEGL::Surfaceless:
        attributeList[13] = EGL_WINDOW_BIT;
        break;
    }

    EGLint count;
    if (!eglChooseConfig(display, attributeList, nullptr, 0, &count)) {
        RELEASE_LOG_INFO(Compositing, "Cannot get count of available EGL configurations: %s.", lastErrorString());
        return false;
    }

    EGLint numberConfigsReturned;
    Vector<EGLConfig> configs(count);
    if (!eglChooseConfig(display, attributeList, reinterpret_cast<EGLConfig*>(configs.data()), count, &numberConfigsReturned) || !numberConfigsReturned) {
        RELEASE_LOG_INFO(Compositing, "Cannot get available EGL configurations: %s.", lastErrorString());
        return false;
    }

    auto index = configs.findMatching([&](EGLConfig value) {
        EGLint redSize, greenSize, blueSize, alphaSize;
        eglGetConfigAttrib(display, value, EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(display, value, EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(display, value, EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(display, value, EGL_ALPHA_SIZE, &alphaSize);
        return redSize == rgbaSize[0] && greenSize == rgbaSize[1]
            && blueSize == rgbaSize[2] && alphaSize == rgbaSize[3];
    });

    if (index != notFound) {
        *config = configs[index];
        return true;
    }

    RELEASE_LOG_INFO(Compositing, "Could not find suitable EGL configuration out of %zu checked.", configs.size());
    return false;
}

std::unique_ptr<GLContextEGL> GLContextEGL::createWindowContext(GLNativeWindowType window, PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, WindowSurface)) {
        RELEASE_LOG_INFO(Compositing, "Cannot obtain EGL window context configuration: %s\n", lastErrorString());
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(platformDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL window context: %s\n", lastErrorString());
        return nullptr;
    }

    EGLSurface surface;
    switch (platformDisplay.type()) {
#if PLATFORM(X11)
    case PlatformDisplay::Type::X11:
        surface = createWindowSurfaceX11(display, config, window);
        break;
#endif
#if PLATFORM(WAYLAND)
    case PlatformDisplay::Type::Wayland:
        surface = createWindowSurfaceWayland(display, config, window);
        break;
#endif
#if USE(WPE_RENDERER)
    case PlatformDisplay::Type::WPE:
        surface = createWindowSurfaceWPE(display, config, window);
        break;
#endif // USE(WPE_RENDERER)
    }

    if (surface == EGL_NO_SURFACE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL window surface: %s. Retrying with fallback.", lastErrorString());
        surface = eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(window), nullptr);
    }

    if (surface == EGL_NO_SURFACE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL window surface: %s\n", lastErrorString());
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, WindowSurface));
}

std::unique_ptr<GLContextEGL> GLContextEGL::createPbufferContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    EGLConfig config;
    if (!getEGLConfig(display, &config, PbufferSurface)) {
        RELEASE_LOG_INFO(Compositing, "Cannot obtain EGL Pbuffer configuration: %s\n", lastErrorString());
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(platformDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL Pbuffer context: %s\n", lastErrorString());
        return nullptr;
    }

    static const int pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttributes);
    if (surface == EGL_NO_SURFACE) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL Pbuffer surface: %s\n", lastErrorString());
        eglDestroyContext(display, context);
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, surface, PbufferSurface));
}

std::unique_ptr<GLContextEGL> GLContextEGL::createSurfacelessContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    EGLDisplay display = platformDisplay.eglDisplay();
    if (display == EGL_NO_DISPLAY) {
        RELEASE_LOG_INFO(Compositing, "Cannot create surfaceless EGL context: invalid display (last error: %s)\n", lastErrorString());
        return nullptr;
    }

    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (!GLContext::isExtensionSupported(extensions, "EGL_KHR_surfaceless_context") && !GLContext::isExtensionSupported(extensions, "EGL_KHR_surfaceless_opengl")) {
        RELEASE_LOG_INFO(Compositing, "Cannot create surfaceless EGL context: required extensions missing.");
        return nullptr;
    }

    EGLConfig config;
    if (!getEGLConfig(display, &config, Surfaceless)) {
        RELEASE_LOG_INFO(Compositing, "Cannot obtain EGL surfaceless configuration: %s\n", lastErrorString());
        return nullptr;
    }

    EGLContext context = createContextForEGLVersion(platformDisplay, config, sharingContext);
    if (context == EGL_NO_CONTEXT) {
        RELEASE_LOG_INFO(Compositing, "Cannot create EGL surfaceless context: %s\n", lastErrorString());
        return nullptr;
    }

    return std::unique_ptr<GLContextEGL>(new GLContextEGL(platformDisplay, context, EGL_NO_SURFACE, Surfaceless));
}

std::unique_ptr<GLContextEGL> GLContextEGL::createContext(GLNativeWindowType window, PlatformDisplay& platformDisplay)
{
    if (platformDisplay.eglDisplay() == EGL_NO_DISPLAY) {
        WTFLogAlways("Cannot create EGL context: invalid display (last error: %s)\n", lastErrorString());
        return nullptr;
    }

    if (eglBindAPI(gEGLAPIVersion) == EGL_FALSE) {
        WTFLogAlways("Cannot create EGL context: error binding %s API (%s)\n", gEGLAPIName, lastErrorString());
        return nullptr;
    }

    EGLContext eglSharingContext = platformDisplay.sharingGLContext() ? static_cast<GLContextEGL*>(platformDisplay.sharingGLContext())->m_context : EGL_NO_CONTEXT;
    auto context = window ? createWindowContext(window, platformDisplay, eglSharingContext) : nullptr;
    if (!context)
        context = createSurfacelessContext(platformDisplay, eglSharingContext);
    if (!context) {
        switch (platformDisplay.type()) {
#if PLATFORM(X11)
        case PlatformDisplay::Type::X11:
            context = createPixmapContext(platformDisplay, eglSharingContext);
            break;
#endif
#if PLATFORM(WAYLAND)
        case PlatformDisplay::Type::Wayland:
            context = createWaylandContext(platformDisplay, eglSharingContext);
            break;
#endif
#if USE(WPE_RENDERER)
        case PlatformDisplay::Type::WPE:
            context = createWPEContext(platformDisplay, eglSharingContext);
            break;
#endif
        }
    }
    if (!context) {
        RELEASE_LOG_INFO(Compositing, "Could not create platform context: %s. Using Pbuffer as fallback.", lastErrorString());
        context = createPbufferContext(platformDisplay, eglSharingContext);
        if (!context)
            RELEASE_LOG_INFO(Compositing, "Could not create Pbuffer context: %s.", lastErrorString());
    }

    if (!context)
        WTFLogAlways("Could not create EGL context.");
    return context;
}

std::unique_ptr<GLContextEGL> GLContextEGL::createSharingContext(PlatformDisplay& platformDisplay)
{
    if (platformDisplay.eglDisplay() == EGL_NO_DISPLAY) {
        WTFLogAlways("Cannot create EGL sharing context: invalid display (last error: %s)", lastErrorString());
        return nullptr;
    }

    if (eglBindAPI(gEGLAPIVersion) == EGL_FALSE) {
        WTFLogAlways("Cannot create EGL sharing context: error binding %s API (%s)\n", gEGLAPIName, lastErrorString());
        return nullptr;
    }

    auto context = createSurfacelessContext(platformDisplay);
    if (!context) {
        switch (platformDisplay.type()) {
#if PLATFORM(X11)
        case PlatformDisplay::Type::X11:
            context = createPixmapContext(platformDisplay);
            break;
#endif
#if PLATFORM(WAYLAND)
        case PlatformDisplay::Type::Wayland:
            context = createWaylandContext(platformDisplay);
            break;
#endif
#if USE(WPE_RENDERER)
        case PlatformDisplay::Type::WPE:
            context = createWPEContext(platformDisplay);
            break;
#endif
        }
    }
    if (!context) {
        RELEASE_LOG_INFO(Compositing, "Could not create platform context: %s. Using Pbuffer as fallback.", lastErrorString());
        context = createPbufferContext(platformDisplay);
        if (!context)
            RELEASE_LOG_INFO(Compositing, "Could not create Pbuffer context: %s.", lastErrorString());
    }

    if (!context)
        WTFLogAlways("Could not create EGL sharing context.");
    return context;
}

GLContextEGL::GLContextEGL(PlatformDisplay& display, EGLContext context, EGLSurface surface, EGLSurfaceType type)
    : GLContext(display)
    , m_context(context)
    , m_surface(surface)
    , m_type(type)
{
    ASSERT(type != PixmapSurface);
    ASSERT(type == Surfaceless || surface != EGL_NO_SURFACE);
    RELEASE_ASSERT(m_display.eglDisplay() != EGL_NO_DISPLAY);
    RELEASE_ASSERT(context != EGL_NO_CONTEXT);
}

GLContextEGL::~GLContextEGL()
{
#if USE(CAIRO)
    if (m_cairoDevice)
        cairo_device_destroy(m_cairoDevice);
#endif

    EGLDisplay display = m_display.eglDisplay();
    if (m_context) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, m_context);
    }

    if (m_surface)
        eglDestroySurface(display, m_surface);

#if PLATFORM(WAYLAND)
    destroyWaylandWindow();
#endif
#if USE(WPE_RENDERER)
    destroyWPETarget();
#endif
}

bool GLContextEGL::canRenderToDefaultFramebuffer()
{
    return m_type == WindowSurface;
}

IntSize GLContextEGL::defaultFrameBufferSize()
{
    if (!canRenderToDefaultFramebuffer())
        return IntSize();

    EGLDisplay display = m_display.eglDisplay();
    EGLint width, height;
    if (!eglQuerySurface(display, m_surface, EGL_WIDTH, &width)
        || !eglQuerySurface(display, m_surface, EGL_HEIGHT, &height))
        return IntSize();

    return IntSize(width, height);
}

EGLContext GLContextEGL::createContextForEGLVersion(PlatformDisplay& platformDisplay, EGLConfig config, EGLContext sharingContext)
{
    static EGLint contextAttributes[7];
    static bool contextAttributesInitialized = false;

    if (!contextAttributesInitialized) {
        contextAttributesInitialized = true;

#if USE(OPENGL_ES)
        // GLES case. Not much to do here besides requesting a GLES2 version.
        contextAttributes[0] = EGL_CONTEXT_CLIENT_VERSION;
        contextAttributes[1] = 2;
        contextAttributes[2] = EGL_NONE;
#else
        // OpenGL case. We want to request an OpenGL version >= 3.2 with a core profile. If that's not possible,
        // we'll use whatever is available. In order to request a concrete version of OpenGL we need EGL version
        // 1.5 or EGL version 1.4 with the extension EGL_KHR_create_context.
        EGLContext context = EGL_NO_CONTEXT;

        if (platformDisplay.eglCheckVersion(1, 5)) {
            contextAttributes[0] = EGL_CONTEXT_MAJOR_VERSION;
            contextAttributes[1] = 3;
            contextAttributes[2] = EGL_CONTEXT_MINOR_VERSION;
            contextAttributes[3] = 2;
            contextAttributes[4] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
            contextAttributes[5] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
            contextAttributes[6] = EGL_NONE;

            // Try to create a context with this configuration.
            context = eglCreateContext(platformDisplay.eglDisplay(), config, sharingContext, contextAttributes);
        } else if (platformDisplay.eglCheckVersion(1, 4)) {
            const char* extensions = eglQueryString(platformDisplay.eglDisplay(), EGL_EXTENSIONS);
            if (GLContext::isExtensionSupported(extensions, "EGL_KHR_create_context")) {
                contextAttributes[0] = EGL_CONTEXT_MAJOR_VERSION_KHR;
                contextAttributes[1] = 3;
                contextAttributes[2] = EGL_CONTEXT_MINOR_VERSION_KHR;
                contextAttributes[3] = 2;
                contextAttributes[4] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
                contextAttributes[5] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
                contextAttributes[6] = EGL_NONE;

                // Try to create a context with this configuration.
                context = eglCreateContext(platformDisplay.eglDisplay(), config, sharingContext, contextAttributes);
            }
        }

        // If the context creation worked, just return it.
        if (context != EGL_NO_CONTEXT)
            return context;

        // Legacy case: the required EGL version is not present, or we haven't been able to create a >= 3.2 OpenGL
        // context, so just request whatever is available.
        contextAttributes[0] = EGL_NONE;
#endif
    }

    return eglCreateContext(platformDisplay.eglDisplay(), config, sharingContext, contextAttributes);
}

bool GLContextEGL::makeContextCurrent()
{
    ASSERT(m_context);

    GLContext::makeContextCurrent();
    if (eglGetCurrentContext() == m_context)
        return true;

    return eglMakeCurrent(m_display.eglDisplay(), m_surface, m_surface, m_context);
}

void GLContextEGL::swapBuffers()
{
    ASSERT(m_surface);
    eglSwapBuffers(m_display.eglDisplay(), m_surface);
}

void GLContextEGL::waitNative()
{
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
}

void GLContextEGL::swapInterval(int interval)
{
    ASSERT(m_surface);
    eglSwapInterval(m_display.eglDisplay(), interval);
}

#if USE(CAIRO)
cairo_device_t* GLContextEGL::cairoDevice()
{
    if (m_cairoDevice)
        return m_cairoDevice;

#if ENABLE(ACCELERATED_2D_CANVAS)
    m_cairoDevice = cairo_egl_device_create(m_display.eglDisplay(), m_context);
#endif

    return m_cairoDevice;
}
#endif

#if ENABLE(GRAPHICS_CONTEXT_GL)
PlatformGraphicsContextGL GLContextEGL::platformContext()
{
    return m_context;
}
#endif

} // namespace WebCore

#endif // USE(EGL)
