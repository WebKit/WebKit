/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EGLConfigHelper.h"

#if USE(EGL)

namespace WebCore {

SharedEGLDisplay* SharedEGLDisplay::m_staticSharedEGLDisplay = 0;

static EGLint configAttributeList[] = {
#if USE(OPENGL_ES_2)
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_STENCIL_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_NONE,
    EGL_NONE
};

void SharedEGLDisplay::deref()
{
    if (derefBase()) {
        m_staticSharedEGLDisplay = 0;
        delete this;
    }
}

EGLDisplay SharedEGLDisplay::sharedEGLDisplay()
{
    return m_eglDisplay;
}

SharedEGLDisplay::SharedEGLDisplay(NativeSharedDisplay* display)
    : m_eglDisplay(EGL_NO_DISPLAY)
{
    if (display)
        m_eglDisplay = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display));
    else
        m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (m_eglDisplay == EGL_NO_DISPLAY) {
        LOG_ERROR("EGLDisplay Initialization failed.");
        return;
    }

    EGLBoolean success;
    success = eglInitialize(m_eglDisplay, 0, 0);

    if (success != EGL_TRUE) {
        LOG_ERROR("EGLInitialization failed.");
        m_eglDisplay = EGL_NO_DISPLAY;
    }

    success = eglBindAPI(eglAPIVersion);

    if (success != EGL_TRUE) {
        LOG_ERROR("Failed to set EGL API(%d).", eglGetError());
        m_eglDisplay = EGL_NO_DISPLAY;
    }
}

void SharedEGLDisplay::cleanup()
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;

    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(m_eglDisplay);
    m_eglDisplay = EGL_NO_DISPLAY;
}

SharedEGLDisplay::~SharedEGLDisplay()
{
    cleanup();
}

EGLConfigHelper::EGLConfigHelper(NativeSharedDisplay* display)
    : m_pbufferFBConfig(0)
    , m_surfaceContextFBConfig(0)
{
    m_sharedDisplay = SharedEGLDisplay::create(display);
}

EGLConfigHelper::~EGLConfigHelper()
{
}

PlatformDisplay EGLConfigHelper::display()
{
    return m_sharedDisplay->sharedEGLDisplay();
}

EGLConfig EGLConfigHelper::pBufferContextConfig()
{
    if (!m_pbufferFBConfig) {
        configAttributeList[13] = EGL_PIXMAP_BIT;
        m_pbufferFBConfig = createConfig(configAttributeList);
    }

    return m_pbufferFBConfig;
}

EGLConfig EGLConfigHelper::surfaceContextConfig()
{
    if (!m_surfaceContextFBConfig) {
        configAttributeList[13] = EGL_WINDOW_BIT;
        m_surfaceContextFBConfig = createConfig(configAttributeList);
    }

    return m_surfaceContextFBConfig;
}

EGLint EGLConfigHelper::nativeVisualId(const EGLConfig& config)
{
    if (display() == EGL_NO_DISPLAY)
        return -1;

    EGLint eglValue = 0;
    eglGetConfigAttrib(display(), config, EGL_NATIVE_VISUAL_ID, &eglValue);

    return eglValue;
}

void EGLConfigHelper::reset()
{
    m_surfaceContextFBConfig = 0;
    m_pbufferFBConfig = 0;
}

EGLConfig EGLConfigHelper::createConfig(const int attributes[])
{
    if (display() == EGL_NO_DISPLAY)
        return 0;

    int numConfigs;
    EGLConfig config = 0;

    if (!eglChooseConfig(display(), attributes, &config, 1, &numConfigs))
        LOG_ERROR("Failed to get supported configirations.");

    if (numConfigs != 1) {
        LOG_ERROR("EGLChooseConfig didn't return exactly one config but (%d).", numConfigs);
        config = 0;
    }

    return config;
}

}

#endif
