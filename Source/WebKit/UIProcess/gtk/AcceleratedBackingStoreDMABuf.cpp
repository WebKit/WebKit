/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "AcceleratedBackingStoreDMABuf.h"

#if USE(GBM)
#include "AcceleratedBackingStoreDMABufMessages.h"
#include "LayerTreeContext.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/GBMDevice.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformDisplay.h>
#include <epoxy/egl.h>
#include <gbm.h>
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(X11) && USE(GTK4)
#include <gdk/x11/gdkx.h>
#endif

namespace WebKit {

static bool gtkGLContextIsEGL()
{
    static bool isEGL = true;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(X11)
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11) {
#if USE(GTK4)
        isEGL = !!gdk_x11_display_get_egl_display(gdk_display_get_default());
#else
        isEGL = false;
#endif
    }
#endif
    });
    return isEGL;
}

bool AcceleratedBackingStoreDMABuf::checkRequirements()
{
    static bool available;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const char* disableDMABuf = getenv("WEBKIT_DISABLE_DMABUF_RENDERER");
        if (disableDMABuf && strcmp(disableDMABuf, "0")) {
            available = false;
            return;
        }

        const auto& eglExtensions = WebCore::PlatformDisplay::sharedDisplay().eglExtensions();
        available = eglExtensions.KHR_image_base
            && eglExtensions.KHR_surfaceless_context
            && eglExtensions.EXT_image_dma_buf_import
            && WebCore::GLContext::isExtensionSupported(eglQueryString(nullptr, EGL_EXTENSIONS), "EGL_MESA_platform_surfaceless");
    });
    return available;
}

std::unique_ptr<AcceleratedBackingStoreDMABuf> AcceleratedBackingStoreDMABuf::create(WebPageProxy& webPage)
{
    ASSERT(checkRequirements());
    return std::unique_ptr<AcceleratedBackingStoreDMABuf>(new AcceleratedBackingStoreDMABuf(webPage));
}

AcceleratedBackingStoreDMABuf::AcceleratedBackingStoreDMABuf(WebPageProxy& webPage)
    : AcceleratedBackingStore(webPage)
{
}

AcceleratedBackingStoreDMABuf::~AcceleratedBackingStoreDMABuf()
{
    if (m_surface.id)
        m_webPage.process().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surface.id);

    if (m_gdkGLContext && m_gdkGLContext.get() == gdk_gl_context_get_current())
        gdk_gl_context_clear_current();
}

AcceleratedBackingStoreDMABuf::RenderSource::RenderSource(const WebCore::IntSize& size, float deviceScaleFactor)
    : m_size(size)
    , m_deviceScaleFactor(deviceScaleFactor)
{
}

AcceleratedBackingStoreDMABuf::Texture::Texture(GdkGLContext* glContext, const UnixFileDescriptor& backFD, const UnixFileDescriptor& frontFD, int fourcc, int32_t offset, int32_t stride, const WebCore::IntSize& size, float deviceScaleFactor)
    : RenderSource(size, deviceScaleFactor)
    , m_context(glContext)
{
    gdk_gl_context_make_current(m_context.get());
    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    Vector<EGLAttrib> attributeList = {
        EGL_WIDTH, m_size.width(),
        EGL_HEIGHT, m_size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, fourcc,
        EGL_DMA_BUF_PLANE0_FD_EXT, backFD.value(),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
        EGL_NONE };
    m_backImage = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributeList);
    if (!m_backImage) {
        WTFLogAlways("Failed to create EGL image from DMABuf with file descriptor %d", backFD.value());
        return;
    }
    attributeList[7] = frontFD.value();
    m_frontImage = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributeList);
    if (!m_frontImage) {
        WTFLogAlways("Failed to create EGL image from DMABuf with file descriptor %d", frontFD.value());
        display.destroyEGLImage(m_backImage);
        m_backImage = nullptr;
        return;
    }

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

#if USE(GTK4)
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_backImage);
    m_texture[0] = adoptGRef(gdk_gl_texture_new(m_context.get(), m_textureID, m_size.width(), m_size.height(), nullptr, nullptr));
    m_texture[1] = adoptGRef(gdk_gl_texture_new(m_context.get(), m_textureID, m_size.width(), m_size.height(), nullptr, nullptr));
    m_textureIndex = 1;
#endif
}

AcceleratedBackingStoreDMABuf::Texture::~Texture()
{
    gdk_gl_context_make_current(m_context.get());
    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    if (m_backImage)
        display.destroyEGLImage(m_backImage);
    if (m_frontImage)
        display.destroyEGLImage(m_frontImage);
    if (m_textureID)
        glDeleteTextures(1, &m_textureID);
}

bool AcceleratedBackingStoreDMABuf::Texture::swap()
{
    std::swap(m_backImage, m_frontImage);
    if (!m_textureID)
        return false;

    gdk_gl_context_make_current(m_context.get());
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_frontImage);
#if USE(GTK4)
    m_textureIndex = !m_textureIndex;
