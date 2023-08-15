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

#include "AcceleratedBackingStoreDMABufMessages.h"
#include "AcceleratedSurfaceDMABufMessages.h"
#include "DMABufRendererBufferMode.h"
#include "LayerTreeContext.h"
#include "ShareableBitmap.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/DMABufFormat.h>
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformDisplay.h>
#include <epoxy/egl.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(GBM)
#include <gbm.h>
#endif

#if PLATFORM(X11) && USE(GTK4)
#include <gdk/x11/gdkx.h>
#endif

namespace WebKit {

OptionSet<DMABufRendererBufferMode> AcceleratedBackingStoreDMABuf::rendererBufferMode()
{
    static OptionSet<DMABufRendererBufferMode> mode;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const char* disableDMABuf = getenv("WEBKIT_DISABLE_DMABUF_RENDERER");
        if (disableDMABuf && strcmp(disableDMABuf, "0"))
            return;

        const char* platformExtensions = eglQueryString(nullptr, EGL_EXTENSIONS);
        if (!WebCore::GLContext::isExtensionSupported(platformExtensions, "EGL_KHR_platform_gbm")
            && !WebCore::GLContext::isExtensionSupported(platformExtensions, "EGL_MESA_platform_surfaceless")) {
            return;
        }

        mode.add(DMABufRendererBufferMode::SharedMemory);

        const auto& eglExtensions = WebCore::PlatformDisplay::sharedDisplay().eglExtensions();
        if (eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import)
            mode.add(DMABufRendererBufferMode::Hardware);
    });
    return mode;
}
bool AcceleratedBackingStoreDMABuf::checkRequirements()
{
    return !rendererBufferMode().isEmpty();
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

AcceleratedBackingStoreDMABuf::Texture::Texture(GdkGLContext* glContext, const UnixFileDescriptor& backFD, const UnixFileDescriptor& frontFD, const UnixFileDescriptor& displayFD, const WebCore::IntSize& size, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier, float deviceScaleFactor)
    : RenderSource(size, deviceScaleFactor)
    , m_context(glContext)
{
    gdk_gl_context_make_current(m_context.get());
    auto& display = WebCore::PlatformDisplay::sharedDisplay();

    auto createImage = [&](const UnixFileDescriptor& fd) -> EGLImage {
        Vector<EGLAttrib> attributes = {
            EGL_WIDTH, m_size.width(),
            EGL_HEIGHT, m_size.height(),
            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(format),
            EGL_DMA_BUF_PLANE0_FD_EXT, fd.value(),
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLAttrib>(offset),
            EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLAttrib>(stride),
        };
        if (modifier != uint64_t(WebCore::DMABufFormat::Modifier::Invalid) && display.eglExtensions().EXT_image_dma_buf_import_modifiers) {
            std::array<EGLAttrib, 4> modifierAttributes {
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLAttrib>(modifier >> 32),
                EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLAttrib>(modifier & 0xffffffff),
            };
            attributes.append(std::span<const EGLAttrib> { modifierAttributes });
        }
        attributes.append(EGL_NONE);

        return display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    };

    m_backImage = createImage(backFD);
    m_frontImage = createImage(frontFD);
    m_displayImage = createImage(displayFD);
    if (!m_backImage || !m_frontImage || !m_displayImage) {
        WTFLogAlways("Failed to create EGL image from DMABufs with file descriptors %d, %d and %d", backFD.value(), frontFD.value(), displayFD.value());
        if (m_backImage)
            display.destroyEGLImage(m_backImage);
        m_backImage = nullptr;
        if (m_frontImage)
            display.destroyEGLImage(m_frontImage);
        m_frontImage = nullptr;
        if (m_displayImage)
            display.destroyEGLImage(m_displayImage);
        m_displayImage = nullptr;
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
    if (m_displayImage)
        display.destroyEGLImage(m_displayImage);
    if (m_textureID)
        glDeleteTextures(1, &m_textureID);
}

void AcceleratedBackingStoreDMABuf::Texture::frame()
{
    std::swap(m_backImage, m_frontImage);
}

void AcceleratedBackingStoreDMABuf::Texture::willDisplayFrame()
{
    std::swap(m_frontImage, m_displayImage);
}

bool AcceleratedBackingStoreDMABuf::Texture::prepareForRendering()
{
    if (!m_textureID)
        return false;

    gdk_gl_context_make_current(m_context.get());
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_displayImage);
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

#if USE(GBM)
AcceleratedBackingStoreDMABuf::Surface::Surface(const UnixFileDescriptor& backFD, const UnixFileDescriptor& frontFD, const UnixFileDescriptor& displayFD, const WebCore::IntSize& size, uint32_t format, uint32_t offset, uint32_t stride, float deviceScaleFactor)
    : RenderSource(size, deviceScaleFactor)
{
    auto* device = WebCore::PlatformDisplay::sharedDisplay().gbmDevice();
    if (!device) {
        WTFLogAlways("Failed to get GBM device");
        return;
    }

    struct gbm_import_fd_data fdData = { backFD.value(), static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), stride, format };
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
    fdData.fd = displayFD.value();
    m_displayBuffer = gbm_bo_import(device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING);
    if (!m_displayBuffer) {
        WTFLogAlways("Failed to import DMABuf with file descriptor %d", fdData.fd);
        gbm_bo_destroy(m_backBuffer);
        m_backBuffer = nullptr;
        gbm_bo_destroy(m_frontBuffer);
        m_frontBuffer = nullptr;
    }
}
#endif

AcceleratedBackingStoreDMABuf::Surface::Surface(RefPtr<ShareableBitmap>& backBitmap, RefPtr<ShareableBitmap>& frontBitmap, RefPtr<ShareableBitmap>& displayBitmap, float deviceScaleFactor)
    : RenderSource(backBitmap ? backBitmap->size() : WebCore::IntSize(), deviceScaleFactor)
    , m_backBitmap(backBitmap)
    , m_frontBitmap(frontBitmap)
    , m_displayBitmap(displayBitmap)
{
}

AcceleratedBackingStoreDMABuf::Surface::~Surface()
{
    m_surface = nullptr;

#if USE(GBM)
    if (m_backBuffer)
        gbm_bo_destroy(m_backBuffer);
    if (m_frontBuffer)
        gbm_bo_destroy(m_frontBuffer);
    if (m_displayBuffer)
        gbm_bo_destroy(m_displayBuffer);
#endif
}

#if USE(GBM)
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
#endif

RefPtr<cairo_surface_t> AcceleratedBackingStoreDMABuf::Surface::map(RefPtr<ShareableBitmap>& bitmap) const
{
    if (!bitmap)
        return nullptr;

    return bitmap->createCairoSurface();
}

void AcceleratedBackingStoreDMABuf::Surface::frame()
{
    if (m_backBitmap && m_frontBitmap)
        std::swap(m_backBitmap, m_frontBitmap);
#if USE(GBM)
    else
        std::swap(m_backBuffer, m_frontBuffer);
#endif
}

void AcceleratedBackingStoreDMABuf::Surface::willDisplayFrame()
{
    if (m_frontBitmap && m_displayBitmap)
        std::swap(m_frontBitmap, m_displayBitmap);
#if USE(GBM)
    else
        std::swap(m_frontBuffer, m_displayBuffer);
#endif
}

bool AcceleratedBackingStoreDMABuf::Surface::prepareForRendering()
{
    if (m_displayBitmap)
        m_surface = map(m_displayBitmap);
#if USE(GBM)
    else
        m_surface = map(m_displayBuffer);
#endif

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

void AcceleratedBackingStoreDMABuf::configure(UnixFileDescriptor&& backFD, UnixFileDescriptor&& frontFD, UnixFileDescriptor&& displayFD, const WebCore::IntSize& size, uint32_t format, uint32_t offset, uint32_t stride, uint64_t modifier)
{
    m_isSoftwareRast = false;
    m_surface.backFD = WTFMove(backFD);
    m_surface.frontFD = WTFMove(frontFD);
    m_surface.displayFD = WTFMove(displayFD);
    m_surface.size = size;
    m_surface.format = format;
    m_surface.offset = offset;
    m_surface.stride = stride;
    m_surface.modifier = modifier;
    if (gtk_widget_get_realized(m_webPage.viewWidget()))
        m_pendingSource = createSource();
}

void AcceleratedBackingStoreDMABuf::configureSHM(ShareableBitmap::Handle&& backBufferHandle, ShareableBitmap::Handle&& frontBufferHandle, ShareableBitmap::Handle&& displayBufferHandle)
{
    m_isSoftwareRast = true;
    m_surface.backBitmap = ShareableBitmap::create(WTFMove(backBufferHandle), SharedMemory::Protection::ReadOnly);
    m_surface.frontBitmap = ShareableBitmap::create(WTFMove(frontBufferHandle), SharedMemory::Protection::ReadOnly);
    m_surface.displayBitmap = ShareableBitmap::create(WTFMove(displayBufferHandle), SharedMemory::Protection::ReadOnly);
    if (gtk_widget_get_realized(m_webPage.viewWidget()))
        m_pendingSource = createSource();
}

std::unique_ptr<AcceleratedBackingStoreDMABuf::RenderSource> AcceleratedBackingStoreDMABuf::createSource()
{
    if (m_isSoftwareRast)
        return makeUnique<Surface>(m_surface.backBitmap, m_surface.frontBitmap, m_surface.displayBitmap, m_webPage.deviceScaleFactor());

#if USE(GBM)
    if (!WebCore::PlatformDisplay::sharedDisplay().gtkEGLDisplay())
        return makeUnique<Surface>(m_surface.backFD, m_surface.frontFD, m_surface.displayFD, m_surface.size, m_surface.format, m_surface.offset, m_surface.stride, m_webPage.deviceScaleFactor());
#endif

    ensureGLContext();
    return makeUnique<Texture>(m_gdkGLContext.get(), m_surface.backFD, m_surface.frontFD, m_surface.displayFD, m_surface.size, m_surface.format, m_surface.offset, m_surface.stride, m_surface.modifier, m_webPage.deviceScaleFactor());
}

void AcceleratedBackingStoreDMABuf::frame()
{
    ASSERT(!m_frameCompletionHandler);
    m_frameCompletionHandler = [this] {
        m_webPage.process().send(Messages::AcceleratedSurfaceDMABuf::FrameDone(), m_surface.id);
    };

    if (m_pendingSource)
        m_committedSource = WTFMove(m_pendingSource);

    if (m_isSoftwareRast)
        std::swap(m_surface.backBitmap, m_surface.frontBitmap);
    else
        std::swap(m_surface.backFD, m_surface.frontFD);

    if (m_committedSource) {
        m_committedSource->frame();
        gtk_widget_queue_draw(m_webPage.viewWidget());
    }
}

void AcceleratedBackingStoreDMABuf::willDisplayFrame()
{
    ASSERT(m_frameCompletionHandler);
    if (m_isSoftwareRast)
        std::swap(m_surface.frontBitmap, m_surface.displayBitmap);
    else
        std::swap(m_surface.frontFD, m_surface.displayFD);

    if (m_committedSource)
        m_committedSource->willDisplayFrame();
}

void AcceleratedBackingStoreDMABuf::frameDone()
{
    if (auto completionHandler = std::exchange(m_frameCompletionHandler, nullptr))
        completionHandler();
}

void AcceleratedBackingStoreDMABuf::realize()
{
    if (m_isSoftwareRast && !m_surface.backBitmap && !m_surface.frontBitmap)
        return;

    if (!m_isSoftwareRast && !m_surface.backFD && !m_surface.frontFD)
        return;

    m_committedSource = createSource();
    if (m_frameCompletionHandler || m_committedSource->prepareForRendering())
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

    if (!gdk_gl_context_realize(m_gdkGLContext.get(), &error.outPtr()))
        g_error("GDK failed to realize the GL context: %s.", error->message);
}

bool AcceleratedBackingStoreDMABuf::makeContextCurrent()
{
    if (!WebCore::PlatformDisplay::sharedDisplay().gtkEGLDisplay())
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
        if (m_frameCompletionHandler) {
            willDisplayFrame();
            frameDone();
        }
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

    if (m_frameCompletionHandler) {
        willDisplayFrame();
        m_committedSource->prepareForRendering();
    }

    m_committedSource->snapshot(gtkSnapshot);
    frameDone();
}
#else
bool AcceleratedBackingStoreDMABuf::paint(cairo_t* cr, const WebCore::IntRect& clipRect)
{
    if (!m_committedSource)
        return false;

    if (m_frameCompletionHandler) {
        willDisplayFrame();
        m_committedSource->prepareForRendering();
    }

    m_committedSource->paint(m_webPage.viewWidget(), cr, clipRect);
    frameDone();

    return true;
}
#endif

} // namespace WebKit
