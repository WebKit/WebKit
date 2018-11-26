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

#include "GLContext.h"
#include <cstdlib>
#include <mutex>

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#endif

#if PLATFORM(WAYLAND)
#include "PlatformDisplayWayland.h"
#endif

#if PLATFORM(WIN)
#include "PlatformDisplayWin.h"
#endif

#if USE(LIBWPE)
#include "PlatformDisplayLibWPE.h"
#endif

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

#if PLATFORM(GTK) && PLATFORM(X11)
#include <gdk/gdkx.h>
#endif

#if PLATFORM(GTK) && PLATFORM(WAYLAND) && !defined(GTK_API_VERSION_2)
#include <gdk/gdkwayland.h>
#endif

#if USE(EGL)
#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#endif
#include "GLContextEGL.h"
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplay> PlatformDisplay::createPlatformDisplay()
{
#if PLATFORM(GTK)
#if defined(GTK_API_VERSION_2)
    return PlatformDisplayX11::create(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
#else
    if (gtk_init_check(nullptr, nullptr)) {
        GdkDisplay* display = gdk_display_manager_get_default_display(gdk_display_manager_get());
#if PLATFORM(X11)
        if (GDK_IS_X11_DISPLAY(display))
            return PlatformDisplayX11::create(GDK_DISPLAY_XDISPLAY(display));
#endif
#if PLATFORM(WAYLAND)
        if (GDK_IS_WAYLAND_DISPLAY(display))
            return PlatformDisplayWayland::create(gdk_wayland_display_get_wl_display(display));
#endif
    }
#endif
#endif // PLATFORM(GTK)

#if USE(LIBWPE)
    return PlatformDisplayLibWPE::create();
#elif PLATFORM(WIN)
    return PlatformDisplayWin::create();
#endif

#if PLATFORM(WAYLAND)
    if (auto platformDisplay = PlatformDisplayWayland::create())
        return platformDisplay;
#endif

#if PLATFORM(X11)
    if (auto platformDisplay = PlatformDisplayX11::create())
        return platformDisplay;
#endif

    // If at this point we still don't have a display, just create a fake display with no native.
#if PLATFORM(WAYLAND)
    return PlatformDisplayWayland::create(nullptr);
#elif PLATFORM(X11)
    return PlatformDisplayX11::create(nullptr);
#endif

    RELEASE_ASSERT_NOT_REACHED();
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    static std::once_flag onceFlag;
    IGNORE_CLANG_WARNINGS_BEGIN("exit-time-destructors")
    static std::unique_ptr<PlatformDisplay> display;
    IGNORE_CLANG_WARNINGS_END
    std::call_once(onceFlag, []{
        display = createPlatformDisplay();
    });
    return *display;
}

static PlatformDisplay* s_sharedDisplayForCompositing;

PlatformDisplay& PlatformDisplay::sharedDisplayForCompositing()
{
    return s_sharedDisplayForCompositing ? *s_sharedDisplayForCompositing : sharedDisplay();
}

void PlatformDisplay::setSharedDisplayForCompositing(PlatformDisplay& display)
{
    s_sharedDisplayForCompositing = &display;
}

PlatformDisplay::PlatformDisplay(NativeDisplayOwned displayOwned)
    : m_nativeDisplayOwned(displayOwned)
#if USE(EGL)
    , m_eglDisplay(EGL_NO_DISPLAY)
#endif
{
}

PlatformDisplay::~PlatformDisplay()
{
#if USE(EGL)
    ASSERT(m_eglDisplay == EGL_NO_DISPLAY);
#endif
}

#if USE(EGL) || USE(GLX)
GLContext* PlatformDisplay::sharingGLContext()
{
    if (!m_sharingGLContext)
        m_sharingGLContext = GLContext::createSharingContext(*this);
    return m_sharingGLContext.get();
}
#endif

#if USE(EGL)
static HashSet<PlatformDisplay*>& eglDisplays()
{
    static NeverDestroyed<HashSet<PlatformDisplay*>> displays;
    return displays;
}

EGLDisplay PlatformDisplay::eglDisplay() const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();
    return m_eglDisplay;
}

bool PlatformDisplay::eglCheckVersion(int major, int minor) const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();

    return (m_eglMajorVersion > major) || ((m_eglMajorVersion == major) && (m_eglMinorVersion >= minor));
}

void PlatformDisplay::initializeEGLDisplay()
{
    m_eglDisplayInitialized = true;

    if (m_eglDisplay == EGL_NO_DISPLAY) {
        m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (m_eglDisplay == EGL_NO_DISPLAY) {
            WTFLogAlways("Cannot get default EGL display: %s\n", GLContextEGL::lastErrorString());
            return;
        }
    }

    EGLint majorVersion, minorVersion;
    if (eglInitialize(m_eglDisplay, &majorVersion, &minorVersion) == EGL_FALSE) {
        WTFLogAlways("EGLDisplay Initialization failed: %s\n", GLContextEGL::lastErrorString());
        terminateEGLDisplay();
        return;
    }

    m_eglMajorVersion = majorVersion;
    m_eglMinorVersion = minorVersion;

    eglDisplays().add(this);

#if !PLATFORM(WIN)
    static bool eglAtexitHandlerInitialized = false;
    if (!eglAtexitHandlerInitialized) {
        // EGL registers atexit handlers to cleanup its global display list.
        // Since the global PlatformDisplay instance is created before,
        // when the PlatformDisplay destructor is called, EGL has already removed the
        // display from the list, causing eglTerminate() to crash. So, here we register
        // our own atexit handler, after EGL has been initialized and after the global
        // instance has been created to ensure we call eglTerminate() before the other
        // EGL atexit handlers and the PlatformDisplay destructor.
        // See https://bugs.webkit.org/show_bug.cgi?id=157973.
        eglAtexitHandlerInitialized = true;
        std::atexit(shutDownEglDisplays);
    }
#endif
}

void PlatformDisplay::terminateEGLDisplay()
{
    m_sharingGLContext = nullptr;
    ASSERT(m_eglDisplayInitialized);
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;
    eglTerminate(m_eglDisplay);
    m_eglDisplay = EGL_NO_DISPLAY;
}

void PlatformDisplay::shutDownEglDisplays()
{
    while (!eglDisplays().isEmpty()) {
        auto* display = eglDisplays().takeAny();
        display->terminateEGLDisplay();
    }
}

#endif // USE(EGL)

} // namespace WebCore