#endif
    return true;
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::Texture::snapshot(GtkSnapshot* gtkSnapshot) const
{
    if (!m_textureID)
        return;

    gdk_gl_context_make_current(m_context.get());
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(m_size.width() / m_deviceScaleFactor), static_cast<float>(m_size.height() / m_deviceScaleFactor));
    gtk_snapshot_append_texture(gtkSnapshot, m_texture[m_textureIndex].get(), &bounds);
}
#else
void AcceleratedBackingStoreDMABuf::Texture::paint(GtkWidget* widget, cairo_t* cr, const WebCore::IntRect&) const
{
    if (!m_textureID)
        return;

    gdk_gl_context_make_current(m_context.get());
    cairo_save(cr);
    gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(widget), m_textureID, GL_TEXTURE, m_deviceScaleFactor, 0, 0, m_size.width(), m_size.height());
    cairo_restore(cr);
}
#endif

AcceleratedBackingStoreDMABuf::Surface::Surface(const UnixFileDescriptor& backFD, const UnixFileDescriptor& frontFD, int fourcc, int32_t offset, int32_t stride, const WebCore::IntSize& size, float deviceScaleFactor)
    : RenderSource(size, deviceScaleFactor)
{
    auto* device = WebCore::GBMDevice::singleton().device();
    struct gbm_import_fd_data fdData = { backFD.value(), static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), static_cast<uint32_t>(stride), static_cast<uint32_t>(fourcc) };
    m_backBuffer = gbm_bo_import(device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING);
    if (!m_backBuffer) {
        WTFLogAlways("Failed to import DMABuf with file descriptor %d", fdData.fd);
        return;
    }
    fdData.fd = frontFD.value();
    m_frontBuffer = gbm_bo_import(device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING);
    if (!m_frontBuffer) {
        WTFLogAlways("Failed to import DMABuf with file descriptor %d", fdData.fd);
        gbm_bo_destroy(m_backBuffer);
        m_backBuffer = nullptr;
    }
}

AcceleratedBackingStoreDMABuf::Surface::~Surface()
{
    m_surface = nullptr;

    if (m_backBuffer)
        gbm_bo_destroy(m_backBuffer);
    if (m_frontBuffer)
        gbm_bo_destroy(m_frontBuffer);
}

RefPtr<cairo_surface_t> AcceleratedBackingStoreDMABuf::Surface::map(struct gbm_bo* buffer) const
{
    if (!buffer)
        return nullptr;

    uint32_t mapStride = 0;
    void* mapData = nullptr;
    void* map = gbm_bo_map(buffer, 0, 0, static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), GBM_BO_TRANSFER_READ, &mapStride, &mapData);
    if (!map)
        return nullptr;

    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(map), CAIRO_FORMAT_ARGB32, m_size.width(), m_size.height(), mapStride));
    cairo_surface_set_device_scale(surface.get(), m_deviceScaleFactor, m_deviceScaleFactor);
    struct BufferData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        struct gbm_bo* buffer;
        void* data;
    };
    auto bufferData = makeUnique<BufferData>(BufferData { buffer, mapData });
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, bufferData.release(), [](void* data) {
        std::unique_ptr<BufferData> bufferData(static_cast<BufferData*>(data));
        gbm_bo_unmap(bufferData->buffer, bufferData->data);
    });
    return surface;
}

bool AcceleratedBackingStoreDMABuf::Surface::swap()
{
    std::swap(m_backBuffer, m_frontBuffer);
    m_surface = map(m_frontBuffer);
    if (m_surface) {
        cairo_surface_mark_dirty(m_surface.get());
        return true;
    }
    return false;
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::Surface::snapshot(GtkSnapshot* gtkSnapshot) const
{
    if (!m_surface)
        return;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(m_size.width()), static_cast<float>(m_size.height()));
    RefPtr<cairo_t> cr = adoptRef(gtk_snapshot_append_cairo(gtkSnapshot, &bounds));
    cairo_set_source_surface(cr.get(), m_surface.get(), 0, 0);
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_OVER);
    cairo_paint(cr.get());
}
#else
void AcceleratedBackingStoreDMABuf::Surface::paint(GtkWidget*, cairo_t* cr, const WebCore::IntRect& clipRect) const
{
    if (!m_surface)
        return;

    cairo_save(cr);
    cairo_matrix_t transform;
    cairo_matrix_init(&transform, 1, 0, 0, -1, 0, cairo_image_surface_get_height(m_surface.get()) / m_deviceScaleFactor);
    cairo_transform(cr, &transform);
    cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
    cairo_set_source_surface(cr, m_surface.get(), 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);
    cairo_restore(cr);
}
#endif

