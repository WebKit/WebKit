/*
 * Copyright (C) 2016, 2019 Igalia S.L.
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

#include "config.h"
#include "AcceleratedBackingStoreWayland.h"

#if PLATFORM(WAYLAND) && USE(EGL)

#include "LayerTreeContext.h"
#include "WebPageProxy.h"
// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and eglplatform.h, included by egl.h, checks that to decide whether it's Wayland platform.
#if USE(GTK4)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <WebCore/CairoUtilities.h>
#include <WebCore/GLContext.h>

#if USE(OPENGL_ES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <WebCore/OpenGLShims.h>
#endif

#if USE(WPE_RENDERER)
#include <wpe/fdo-egl.h>
#if WPE_FDO_CHECK_VERSION(1, 7, 0)
#include <wayland-server.h>
#include <wpe/unstable/fdo-shm.h>
#endif
#else
#include "WaylandCompositor.h"
#endif

#if USE(WPE_RENDERER)
#if !defined(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES);
#endif

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glImageTargetTexture2D;
#endif

namespace WebKit {
using namespace WebCore;

enum class WaylandImpl { Unsupported, EGL, SHM };
static Optional<WaylandImpl> s_waylandImpl;

#if USE(WPE_RENDERER)
static bool isEGLImageAvailable(bool useIndexedGetString)
{
#if USE(OPENGL_ES)
    UNUSED_PARAM(useIndexedGetString);
#else
    if (useIndexedGetString) {
        GLint numExtensions = 0;
        ::glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i) {
            String extension = String(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
            if (extension == "GL_OES_EGL_image" || extension == "GL_OES_EGL_image_external")
                return true;
        }
    } else
#endif
    {
        String extensionsString = String(reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS)));
        for (auto& extension : extensionsString.split(' ')) {
            if (extension == "GL_OES_EGL_image" || extension == "GL_OES_EGL_image_external")
                return true;
        }
    }

    return false;
}

static bool tryInitializeEGL()
{
    auto* eglDisplay = PlatformDisplay::sharedDisplay().eglDisplay();
    const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    if (!GLContext::isExtensionSupported(extensions, "EGL_WL_bind_wayland_display"))
        return false;

    if (!wpe_fdo_initialize_for_egl_display(eglDisplay))
        return false;

    std::unique_ptr<WebCore::GLContext> eglContext = GLContext::createOffscreenContext();
    if (!eglContext)
        return false;

    if (!eglContext->makeContextCurrent())
        return false;

#if USE(OPENGL_ES)
    if (isEGLImageAvailable(false))
#else
    if (isEGLImageAvailable(GLContext::current()->version() >= 320))
#endif
        glImageTargetTexture2D = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    if (!glImageTargetTexture2D)
        return false;

    s_waylandImpl = WaylandImpl::EGL;
    return true;
}

static bool tryInitializeSHM()
{
#if WPE_FDO_CHECK_VERSION(1, 7, 0)
    if (!wpe_fdo_initialize_shm())
        return false;

    s_waylandImpl = WaylandImpl::SHM;
    return true;
#else
    return false;
#endif
}
#endif // USE(WPE_RENDERER)

bool AcceleratedBackingStoreWayland::checkRequirements()
{
#if USE(WPE_RENDERER)
    if (s_waylandImpl)
        return s_waylandImpl.value() != WaylandImpl::Unsupported;

    if (tryInitializeEGL())
        return true;

    if (tryInitializeSHM())
        return true;

    WTFLogAlways("AcceleratedBackingStoreWayland requires glEGLImageTargetTexture2D or shm interface");
#else
    if (WaylandCompositor::singleton().isRunning()) {
        s_waylandImpl = WaylandImpl::EGL;
        return true;
    }
#endif

    s_waylandImpl = WaylandImpl::Unsupported;
    return false;
}

std::unique_ptr<AcceleratedBackingStoreWayland> AcceleratedBackingStoreWayland::create(WebPageProxy& webPage)
{
    ASSERT(checkRequirements());
    return std::unique_ptr<AcceleratedBackingStoreWayland>(new AcceleratedBackingStoreWayland(webPage));
}

AcceleratedBackingStoreWayland::AcceleratedBackingStoreWayland(WebPageProxy& webPage)
    : AcceleratedBackingStore(webPage)
{
#if USE(WPE_RENDERER)
    static struct wpe_view_backend_exportable_fdo_egl_client exportableEGLClient = {
        // export_egl_image
        nullptr,
        // export_fdo_egl_image
        [](void* data, struct wpe_fdo_egl_exported_image* image)
        {
            static_cast<AcceleratedBackingStoreWayland*>(data)->displayImage(image);
        },
        // padding
        nullptr, nullptr, nullptr
    };

#if WPE_FDO_CHECK_VERSION(1, 7, 0)
    static struct wpe_view_backend_exportable_fdo_client exportableClient = {
        nullptr,
        nullptr,
        // export_shm_buffer
        [](void* data, struct wpe_fdo_shm_exported_buffer* buffer)
        {
            static_cast<AcceleratedBackingStoreWayland*>(data)->displayBuffer(buffer);
        },
        nullptr,
        nullptr,
    };
#endif

    auto viewSize = webPage.viewSize();
    switch (s_waylandImpl.value()) {
    case WaylandImpl::EGL:
        m_exportable = wpe_view_backend_exportable_fdo_egl_create(&exportableEGLClient, this, viewSize.width(), viewSize.height());
        break;
    case WaylandImpl::SHM:
#if WPE_FDO_CHECK_VERSION(1, 7, 0)
        m_exportable = wpe_view_backend_exportable_fdo_create(&exportableClient, this, viewSize.width(), viewSize.height());
        break;
#else
        FALLTHROUGH;
#endif
    case WaylandImpl::Unsupported:
        RELEASE_ASSERT_NOT_REACHED();
    }

    wpe_view_backend_initialize(wpe_view_backend_exportable_fdo_get_view_backend(m_exportable));
#else
    WaylandCompositor::singleton().registerWebPage(m_webPage);
#endif
}

AcceleratedBackingStoreWayland::~AcceleratedBackingStoreWayland()
{
#if USE(WPE_RENDERER)
    if (s_waylandImpl.value() == WaylandImpl::EGL) {
        if (m_egl.pendingImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.pendingImage);
        if (m_egl.committedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.committedImage);
        if (m_egl.viewTexture) {
            if (makeContextCurrent())
                glDeleteTextures(1, &m_egl.viewTexture);
        }
    }
    wpe_view_backend_exportable_fdo_destroy(m_exportable);
#else
    WaylandCompositor::singleton().unregisterWebPage(m_webPage);
#endif

    if (m_gdkGLContext && m_gdkGLContext.get() == gdk_gl_context_get_current())
        gdk_gl_context_clear_current();
}

void AcceleratedBackingStoreWayland::realize()
{
#if !USE(WPE_RENDERER)
    WaylandCompositor::singleton().bindWebPage(m_webPage);
#endif
}

void AcceleratedBackingStoreWayland::unrealize()
{
    if (!m_glContextInitialized)
        return;

#if USE(WPE_RENDERER)
    if (s_waylandImpl.value() == WaylandImpl::EGL) {
        if (m_egl.viewTexture) {
            if (makeContextCurrent())
                glDeleteTextures(1, &m_egl.viewTexture);
            m_egl.viewTexture = 0;
        }
    }
#else
    WaylandCompositor::singleton().unbindWebPage(m_webPage);
#endif

    if (m_gdkGLContext && m_gdkGLContext.get() == gdk_gl_context_get_current())
        gdk_gl_context_clear_current();

    m_glContextInitialized = false;
}

void AcceleratedBackingStoreWayland::tryEnsureGLContext()
{
    if (m_glContextInitialized || !gtk_widget_get_realized(m_webPage.viewWidget()))
        return;

    m_glContextInitialized = true;
    if (s_waylandImpl.value() != WaylandImpl::EGL)
        return;

    GUniqueOutPtr<GError> error;
#if USE(GTK4)
    m_gdkGLContext = adoptGRef(gdk_surface_create_gl_context(gtk_native_get_surface(gtk_widget_get_native(m_webPage.viewWidget())), &error.outPtr()));
#else
    m_gdkGLContext = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(m_webPage.viewWidget()), &error.outPtr()));
#endif
    if (m_gdkGLContext) {
#if USE(OPENGL_ES)
        gdk_gl_context_set_use_es(m_gdkGLContext.get(), TRUE);
#endif
        gdk_gl_context_realize(m_gdkGLContext.get(), &error.outPtr());
        if (!error)
            return;
    }

    g_warning("GDK is not able to create a GL context, falling back to glReadPixels (slow!): %s", error->message);

    m_glContext = GLContext::createOffscreenContext();
}

bool AcceleratedBackingStoreWayland::makeContextCurrent()
{
    tryEnsureGLContext();

    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        return true;
    }
    return m_glContext ? m_glContext->makeContextCurrent() : false;
}

#if USE(WPE_RENDERER)
void AcceleratedBackingStoreWayland::update(const LayerTreeContext& context)
{
    if (m_surfaceID == context.contextID)
        return;

    m_surfaceID = context.contextID;
    switch (s_waylandImpl.value()) {
    case WaylandImpl::EGL:
        if (m_egl.pendingImage) {
            wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.pendingImage);
            m_egl.pendingImage = nullptr;
        }
        break;
    case WaylandImpl::SHM:
#if WPE_FDO_CHECK_VERSION(1, 7, 0)
        if (m_shm.pendingFrame) {
            wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
            m_shm.pendingFrame = false;
        }
        break;
#else
        FALLTHROUGH;
#endif
    case WaylandImpl::Unsupported:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

int AcceleratedBackingStoreWayland::renderHostFileDescriptor()
{
    return wpe_view_backend_get_renderer_host_fd(wpe_view_backend_exportable_fdo_get_view_backend(m_exportable));
}

void AcceleratedBackingStoreWayland::displayImage(struct wpe_fdo_egl_exported_image* image)
{
    ASSERT(s_waylandImpl.value() == WaylandImpl::EGL);
    if (!m_surfaceID) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
        if (image != m_egl.committedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, image);
        return;
    }

    if (m_egl.pendingImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.pendingImage);
    m_egl.pendingImage = image;

    m_webPage.setViewNeedsDisplay(IntRect(IntPoint::zero(), m_webPage.viewSize()));
}

#if WPE_FDO_CHECK_VERSION(1, 7, 0)
void AcceleratedBackingStoreWayland::displayBuffer(struct wpe_fdo_shm_exported_buffer* buffer)
{
    ASSERT(s_waylandImpl.value() == WaylandImpl::SHM);
    if (!m_surfaceID) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
        wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(m_exportable, buffer);
        return;
    }

    m_shm.pendingFrame = true;

    struct wl_shm_buffer* shmBuffer = wpe_fdo_shm_exported_buffer_get_shm_buffer(buffer);
    auto format = wl_shm_buffer_get_format(shmBuffer);
    if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888) {
        wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(m_exportable, buffer);
        return;
    }

    int32_t width = wl_shm_buffer_get_width(shmBuffer);
    int32_t height = wl_shm_buffer_get_height(shmBuffer);
    int32_t stride = wl_shm_buffer_get_stride(shmBuffer);
    int32_t surfaceStride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    if (!m_surface || cairo_image_surface_get_width(m_surface.get()) != width || cairo_image_surface_get_height(m_surface.get()) != height) {
        auto* surfaceData = fastZeroedMalloc(height * surfaceStride);
        m_surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(surfaceData), CAIRO_FORMAT_ARGB32, width, height, surfaceStride));
        static cairo_user_data_key_t s_surfaceDataKey;
        cairo_surface_set_user_data(m_surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) {
            fastFree(data);
        });
        cairo_surface_set_device_scale(m_surface.get(), m_webPage.deviceScaleFactor(), m_webPage.deviceScaleFactor());
    }

    unsigned char* surfaceData = cairo_image_surface_get_data(m_surface.get());
    wl_shm_buffer_begin_access(shmBuffer);
    auto* data = static_cast<unsigned char*>(wl_shm_buffer_get_data(shmBuffer));
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            surfaceData[surfaceStride * y + 4 * x + 0] = data[stride * y + 4 * x + 0];
            surfaceData[surfaceStride * y + 4 * x + 1] = data[stride * y + 4 * x + 1];
            surfaceData[surfaceStride * y + 4 * x + 2] = data[stride * y + 4 * x + 2];
            surfaceData[surfaceStride * y + 4 * x + 3] = data[stride * y + 4 * x + 3];
        }
    }
    wl_shm_buffer_end_access(shmBuffer);
    wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(m_exportable, buffer);

    cairo_surface_mark_dirty(m_surface.get());
    m_webPage.setViewNeedsDisplay(IntRect(IntPoint::zero(), m_webPage.viewSize()));
}
#endif
#endif

bool AcceleratedBackingStoreWayland::tryEnsureTexture(unsigned& texture, IntSize& textureSize)
{
    ASSERT(s_waylandImpl.value() == WaylandImpl::EGL);
#if USE(WPE_RENDERER)
    if (!makeContextCurrent())
        return false;

    if (m_egl.pendingImage) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);

        if (m_egl.committedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.committedImage);
        m_egl.committedImage = m_egl.pendingImage;
        m_egl.pendingImage = nullptr;
    }

    if (!m_egl.committedImage)
        return false;

    if (!m_egl.viewTexture) {
        glGenTextures(1, &m_egl.viewTexture);
        glBindTexture(GL_TEXTURE_2D, m_egl.viewTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, m_egl.viewTexture);
    glImageTargetTexture2D(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(m_egl.committedImage));

    texture = m_egl.viewTexture;
    textureSize = { static_cast<int>(wpe_fdo_egl_exported_image_get_width(m_egl.committedImage)), static_cast<int>(wpe_fdo_egl_exported_image_get_height(m_egl.committedImage)) };
#else
    if (!WaylandCompositor::singleton().getTexture(m_webPage, texture, textureSize))
        return false;
#endif

    return true;
}

void AcceleratedBackingStoreWayland::downloadTexture(unsigned texture, const IntSize& textureSize)
{
    ASSERT(s_waylandImpl.value() == WaylandImpl::EGL);
    ASSERT(m_glContext);

    if (!m_surface || cairo_image_surface_get_width(m_surface.get()) != textureSize.width() || cairo_image_surface_get_height(m_surface.get()) != textureSize.height())
        m_surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textureSize.width(), textureSize.height()));

    cairo_surface_set_device_scale(m_surface.get(), m_webPage.deviceScaleFactor(), m_webPage.deviceScaleFactor());

    GLuint fb;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);

#if USE(OPENGL_ES)
    unsigned char* data = cairo_image_surface_get_data(m_surface.get());
    if (cairo_image_surface_get_stride(m_surface.get()) == textureSize.width() * 4)
        glReadPixels(0, 0, textureSize.width(), textureSize.height(), GL_RGBA, GL_UNSIGNED_BYTE, data);
    else {
        int strideBytes = cairo_image_surface_get_stride(m_surface.get());
        for (int i = 0; i < textureSize.height(); i++) {
            unsigned char* dataOffset = data + i * strideBytes;
            glReadPixels(0, i, textureSize.width(), 1, GL_RGBA, GL_UNSIGNED_BYTE, dataOffset);
        }
    }

    // Convert to BGRA.
    int totalBytes = textureSize.width() * textureSize.height() * 4;
    for (int i = 0; i < totalBytes; i += 4)
        std::swap(data[i], data[i + 2]);
#else
    glPixelStorei(GL_PACK_ROW_LENGTH, cairo_image_surface_get_stride(m_surface.get()) / 4);
    glReadPixels(0, 0, textureSize.width(), textureSize.height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, cairo_image_surface_get_data(m_surface.get()));
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);

    cairo_surface_mark_dirty(m_surface.get());
}

#if USE(GTK4)
void AcceleratedBackingStoreWayland::snapshot(GtkSnapshot* gtkSnapshot)
{
    switch (s_waylandImpl.value()) {
    case WaylandImpl::EGL: {
        GLuint texture;
        IntSize textureSize;
        if (!tryEnsureTexture(texture, textureSize))
            return;

        graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(textureSize.width()), static_cast<float>(textureSize.height()));
        if (m_gdkGLContext) {
            GRefPtr<GdkTexture> gdkTexture = adoptGRef(gdk_gl_texture_new(m_gdkGLContext.get(), texture, textureSize.width(), textureSize.height(), nullptr, nullptr));
            gtk_snapshot_append_texture(gtkSnapshot, gdkTexture.get(), &bounds);
            return;
        }

        downloadTexture(texture, textureSize);
        break;
    }
    case WaylandImpl::SHM:
#if USE(WPE_RENDERER)
#if WPE_FDO_CHECK_VERSION(1, 7, 0)
        if (m_shm.pendingFrame) {
            wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
            m_shm.pendingFrame = false;
        }

        if (!m_surface)
            return;
        break;
#else
        FALLTHROUGH;
#endif // WPE_FDO_CHECK_VERSION
#else
        FALLTHROUGH;
#endif
    case WaylandImpl::Unsupported:
        RELEASE_ASSERT_NOT_REACHED();
    }

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(cairo_image_surface_get_width(m_surface.get())), static_cast<float>(cairo_image_surface_get_height(m_surface.get())));
    RefPtr<cairo_t> cr = adoptRef(gtk_snapshot_append_cairo(gtkSnapshot, &bounds));

    // The compositor renders the texture flipped for gdk, fix that here.
    cairo_matrix_t transform;
    cairo_matrix_init(&transform, 1, 0, 0, -1, 0, cairo_image_surface_get_height(m_surface.get()) / m_webPage.deviceScaleFactor());
    cairo_transform(cr.get(), &transform);

    cairo_set_source_surface(cr.get(), m_surface.get(), 0, 0);
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_OVER);
    cairo_paint(cr.get());

    cairo_surface_flush(m_surface.get());
}
#else
bool AcceleratedBackingStoreWayland::paint(cairo_t* cr, const IntRect& clipRect)
{
    switch (s_waylandImpl.value()) {
    case WaylandImpl::EGL: {
        GLuint texture;
        IntSize textureSize;
        if (!tryEnsureTexture(texture, textureSize))
            return true;

        cairo_save(cr);

        if (m_gdkGLContext) {
            gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(m_webPage.viewWidget()), texture, GL_TEXTURE, m_webPage.deviceScaleFactor(), 0, 0, textureSize.width(), textureSize.height());
            cairo_restore(cr);
            return true;
        }

        downloadTexture(texture, textureSize);
        break;
    }
    case WaylandImpl::SHM:
#if USE(WPE_RENDERER)
#if WPE_FDO_CHECK_VERSION(1, 7, 0)
        if (m_shm.pendingFrame) {
            wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
            m_shm.pendingFrame = false;
        }

        if (!m_surface)
            return true;

        cairo_save(cr);
        break;
#else
        FALLTHROUGH;
#endif // WPE_FDO_CHECK_VERSION
#else
        FALLTHROUGH;
#endif // USE(WPE_RENDERER)
    case WaylandImpl::Unsupported:
        RELEASE_ASSERT_NOT_REACHED();
    }

    // The compositor renders the texture flipped for gdk_cairo_draw_from_gl, fix that here.
    cairo_matrix_t transform;
    cairo_matrix_init(&transform, 1, 0, 0, -1, 0, cairo_image_surface_get_height(m_surface.get()) / m_webPage.deviceScaleFactor());
    cairo_transform(cr, &transform);

    cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
    cairo_set_source_surface(cr, m_surface.get(), 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);

    cairo_restore(cr);

    cairo_surface_flush(m_surface.get());

    return true;
}
#endif

} // namespace WebKit

#endif // PLATFORM(WAYLAND) && USE(EGL)
