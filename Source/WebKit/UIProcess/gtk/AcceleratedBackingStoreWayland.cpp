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

#include "config.h"
#include "AcceleratedBackingStoreWayland.h"

#if PLATFORM(WAYLAND) && USE(EGL)

#include "WaylandCompositor.h"
#include "WebPageProxy.h"
#include <WebCore/CairoUtilities.h>
#include <WebCore/GLContext.h>

#if USE(OPENGL_ES)
#include <GLES2/gl2.h>
#else
#include <WebCore/OpenGLShims.h>
#endif

namespace WebKit {
using namespace WebCore;

std::unique_ptr<AcceleratedBackingStoreWayland> AcceleratedBackingStoreWayland::create(WebPageProxy& webPage)
{
    if (!WaylandCompositor::singleton().isRunning())
        return nullptr;
    return std::unique_ptr<AcceleratedBackingStoreWayland>(new AcceleratedBackingStoreWayland(webPage));
}

AcceleratedBackingStoreWayland::AcceleratedBackingStoreWayland(WebPageProxy& webPage)
    : AcceleratedBackingStore(webPage)
{
    WaylandCompositor::singleton().registerWebPage(m_webPage);
}

AcceleratedBackingStoreWayland::~AcceleratedBackingStoreWayland()
{
    WaylandCompositor::singleton().unregisterWebPage(m_webPage);

#if GTK_CHECK_VERSION(3, 16, 0)
    if (m_gdkGLContext && m_gdkGLContext.get() == gdk_gl_context_get_current())
        gdk_gl_context_clear_current();
#endif
}

void AcceleratedBackingStoreWayland::tryEnsureGLContext()
{
    if (m_glContextInitialized)
        return;

    m_glContextInitialized = true;

#if GTK_CHECK_VERSION(3, 16, 0)
    GUniqueOutPtr<GError> error;
    m_gdkGLContext = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(m_webPage.viewWidget()), &error.outPtr()));
    if (m_gdkGLContext) {
#if USE(OPENGL_ES)
        gdk_gl_context_set_use_es(m_gdkGLContext.get(), TRUE);
#endif
        return;
    }

    g_warning("GDK is not able to create a GL context, falling back to glReadPixels (slow!): %s", error->message);
#endif

    m_glContext = GLContext::createOffscreenContext();
}

bool AcceleratedBackingStoreWayland::makeContextCurrent()
{
    tryEnsureGLContext();

#if GTK_CHECK_VERSION(3, 16, 0)
    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        return true;
    }
#endif

    return m_glContext ? m_glContext->makeContextCurrent() : false;
}

bool AcceleratedBackingStoreWayland::paint(cairo_t* cr, const IntRect& clipRect)
{
    GLuint texture;
    IntSize textureSize;
    if (!WaylandCompositor::singleton().getTexture(m_webPage, texture, textureSize))
        return false;

    cairo_save(cr);
    AcceleratedBackingStore::paint(cr, clipRect);

#if GTK_CHECK_VERSION(3, 16, 0)
    if (m_gdkGLContext) {
        gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(m_webPage.viewWidget()), texture, GL_TEXTURE, m_webPage.deviceScaleFactor(), 0, 0, textureSize.width(), textureSize.height());
        cairo_restore(cr);
        return true;
    }
#endif

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
