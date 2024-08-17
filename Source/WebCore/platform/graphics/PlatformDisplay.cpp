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
#include <wtf/HashSet.h>
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

#if USE(WPE_RENDERER)
#include "PlatformDisplayLibWPE.h"
#endif

#if PLATFORM(WPE)
#include "PlatformDisplayGBM.h"
#include "PlatformDisplaySurfaceless.h"
#endif

#if PLATFORM(GTK)
#include "GtkVersioning.h"
#endif

#if PLATFORM(GTK) && PLATFORM(X11)
#if USE(GTK4)
#include <gdk/x11/gdkx.h>
#else
#include <gdk/gdkx.h>
#endif
#if defined(None)
#undef None
#endif
#endif

#if PLATFORM(GTK) && PLATFORM(WAYLAND)
#if USE(GTK4)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>
#endif
#endif

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if USE(GBM)
#include <drm_fourcc.h>
#endif

#if USE(ATSPI)
#include <wtf/glib/GUniquePtr.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplay> PlatformDisplay::createPlatformDisplay()
{
#if PLATFORM(GTK)
    if (gtk_init_check(nullptr, nullptr)) {
        GdkDisplay* display = gdk_display_manager_get_default_display(gdk_display_manager_get());
#if PLATFORM(X11)
        if (GDK_IS_X11_DISPLAY(display))
            return PlatformDisplayX11::create(display);
#endif

#if PLATFORM(WAYLAND)
        if (GDK_IS_WAYLAND_DISPLAY(display))
            return PlatformDisplayWayland::create(display);
#endif
    }
#endif // PLATFORM(GTK)

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

#if USE(WPE_RENDERER)
    return PlatformDisplayLibWPE::create();
#endif

    RELEASE_ASSERT_NOT_REACHED();
}

#if PLATFORM(WIN)
PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    // ANGLE D3D renderer isn't thread-safe. Don't destruct it on non-main threads which calls _exit().
    static PlatformDisplay* display = PlatformDisplayWin::create().release();
    return *display;
}
#else
IGNORE_CLANG_WARNINGS_BEGIN("exit-time-destructors")
static std::unique_ptr<PlatformDisplay> s_sharedDisplay;
IGNORE_CLANG_WARNINGS_END

void PlatformDisplay::setSharedDisplay(std::unique_ptr<PlatformDisplay>&& display)
{
    RELEASE_ASSERT(!s_sharedDisplay);
    s_sharedDisplay = WTFMove(display);
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        if (!s_sharedDisplay)
            setSharedDisplay(createPlatformDisplay());
    });
    return *s_sharedDisplay;
}
#endif

PlatformDisplay::PlatformDisplay() = default;

#if PLATFORM(GTK)
PlatformDisplay::PlatformDisplay(GdkDisplay* display)
    : m_sharedDisplay(display)
{
    if (m_sharedDisplay) {
        g_signal_connect(m_sharedDisplay.get(), "closed", G_CALLBACK(+[](GdkDisplay*, gboolean, gpointer userData) {
            auto& platformDisplay = *static_cast<PlatformDisplay*>(userData);
            platformDisplay.sharedDisplayDidClose();
        }), this);
    }
}

void PlatformDisplay::sharedDisplayDidClose()
{
    terminateEGLDisplay();
}
#endif // PLATFORM(GTK)

static HashSet<PlatformDisplay*>& eglDisplays()
{
    static NeverDestroyed<HashSet<PlatformDisplay*>> displays;
    return displays;
}

PlatformDisplay::~PlatformDisplay()
{
    if (m_eglDisplay && eglDisplays().remove(this))
        terminateEGLDisplay();

#if PLATFORM(GTK)
    if (m_sharedDisplay)
        g_signal_handlers_disconnect_by_data(m_sharedDisplay.get(), this);
#endif
}

GLContext* PlatformDisplay::sharingGLContext()
{
    if (!m_sharingGLContext)
        m_sharingGLContext = GLContext::createSharing(*this);
    return m_sharingGLContext.get();
}

void PlatformDisplay::clearSharingGLContext()
{
#if USE(SKIA)
    invalidateSkiaGLContexts();
#endif
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    m_gstGLContext = nullptr;
#endif
#if ENABLE(WEBGL) && !PLATFORM(WIN)
    clearANGLESharingGLContext();
#endif
    m_sharingGLContext = nullptr;
}

EGLDisplay PlatformDisplay::eglDisplay() const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();
    return m_eglDisplay ? m_eglDisplay->eglDisplay() : EGL_NO_DISPLAY;
}

bool PlatformDisplay::eglCheckVersion(int major, int minor) const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();
    return m_eglDisplay ? m_eglDisplay->checkVersion(major, minor) : false;
}

const GLDisplay::Extensions& PlatformDisplay::eglExtensions() const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();
    static GLDisplay::Extensions empty;
    return m_eglDisplay ? m_eglDisplay->extensions() : empty;
}

void PlatformDisplay::initializeEGLDisplay()
{
    m_eglDisplayInitialized = true;

    if (!m_eglDisplay) {
        m_eglDisplay = GLDisplay::create(eglGetDisplay(EGL_DEFAULT_DISPLAY));
        if (!m_eglDisplay) {
            WTFLogAlways("Cannot get default EGL display: %s\n", GLContext::lastErrorString());
            return;
        }
    }

    if (!m_eglDisplayOwned)
        return;

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
        std::atexit([] {
            while (!eglDisplays().isEmpty()) {
                auto* display = eglDisplays().takeAny();
                display->terminateEGLDisplay();
            }
        });
    }
#endif
}

void PlatformDisplay::terminateEGLDisplay()
{
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    m_gstGLDisplay = nullptr;
#endif
    clearSharingGLContext();
    ASSERT(m_eglDisplayInitialized);
    if (!m_eglDisplay)
        return;

    if (m_eglDisplayOwned)
        m_eglDisplay->terminate();
    m_eglDisplay = nullptr;
}

EGLImage PlatformDisplay::createEGLImage(EGLContext context, EGLenum target, EGLClientBuffer clientBuffer, const Vector<EGLAttrib>& attributes) const
{
    return m_eglDisplay ? m_eglDisplay->createImage(context, target, clientBuffer, attributes) : EGL_NO_IMAGE_KHR;
}

bool PlatformDisplay::destroyEGLImage(EGLImage image) const
{
    return m_eglDisplay ? m_eglDisplay->destroyImage(image) : false;
}

#if USE(GBM)
const Vector<GLDisplay::DMABufFormat>& PlatformDisplay::dmabufFormats()
{
    static Vector<GLDisplay::DMABufFormat> empty;
    return m_eglDisplay ? m_eglDisplay->dmabufFormats() : empty;
}
#endif // USE(GBM)

#if USE(ATSPI)
String PlatformDisplay::accessibilityBusAddress() const
{
#if USE(GTK4)
    if (m_sharedDisplay) {
        if (const char* atspiBusAddress = static_cast<const char*>(g_object_get_data(G_OBJECT(m_sharedDisplay.get()), "-gtk-atspi-bus-address")))
            return String::fromUTF8(atspiBusAddress);
    }
#endif

    return { };
}
#endif

} // namespace WebCore
