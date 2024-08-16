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

#if !USE(LIBEPOXY)
typedef EGLImage (EGLAPIENTRYP PFNEGLCREATEIMAGEPROC) (EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLAttrib*);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEPROC) (EGLDisplay, EGLImage);
#ifndef EGL_KHR_image_base
#define EGL_KHR_image_base 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay, EGLImage);
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay, EGLContext, EGLenum target, EGLClientBuffer, const EGLint* attribList);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay, EGLImageKHR);
#endif
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

PlatformDisplay::PlatformDisplay()
    : m_eglDisplay(EGL_NO_DISPLAY)
{
}

#if PLATFORM(GTK)
PlatformDisplay::PlatformDisplay(GdkDisplay* display)
    : m_sharedDisplay(display)
    , m_eglDisplay(EGL_NO_DISPLAY)
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
    if (m_eglDisplay != EGL_NO_DISPLAY && eglDisplays().remove(this))
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
    return m_eglDisplay;
}

bool PlatformDisplay::eglCheckVersion(int major, int minor) const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();

    return (m_eglMajorVersion > major) || ((m_eglMajorVersion == major) && (m_eglMinorVersion >= minor));
}

const PlatformDisplay::EGLExtensions& PlatformDisplay::eglExtensions() const
{
    if (!m_eglDisplayInitialized)
        const_cast<PlatformDisplay*>(this)->initializeEGLDisplay();
    return m_eglExtensions;
}

void PlatformDisplay::initializeEGLDisplay()
{
    m_eglDisplayInitialized = true;

    if (m_eglDisplay == EGL_NO_DISPLAY) {
        m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (m_eglDisplay == EGL_NO_DISPLAY) {
            WTFLogAlways("Cannot get default EGL display: %s\n", GLContext::lastErrorString());
            return;
        }
    }

    EGLint majorVersion, minorVersion;
    if (eglInitialize(m_eglDisplay, &majorVersion, &minorVersion) == EGL_FALSE) {
        WTFLogAlways("EGLDisplay Initialization failed: %s\n", GLContext::lastErrorString());
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
        m_eglExtensions.KHR_surfaceless_context = findExtension("EGL_KHR_surfaceless_context"_s);
        m_eglExtensions.KHR_fence_sync = findExtension("EGL_KHR_fence_sync"_s);
        m_eglExtensions.KHR_wait_sync = findExtension("EGL_KHR_wait_sync"_s);
        m_eglExtensions.ANDROID_native_fence_sync = findExtension("EGL_ANDROID_native_fence_sync"_s);
        m_eglExtensions.EXT_image_dma_buf_import = findExtension("EGL_EXT_image_dma_buf_import"_s);
        m_eglExtensions.EXT_image_dma_buf_import_modifiers = findExtension("EGL_EXT_image_dma_buf_import_modifiers"_s);
        m_eglExtensions.MESA_image_dma_buf_export = findExtension("EGL_MESA_image_dma_buf_export"_s);
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
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;

    if (m_eglDisplayOwned) {
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(m_eglDisplay);
    }
    m_eglDisplay = EGL_NO_DISPLAY;
}

EGLImage PlatformDisplay::createEGLImage(EGLContext context, EGLenum target, EGLClientBuffer clientBuffer, const Vector<EGLAttrib>& attributes) const
{
    if (eglCheckVersion(1, 5)) {
        static PFNEGLCREATEIMAGEPROC s_eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEPROC>(eglGetProcAddress("eglCreateImage"));
        if (s_eglCreateImage)
            return s_eglCreateImage(m_eglDisplay, context, target, clientBuffer, attributes.isEmpty() ? nullptr : attributes.data());
        return EGL_NO_IMAGE;
    }

    if (!m_eglExtensions.KHR_image_base)
        return EGL_NO_IMAGE;

    Vector<EGLint> intAttributes = attributes.map<Vector<EGLint>>([] (EGLAttrib value) {
        return value;
    });
    static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    if (s_eglCreateImageKHR)
        return s_eglCreateImageKHR(m_eglDisplay, context, target, clientBuffer, intAttributes.isEmpty() ? nullptr : intAttributes.data());
    return EGL_NO_IMAGE_KHR;
}

bool PlatformDisplay::destroyEGLImage(EGLImage image) const
{
    if (eglCheckVersion(1, 5)) {
        static PFNEGLDESTROYIMAGEPROC s_eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEPROC>(eglGetProcAddress("eglDestroyImage"));
        if (s_eglDestroyImage)
            return s_eglDestroyImage(m_eglDisplay, image);
        return false;
    }

    if (!m_eglExtensions.KHR_image_base)
        return false;

    static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (s_eglDestroyImageKHR)
        return s_eglDestroyImageKHR(m_eglDisplay, image);
    return false;
}

#if USE(GBM)
const Vector<PlatformDisplay::DMABufFormat>& PlatformDisplay::dmabufFormats()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [this] {
        const auto& extensions = eglExtensions();
        if (!extensions.EXT_image_dma_buf_import)
            return;

        static PFNEGLQUERYDMABUFFORMATSEXTPROC s_eglQueryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
        if (!s_eglQueryDmaBufFormatsEXT)
            return;

        EGLint formatsCount;
        if (!s_eglQueryDmaBufFormatsEXT(m_eglDisplay, 0, nullptr, &formatsCount) || !formatsCount)
            return;

        Vector<EGLint> formats(formatsCount);
        if (!s_eglQueryDmaBufFormatsEXT(m_eglDisplay, formatsCount, reinterpret_cast<EGLint*>(formats.data()), &formatsCount))
            return;

        static PFNEGLQUERYDMABUFMODIFIERSEXTPROC s_eglQueryDmaBufModifiersEXT = extensions.EXT_image_dma_buf_import_modifiers ?
            reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT")) : nullptr;

        // For now we only support formats that can be created with a single GBM buffer for all planes.
        static const Vector<EGLint> s_supportedFormats = {
            DRM_FORMAT_XRGB8888, DRM_FORMAT_RGBX8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_BGRX8888,
            DRM_FORMAT_ARGB8888, DRM_FORMAT_RGBA8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_BGRA8888,
            DRM_FORMAT_RGB565,
            DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010,
            DRM_FORMAT_XRGB16161616F, DRM_FORMAT_XBGR16161616F, DRM_FORMAT_ARGB16161616F, DRM_FORMAT_ABGR16161616F
        };

        m_dmabufFormats = WTF::compactMap(s_supportedFormats, [&](auto format) -> std::optional<DMABufFormat> {
            if (!formats.contains(format))
                return std::nullopt;

            Vector<uint64_t, 1> dmabufModifiers = { DRM_FORMAT_MOD_INVALID };
            if (s_eglQueryDmaBufModifiersEXT) {
                EGLint modifiersCount;
                if (s_eglQueryDmaBufModifiersEXT(m_eglDisplay, format, 0, nullptr, nullptr, &modifiersCount) && modifiersCount) {
                    Vector<EGLuint64KHR> modifiers(modifiersCount);
                    if (s_eglQueryDmaBufModifiersEXT(m_eglDisplay, format, modifiersCount, reinterpret_cast<EGLuint64KHR*>(modifiers.data()), nullptr, &modifiersCount)) {
                        dmabufModifiers.grow(modifiersCount);
                        for (int i = 0; i < modifiersCount; ++i)
                            dmabufModifiers[i] = modifiers[i];
                    }
                }
            }
            return DMABufFormat { static_cast<uint32_t>(format), WTFMove(dmabufModifiers) };
        });
    });
    return m_dmabufFormats;
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