void AcceleratedBackingStoreDMABuf::configure(UnixFileDescriptor&& backFD, UnixFileDescriptor&& frontFD, int fourcc, int32_t offset, int32_t stride, WebCore::IntSize&& size)
{
    m_surface.backFD = WTFMove(backFD);
    m_surface.frontFD = WTFMove(frontFD);
    m_surface.fourcc = fourcc;
    m_surface.offset = offset;
    m_surface.stride = stride;
    m_surface.size = WTFMove(size);
    if (gtk_widget_get_realized(m_webPage.viewWidget()))
        m_pendingSource = createSource();
}

std::unique_ptr<AcceleratedBackingStoreDMABuf::RenderSource> AcceleratedBackingStoreDMABuf::createSource()
{
    if (!gtkGLContextIsEGL())
        return makeUnique<Surface>(m_surface.backFD, m_surface.frontFD, m_surface.fourcc, m_surface.offset, m_surface.stride, m_surface.size, m_webPage.deviceScaleFactor());

    ensureGLContext();
    return makeUnique<Texture>(m_gdkGLContext.get(), m_surface.backFD, m_surface.frontFD, m_surface.fourcc, m_surface.offset, m_surface.stride, m_surface.size, m_webPage.deviceScaleFactor());
}

void AcceleratedBackingStoreDMABuf::frame(CompletionHandler<void()>&& completionHandler)
{
    if (m_pendingSource)
        m_committedSource = WTFMove(m_pendingSource);

    std::swap(m_surface.backFD, m_surface.frontFD);
    if (!m_committedSource || !m_committedSource->swap()) {
        completionHandler();
        return;
    }

    ASSERT(!m_frameCompletionHandler);
    m_frameCompletionHandler = WTFMove(completionHandler);
    gtk_widget_queue_draw(m_webPage.viewWidget());
}

void AcceleratedBackingStoreDMABuf::realize()
{
    if (!m_surface.backFD && !m_surface.frontFD)
        return;

    m_committedSource = createSource();
    gtk_widget_queue_draw(m_webPage.viewWidget());
}

void AcceleratedBackingStoreDMABuf::unrealize()
{
    m_pendingSource = nullptr;
    m_committedSource = nullptr;

    if (m_gdkGLContext) {
        if (m_gdkGLContext.get() == gdk_gl_context_get_current())
            gdk_gl_context_clear_current();

        m_gdkGLContext = nullptr;
    }
}

void AcceleratedBackingStoreDMABuf::ensureGLContext()
{
    if (m_gdkGLContext)
        return;

    GUniqueOutPtr<GError> error;
#if USE(GTK4)
    m_gdkGLContext = adoptGRef(gdk_surface_create_gl_context(gtk_native_get_surface(gtk_widget_get_native(m_webPage.viewWidget())), &error.outPtr()));
#else
    m_gdkGLContext = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(m_webPage.viewWidget()), &error.outPtr()));
#endif
    if (!m_gdkGLContext)
        g_error("GDK is not able to create a GL context: %s.", error->message);

#if USE(OPENGL_ES)
    gdk_gl_context_set_use_es(m_gdkGLContext.get(), TRUE);
#endif

    if (!gdk_gl_context_realize(m_gdkGLContext.get(), &error.outPtr()))
        g_error("GDK failed to realize the GL context: %s.", error->message);
}

bool AcceleratedBackingStoreDMABuf::makeContextCurrent()
{
    if (!gtkGLContextIsEGL())
        return false;

    if (!gtk_widget_get_realized(m_webPage.viewWidget()))
        return false;

    ensureGLContext();
    gdk_gl_context_make_current(m_gdkGLContext.get());
    return true;
}

void AcceleratedBackingStoreDMABuf::update(const LayerTreeContext& context)
{
    if (m_surface.id == context.contextID)
        return;

    if (m_surface.id) {
        if (auto completionHandler = std::exchange(m_frameCompletionHandler, nullptr))
            completionHandler();
        m_webPage.process().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surface.id);
    }

    m_surface.id = context.contextID;
    if (m_surface.id)
        m_webPage.process().addMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surface.id, *this);
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::snapshot(GtkSnapshot* gtkSnapshot)
{
    if (!m_committedSource)
        return;

    m_committedSource->snapshot(gtkSnapshot);
    if (auto completionHandler = std::exchange(m_frameCompletionHandler, nullptr))
        completionHandler();
}
#else
bool AcceleratedBackingStoreDMABuf::paint(cairo_t* cr, const WebCore::IntRect& clipRect)
{
    if (!m_committedSource)
        return false;

    m_committedSource->paint(m_webPage.viewWidget(), cr, clipRect);
    if (auto completionHandler = std::exchange(m_frameCompletionHandler, nullptr))
        completionHandler();

    return true;
}
#endif

} // namespace WebKit

#endif // USE(GBM)
