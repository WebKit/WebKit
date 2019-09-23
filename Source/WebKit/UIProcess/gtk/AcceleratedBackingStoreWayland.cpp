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
#include <gdk/gdkwayland.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <WebCore/CairoUtilities.h>
#include <WebCore/GLContext.h>

#if USE(OPENGL_ES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <WebCore/Extensions3DOpenGLES.h>
#else
#include <WebCore/Extensions3DOpenGL.h>
#include <WebCore/OpenGLShims.h>
#endif

#if USE(WPE_RENDERER)
#include <wpe/fdo-egl.h>
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

bool AcceleratedBackingStoreWayland::checkRequirements()
{
#if USE(WPE_RENDERER)
    if (!glImageTargetTexture2D) {
        if (!wpe_fdo_initialize_for_egl_display(PlatformDisplay::sharedDisplay().eglDisplay()))
            return false;

        std::unique_ptr<WebCore::GLContext> eglContext = GLContext::createOffscreenContext();
        if (!eglContext)
            return false;

        if (!eglContext->makeContextCurrent())
            return false;

#if USE(OPENGL_ES)
        std::unique_ptr<Extensions3DOpenGLES> glExtensions = makeUnique<Extensions3DOpenGLES>(nullptr,  false);
#else
        std::unique_ptr<Extensions3DOpenGL> glExtensions = makeUnique<Extensions3DOpenGL>(nullptr, GLContext::current()->version() >= 320);
#endif
        if (glExtensions->supports("GL_OES_EGL_image") || glExtensions->supports("GL_OES_EGL_image_external"))
            glImageTargetTexture2D = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    }

    if (!glImageTargetTexture2D) {
        WTFLogAlways("AcceleratedBackingStoreWPE requires glEGLImageTargetTexture2D.");
        return false;
    }

    return true;
#else
    return WaylandCompositor::singleton().isRunning();
#endif
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
    static struct wpe_view_backend_exportable_fdo_egl_client exportableClient = {
        // export_egl_image
        nullptr,
        // export_fdo_egl_image
        [](void* data, struct wpe_fdo_egl_exported_image* image)
        {
            static_cast<AcceleratedBackingStoreWayland*>(data)->displayBuffer(image);
        },
        // padding
        nullptr, nullptr, nullptr
    };

    auto viewSize = webPage.viewSize();
    m_exportable = wpe_view_backend_exportable_fdo_egl_create(&exportableClient, this, viewSize.width(), viewSize.height());
    wpe_view_backend_initialize(wpe_view_backend_exportable_fdo_get_view_backend(m_exportable));
#else
    WaylandCompositor::singleton().registerWebPage(m_webPage);
#endif
}

AcceleratedBackingStoreWayland::~AcceleratedBackingStoreWayland()
{
#if USE(WPE_RENDERER)
    if (m_pendingImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_pendingImage);
    if (m_committedImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_committedImage);
    if (m_viewTexture) {
        if (makeContextCurrent())
            glDeleteTextures(1, &m_viewTexture);
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
    if (m_viewTexture) {
        if (makeContextCurrent())
            glDeleteTextures(1, &m_viewTexture);
        m_viewTexture = 0;
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

    GUniqueOutPtr<GError> error;
    m_gdkGLContext = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(m_webPage.viewWidget()), &error.outPtr()));
    if (m_gdkGLContext) {
#if USE(OPENGL_ES)
        gdk_gl_context_set_use_es(m_gdkGLContext.get(), TRUE);
#endif
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
    if (m_pendingImage) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_pendingImage);
        m_pendingImage = nullptr;
    }
}

int AcceleratedBackingStoreWayland::renderHostFileDescriptor()
{
    return wpe_view_backend_get_renderer_host_fd(wpe_view_backend_exportable_fdo_get_view_backend(m_exportable));
}

void AcceleratedBackingStoreWayland::displayBuffer(struct wpe_fdo_egl_exported_image* image)
{
    if (!m_surfaceID) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
        if (image != m_committedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, image);
        return;
    }

    if (m_pendingImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_pendingImage);
    m_pendingImage = image;

    m_webPage.setViewNeedsDisplay(IntRect(IntPoint::zero(), m_webPage.viewSize()));
}
#endif

bool AcceleratedBackingStoreWayland::paint(cairo_t* cr, const IntRect& clipRect)
{
    GLuint texture;
    IntSize textureSize;

#if USE(WPE_RENDERER)
    if (!makeContextCurrent())
        return true;

    if (m_pendingImage) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);

        if (m_committedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_committedImage);
        m_committedImage = m_pendingImage;
        m_pendingImage = nullptr;
    }

    if (!m_committedImage)
        return true;

    if (!m_viewTexture) {
        glGenTextures(1, &m_viewTexture);
        glBindTexture(GL_TEXTURE_2D, m_viewTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, m_viewTexture);
    glImageTargetTexture2D(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(m_committedImage));

    texture = m_viewTexture;
    textureSize = { static_cast<int>(wpe_fdo_egl_exported_image_get_width(m_committedImage)), static_cast<int>(wpe_fdo_egl_exported_image_get_height(m_committedImage)) };
#else
    if (!WaylandCompositor::singleton().getTexture(m_webPage, texture, textureSize))
        return false;
#endif

    cairo_save(cr);

    if (m_gdkGLContext) {
        gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(m_webPage.viewWidget()), texture, GL_TEXTURE, m_webPage.deviceScaleFactor(), 0, 0, textureSize.width(), textureSize.height());
        cairo_restore(cr);
        return true;
    }

    ASSERT(m_glContext);

    if (!m_surface || cairo_image_surface_get_width(m_surface.get()) != textureSize.width() || cairo_image_surface_get_height(m_surface.get()) != textureSize.height())
        m_surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textureSize.width(), textureSize.height()));

    cairoSurfaceSetDeviceScale(m_surface.get(), m_webPage.deviceScaleFactor(), m_webPage.deviceScaleFactor());

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

    // The surface can be modified by the web process at any time, so we mark it
    // as dirty to ensure we always render the updated contents as soon as possible.
    cairo_surface_mark_dirty(m_surface.get());

    // The compositor renders the texture flipped for gdk_cairo_draw_from_gl, fix that here.
    cairo_matrix_t transform;
    cairo_matrix_init(&transform, 1, 0, 0, -1, 0, textureSize.height() / m_webPage.deviceScaleFactor());
    cairo_transform(cr, &transform);

    cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
    cairo_set_source_surface(cr, m_surface.get(), 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);

    cairo_restore(cr);

    return true;
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND) && USE(EGL)
