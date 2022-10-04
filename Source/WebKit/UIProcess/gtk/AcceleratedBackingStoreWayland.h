/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AcceleratedBackingStore.h"

#if PLATFORM(WAYLAND)

#include <WebCore/RefPtrCairo.h>
#include <gtk/gtk.h>
#include <wpe/fdo.h>
#include <wtf/glib/GRefPtr.h>

typedef void* EGLImageKHR;
typedef struct _GdkGLContext GdkGLContext;
struct wpe_fdo_egl_exported_image;
struct wpe_fdo_shm_exported_buffer;

namespace WebCore {
class GLContext;
class IntSize;
}

namespace WebKit {

class WebPageProxy;

class AcceleratedBackingStoreWayland final : public AcceleratedBackingStore {
    WTF_MAKE_NONCOPYABLE(AcceleratedBackingStoreWayland); WTF_MAKE_FAST_ALLOCATED;
public:
    static bool checkRequirements();
    static std::unique_ptr<AcceleratedBackingStoreWayland> create(WebPageProxy&);
    ~AcceleratedBackingStoreWayland();

private:
    AcceleratedBackingStoreWayland(WebPageProxy&);

    void tryEnsureGLContext();
    void displayImage(struct wpe_fdo_egl_exported_image*);
#if WPE_FDO_CHECK_VERSION(1,7,0)
    void displayBuffer(struct wpe_fdo_shm_exported_buffer*);
#endif
    bool tryEnsureTexture(unsigned&, WebCore::IntSize&);
    void downloadTexture(unsigned, const WebCore::IntSize&);

#if USE(GTK4)
    void snapshot(GtkSnapshot*) override;
#else
    bool paint(cairo_t*, const WebCore::IntRect&) override;
#endif
    void unrealize() override;
    bool makeContextCurrent() override;
    void update(const LayerTreeContext&) override;
    int renderHostFileDescriptor() override;

    RefPtr<cairo_surface_t> m_surface;
    bool m_glContextInitialized { false };
    GRefPtr<GdkGLContext> m_gdkGLContext;
    std::unique_ptr<WebCore::GLContext> m_glContext;

    struct wpe_view_backend_exportable_fdo* m_exportable { nullptr };
    uint64_t m_surfaceID { 0 };
    struct {
        unsigned viewTexture { 0 };
        struct wpe_fdo_egl_exported_image* committedImage { nullptr };
        struct wpe_fdo_egl_exported_image* pendingImage { nullptr };
    } m_egl;

#if WPE_FDO_CHECK_VERSION(1,7,0)
    struct {
        bool pendingFrame { false };
    } m_shm;
#endif
};

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
