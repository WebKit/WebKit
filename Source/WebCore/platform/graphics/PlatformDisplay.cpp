/*
 * Copyright (C) 2015 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformDisplay.h"

#include <mutex>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#endif

#if PLATFORM(WAYLAND)
#include "PlatformDisplayWayland.h"
#endif

#if PLATFORM(WIN)
#include "PlatformDisplayWin.h"
#endif

#if PLATFORM(GTK)
#if PLATFORM(X11)
#include <gdk/gdkx.h>
#endif
#if PLATFORM(WAYLAND) && !defined(GTK_API_VERSION_2)
#include <gdk/gdkwayland.h>
#endif
#endif // PLATFORM(GTK)

#if PLATFORM(EFL) && defined(HAVE_ECORE_X)
#include <Ecore_X.h>
#endif

#if USE(EGL)
#include <EGL/egl.h>
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplay> PlatformDisplay::createPlatformDisplay()
{
#if PLATFORM(GTK)
#if defined(GTK_API_VERSION_2)
    return std::make_unique<PlatformDisplayX11>(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
#else
    GdkDisplay* display = gdk_display_manager_get_default_display(gdk_display_manager_get());
#if PLATFORM(X11)
    if (GDK_IS_X11_DISPLAY(display))
        return std::make_unique<PlatformDisplayX11>(GDK_DISPLAY_XDISPLAY(display));
#endif
#if PLATFORM(WAYLAND)
    if (GDK_IS_WAYLAND_DISPLAY(display))
        return PlatformDisplayWayland::create();
#endif
#endif
#elif PLATFORM(EFL) && defined(HAVE_ECORE_X)
    return std::make_unique<PlatformDisplayX11>(static_cast<Display*>(ecore_x_display_get()));
#elif PLATFORM(WIN)
    return std::make_unique<PlatformDisplayWin>();
#endif

#if PLATFORM(X11)
    return std::make_unique<PlatformDisplayX11>();
#endif

    ASSERT_NOT_REACHED();
    return nullptr;
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    static std::once_flag onceFlag;
    static std::unique_ptr<PlatformDisplay> display;
    std::call_once(onceFlag, []{
        display = createPlatformDisplay();
    });
    return *display;
}

PlatformDisplay::PlatformDisplay()
#if USE(EGL)
    : m_eglDisplay(EGL_NO_DISPLAY)
#endif
{
}

PlatformDisplay::~PlatformDisplay()
{
    // WinCairo crashes when terminating EGL on exit.
    // https://bugs.webkit.org/show_bug.cgi?id=145832
#if USE(EGL) && !PLATFORM(WIN)
    terminateEGLDisplay();
#endif
}

#if USE(EGL)
EGLDisplay PlatformDisplay::eglDisplay() const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();
    return m_eglDisplay;
}

void PlatformDisplay::initializeEGLDisplay()
{
    m_eglDisplayInitialized = true;

    if (m_eglDisplay == EGL_NO_DISPLAY) {
        m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (m_eglDisplay == EGL_NO_DISPLAY)
            return;
    }

    if (eglInitialize(m_eglDisplay, 0, 0) == EGL_FALSE) {
        LOG_ERROR("EGLDisplay Initialization failed.");
        terminateEGLDisplay();
        return;
    }

#if USE(OPENGL_ES_2)
    static const EGLenum eglAPIVersion = EGL_OPENGL_ES_API;
#else
    static const EGLenum eglAPIVersion = EGL_OPENGL_API;
#endif
    if (eglBindAPI(eglAPIVersion) == EGL_FALSE) {
        LOG_ERROR("Failed to set EGL API(%d).", eglGetError());
        terminateEGLDisplay();
        return;
    }
}

void PlatformDisplay::terminateEGLDisplay()
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;
    eglTerminate(m_eglDisplay);
    m_eglDisplay = EGL_NO_DISPLAY;
}
#endif // USE(EGL)

} // namespace WebCore
