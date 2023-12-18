/*
 * Copyright (C) 2023 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PlatformDisplay.h"

#if ENABLE(WEBGL)
#include "ANGLEHeaders.h"
#include "GLContext.h"
#include "Logging.h"

namespace WebCore {

EGLDisplay PlatformDisplay::angleEGLDisplay() const
{
#if PLATFORM(WIN)
    return eglDisplay();
#else
    if (m_angleEGLDisplay != EGL_NO_DISPLAY)
        return m_angleEGLDisplay;

    auto display = eglDisplay();
    if (display == EGL_NO_DISPLAY)
        return EGL_NO_DISPLAY;

    if (!m_anglePlatform)
        return EGL_NO_DISPLAY;

    Vector<EGLint> displayAttributes {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, m_anglePlatform.value(),
        EGL_NONE,
    };

    auto angleDisplay = EGL_GetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, m_angleNativeDisplay ? m_angleNativeDisplay : EGL_DEFAULT_DISPLAY, displayAttributes.data());
    if (angleDisplay == EGL_NO_DISPLAY)
        return EGL_NO_DISPLAY;

    EGLint majorVersion, minorVersion;
    if (EGL_Initialize(angleDisplay, &majorVersion, &minorVersion) == EGL_FALSE) {
        LOG(WebGL, "EGLDisplay Initialization failed.");
        return EGL_NO_DISPLAY;
    }
    LOG(WebGL, "ANGLE initialised Major: %d Minor: %d", majorVersion, minorVersion);

    m_angleEGLDisplay = angleDisplay;
    return m_angleEGLDisplay;
#endif
}

EGLContext PlatformDisplay::angleSharingGLContext()
{
#if PLATFORM(WIN)
    return sharingGLContext()->platformContext();
#else
    if (m_angleSharingGLContext != EGL_NO_CONTEXT)
        return m_angleSharingGLContext;

    ASSERT(m_angleEGLDisplay != EGL_NO_DISPLAY);
    auto sharingContext = sharingGLContext();
    if (!sharingContext)
        return EGL_NO_CONTEXT;

    EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, type() == Type::Surfaceless ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numberConfigsReturned = 0;
    EGL_ChooseConfig(m_angleEGLDisplay, configAttributes, &config, 1, &numberConfigsReturned);
    if (numberConfigsReturned != 1)
        return EGL_NO_CONTEXT;

    GLContext::ScopedGLContextCurrent scopedCurrent(*sharingContext);
    EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_EXTERNAL_CONTEXT_ANGLE, EGL_TRUE,
        EGL_NONE
    };
    m_angleSharingGLContext = EGL_CreateContext(m_angleEGLDisplay, config, EGL_NO_CONTEXT, contextAttributes);
    return m_angleSharingGLContext;
#endif
}

#if ENABLE(WEBGL) && !PLATFORM(WIN)
void PlatformDisplay::clearANGLESharingGLContext()
{
    if (m_angleSharingGLContext == EGL_NO_CONTEXT)
        return;

    ASSERT(m_angleEGLDisplay);
    ASSERT(m_sharingGLContext);
    GLContext::ScopedGLContextCurrent scopedCurrent(*m_sharingGLContext);
    EGL_DestroyContext(m_angleEGLDisplay, m_angleSharingGLContext);
    m_angleSharingGLContext = EGL_NO_CONTEXT;
}
#endif


} // namespace WebCore

#endif // ENABLE(WEBGL)
