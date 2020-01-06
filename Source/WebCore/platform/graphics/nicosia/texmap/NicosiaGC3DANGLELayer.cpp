/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaGC3DANGLELayer.h"

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#include "Logging.h"

#define EGL_EGL_PROTOTYPES 0
// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#define GL_ANGLE_explicit_context_gles1
typedef void* GLeglContext;
#include <ANGLE/egl.h>
#include <ANGLE/eglext.h>
#include <ANGLE/eglext_angle.h>
#include <ANGLE/entry_points_egl.h>
#include <ANGLE/entry_points_gles_2_0_autogen.h>
#include <ANGLE/entry_points_gles_ext_autogen.h>
#include <ANGLE/gl2ext.h>
#include <ANGLE/gl2ext_angle.h>

namespace Nicosia {

using namespace WebCore;

const char* GC3DANGLELayer::ANGLEContext::errorString(int statusCode)
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

const char* GC3DANGLELayer::ANGLEContext::lastErrorString()
{
    return errorString(EGL_GetError());
}

std::unique_ptr<GC3DANGLELayer::ANGLEContext> GC3DANGLELayer::ANGLEContext::createContext()
{
    EGLDisplay display = EGL_GetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY)
        return nullptr;

    EGLint majorVersion, minorVersion;
    if (EGL_Initialize(display, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);

    const char* displayExtensions = EGL_QueryString(display, EGL_EXTENSIONS);
    LOG(WebGL, "Extensions: %s", displayExtensions);

    EGLConfig config;
    EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint numberConfigsReturned = 0;
    EGL_ChooseConfig(display, configAttributes, &config, 1, &numberConfigsReturned);
    if (numberConfigsReturned != 1) {
        LOG(WebGL, "EGLConfig Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "Got EGLConfig");

    EGL_BindAPI(EGL_OPENGL_ES_API);
    if (EGL_GetError() != EGL_SUCCESS) {
        LOG(WebGL, "Unable to bind to OPENGL_ES_API");
        return nullptr;
    }

    std::vector<EGLint> contextAttributes;
    contextAttributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
    contextAttributes.push_back(2);
    contextAttributes.push_back(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    contextAttributes.push_back(EGL_TRUE);
    contextAttributes.push_back(EGL_EXTENSIONS_ENABLED_ANGLE);
    contextAttributes.push_back(EGL_TRUE);
    if (strstr(displayExtensions, "EGL_ANGLE_power_preference")) {
        contextAttributes.push_back(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        contextAttributes.push_back(EGL_LOW_POWER_ANGLE);
    }
    contextAttributes.push_back(EGL_NONE);

    EGLContext context = EGL_CreateContext(display, config, EGL_NO_CONTEXT, contextAttributes.data());
    if (context == EGL_NO_CONTEXT) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "Got EGLContext");

    return std::unique_ptr<ANGLEContext>(new ANGLEContext(display, context, EGL_NO_SURFACE));
}

GC3DANGLELayer::ANGLEContext::ANGLEContext(EGLDisplay display, EGLContext context, EGLSurface surface)
    : m_display(display)
    , m_context(context)
    , m_surface(surface)
{
}

GC3DANGLELayer::ANGLEContext::~ANGLEContext()
{
    if (m_context) {
        gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        EGL_MakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_display, m_context);
    }

    if (m_surface)
        EGL_DestroySurface(m_display, m_surface);
}

bool GC3DANGLELayer::ANGLEContext::makeContextCurrent()
{
    ASSERT(m_context);

    if (EGL_GetCurrentContext() != m_context)
        return EGL_MakeCurrent(m_display, m_surface, m_surface, m_context);
    return true;
}

PlatformGraphicsContextGL GC3DANGLELayer::ANGLEContext::platformContext() const
{
    return m_context;
}

GC3DANGLELayer::GC3DANGLELayer(GraphicsContextGLOpenGL& context, GraphicsContextGLOpenGL::Destination destination)
    : GC3DLayer(context)
{
    switch (destination) {
    case GraphicsContextGLOpenGL::Destination::Offscreen:
        m_angleContext = ANGLEContext::createContext();
        break;
    case GraphicsContextGLOpenGL::Destination::DirectlyToHostWindow:
        ASSERT_NOT_REACHED();
        break;
    }
}

GC3DANGLELayer::~GC3DANGLELayer()
{
}

bool GC3DANGLELayer::makeContextCurrent()
{
    ASSERT(m_angleContext);
    return m_angleContext->makeContextCurrent();

}

PlatformGraphicsContextGL GC3DANGLELayer::platformContext() const
{
    ASSERT(m_angleContext);
    return m_angleContext->platformContext();
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
