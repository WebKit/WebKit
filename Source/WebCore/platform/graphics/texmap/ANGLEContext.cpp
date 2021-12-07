/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
 * Copyright (C) 2009-2020 Apple Inc.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "ANGLEContext.h"

#if USE(TEXTURE_MAPPER) && USE(ANGLE) && !USE(NICOSIA)

#include "ANGLEHeaders.h"
#include "Logging.h"
#include <wtf/Vector.h>

namespace WebCore {

const char* ANGLEContext::errorString(int statusCode)
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

const char* ANGLEContext::lastErrorString()
{
    return errorString(EGL_GetError());
}

std::unique_ptr<ANGLEContext> ANGLEContext::createContext(EGLContext sharingContext, bool isForWebGL2)
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

    Vector<EGLint> contextAttributes;
    if (isForWebGL2) {
        contextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
        contextAttributes.append(3);
    } else {
        contextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
        contextAttributes.append(2);
        // ANGLE will upgrade the context to ES3 automatically unless this is specified.
        contextAttributes.append(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
        contextAttributes.append(EGL_FALSE);
    }
    contextAttributes.append(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    contextAttributes.append(EGL_TRUE);
    // WebGL requires that all resources are cleared at creation.
    contextAttributes.append(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    contextAttributes.append(EGL_TRUE);
    // WebGL doesn't allow client arrays.
    contextAttributes.append(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    contextAttributes.append(EGL_FALSE);
    // WebGL doesn't allow implicit creation of objects on bind.
    contextAttributes.append(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    contextAttributes.append(EGL_FALSE);

    if (strstr(displayExtensions, "EGL_ANGLE_power_preference")) {
        contextAttributes.append(EGL_POWER_PREFERENCE_ANGLE);
        // EGL_LOW_POWER_ANGLE is the default. Change to
        // EGL_HIGH_POWER_ANGLE if desired.
        contextAttributes.append(EGL_LOW_POWER_ANGLE);
    }
    contextAttributes.append(EGL_NONE);

    EGLContext context = EGL_CreateContext(display, config, sharingContext, contextAttributes.data());
    if (context == EGL_NO_CONTEXT) {
        LOG(WebGL, "EGLContext Initialization failed.");
        return nullptr;
    }
    LOG(WebGL, "Got EGLContext");

    return std::unique_ptr<ANGLEContext>(new ANGLEContext(display,  config, context, EGL_NO_SURFACE));
}

ANGLEContext::ANGLEContext(EGLDisplay display, EGLConfig config, EGLContext context, EGLSurface surface)
    : m_display(display)
    , m_config(config)
    , m_context(context)
    , m_surface(surface)
{
}

ANGLEContext::~ANGLEContext()
{
    if (m_context) {
        GL_BindFramebuffer(GL_FRAMEBUFFER, 0);
        EGL_MakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        EGL_DestroyContext(m_display, m_context);
    }

    if (m_surface)
        EGL_DestroySurface(m_display, m_surface);
}

bool ANGLEContext::makeContextCurrent()
{
    ASSERT(m_context);

    if (EGL_GetCurrentContext() != m_context)
        return EGL_MakeCurrent(m_display, m_surface, m_surface, m_context);
    return true;
}

EGLContext ANGLEContext::platformContext() const
{
    return m_context;
}

EGLDisplay ANGLEContext::platformDisplay() const
{
    return m_display;
}

EGLConfig ANGLEContext::platformConfig() const
{
    return m_config;
}

} // namespace WebCore

#endif
