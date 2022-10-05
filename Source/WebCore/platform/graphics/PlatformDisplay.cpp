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

#if USE(WPE_RENDERER)
#include "PlatformDisplayLibWPE.h"
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
#elif PLATFORM(WIN)
    return PlatformDisplayWin::create();
#endif

    RELEASE_ASSERT_NOT_REACHED();
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
#if PLATFORM(WIN)
    // ANGLE D3D renderer isn't thread-safe. Don't destruct it on non-main threads which calls _exit().
    static PlatformDisplay* display = createPlatformDisplay().release();
    return *display;
#else
    static std::once_flag onceFlag;
    IGNORE_CLANG_WARNINGS_BEGIN("exit-time-destructors")
    static std::unique_ptr<PlatformDisplay> display;
    IGNORE_CLANG_WARNINGS_END
    std::call_once(onceFlag, []{
        display = createPlatformDisplay();
    });
    return *display;
#endif
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

PlatformDisplay::PlatformDisplay()
#if USE(EGL)
    : m_eglDisplay(EGL_NO_DISPLAY)
#endif
{
}

#if PLATFORM(GTK)
PlatformDisplay::PlatformDisplay(GdkDisplay* display)
    : m_sharedDisplay(display)
#if USE(EGL)
    , m_eglDisplay(EGL_NO_DISPLAY)
#endif
{
    if (m_sharedDisplay) {
#if USE(ATSPI) && USE(GTK4)
        if (const char* atspiBusAddress = static_cast<const char*>(g_object_get_data(G_OBJECT(m_sharedDisplay.get()), "-gtk-atspi-bus-address")))
            m_accessibilityBusAddress = String::fromUTF8(atspiBusAddress);
#endif

        g_signal_connect(m_sharedDisplay.get(), "closed", G_CALLBACK(+[](GdkDisplay*, gboolean, gpointer userData) {
            auto& platformDisplay = *static_cast<PlatformDisplay*>(userData);
            platformDisplay.sharedDisplayDidClose();
        }), this);
    }
}

void PlatformDisplay::sharedDisplayDidClose()
{
#if USE(EGL) || USE(GLX)
    clearSharingGLContext();
#endif
}
#endif

PlatformDisplay::~PlatformDisplay()
{
#if USE(EGL) && !PLATFORM(WIN)
    ASSERT(m_eglDisplay == EGL_NO_DISPLAY);
#endif
#if PLATFORM(GTK)
    if (m_sharedDisplay)
        g_signal_handlers_disconnect_by_data(m_sharedDisplay.get(), this);
#endif
    if (s_sharedDisplayForCompositing == this)
        s_sharedDisplayForCompositing = nullptr;
}

#if USE(EGL) || USE(GLX)
GLContext* PlatformDisplay::sharingGLContext()
{
    if (!m_sharingGLContext)
        m_sharingGLContext = GLContext::createSharingContext(*this);
    return m_sharingGLContext.get();
}

void PlatformDisplay::clearSharingGLContext()
{
    m_sharingGLContext = nullptr;
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

    {
        const char* extensionsString = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);
        auto displayExtensions = StringView::fromLatin1(extensionsString).split(' ');
        auto findExtension =
            [&](auto extensionName) {
                return std::any_of(displayExtensions.begin(), displayExtensions.end(),
                    [&](auto extensionEntry) {
                        return extensionEntry == extensionName;
                    });
            };

        m_eglExtensions.KHR_image_base = findExtension("EGL_KHR_image_base"_s);
        m_eglExtensions.EXT_image_dma_buf_import = findExtension("EGL_EXT_image_dma_buf_import"_s);
        m_eglExtensions.EXT_image_dma_buf_import_modifiers = findExtension("EGL_EXT_image_dma_buf_import_modifiers"_s);
    }

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
    m_gstGLContext = nullptr;
#endif
    clearSharingGLContext();
    ASSERT(m_eglDisplayInitialized);
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(m_eglDisplay);
    m_eglDisplay = EGL_NO_DISPLAY;
}

#endif // USE(EGL)

#if USE(LCMS)
cmsHPROFILE PlatformDisplay::colorProfile() const
{
    if (!m_iccProfile)
        m_iccProfile = LCMSProfilePtr(cmsCreate_sRGBProfile());
    return m_iccProfile.get();
}
#endif

#if USE(ATSPI)
const String& PlatformDisplay::accessibilityBusAddress() const
{
    if (m_accessibilityBusAddress)
        return m_accessibilityBusAddress.value();

    const char* address = g_getenv("AT_SPI_BUS_ADDRESS");
    if (address && *address) {
        m_accessibilityBusAddress = String::fromUTF8(address);
        return m_accessibilityBusAddress.value();
    }

    auto platformAddress = platformAccessibilityBusAddress();
    if (!platformAddress.isEmpty()) {
        m_accessibilityBusAddress = platformAddress;
        return m_accessibilityBusAddress.value();
    }

    GRefPtr<GDBusConnection> sessionBus = adoptGRef(g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr));
    if (sessionBus.get()) {
        GRefPtr<GDBusMessage> message = adoptGRef(g_dbus_message_new_method_call("org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress"));
        g_dbus_message_set_body(message.get(), g_variant_new("()"));
        GRefPtr<GDBusMessage> reply = adoptGRef(g_dbus_connection_send_message_with_reply_sync(sessionBus.get(), message.get(),
            G_DBUS_SEND_MESSAGE_FLAGS_NONE, 30000, nullptr, nullptr, nullptr));
        if (reply) {
            GUniqueOutPtr<GError> error;
            if (g_dbus_message_to_gerror(reply.get(), &error.outPtr())) {
                if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
                    WTFLogAlways("Can't find a11y bus: %s", error->message);
            } else {
                GUniqueOutPtr<char> a11yAddress;
                g_variant_get(g_dbus_message_get_body(reply.get()), "(s)", &a11yAddress.outPtr());
                m_accessibilityBusAddress = String::fromUTF8(a11yAddress.get());
                return m_accessibilityBusAddress.value();
            }
        }
    }

    WTFLogAlways("Could not determine the accessibility bus address");
    m_accessibilityBusAddress = String();
    return m_accessibilityBusAddress.value();
}
#endif

} // namespace WebCore
