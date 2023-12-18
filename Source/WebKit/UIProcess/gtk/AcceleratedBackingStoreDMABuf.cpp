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

#if USE(EGL)

#include "AcceleratedBackingStoreDMABufMessages.h"
#include "AcceleratedSurfaceDMABufMessages.h"
#include "DMABufRendererBufferMode.h"
#include "LayerTreeContext.h"
#include "ShareableBitmap.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformDisplay.h>
#include <epoxy/egl.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(GBM)
#include <drm_fourcc.h>
#include <gbm.h>

static constexpr uint64_t s_dmabufInvalidModifier = DRM_FORMAT_MOD_INVALID;
#else
static constexpr uint64_t s_dmabufInvalidModifier = ((1ULL << 56) - 1);
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

        const char* forceSHM = getenv("WEBKIT_DMABUF_RENDERER_FORCE_SHM");
        if (forceSHM && strcmp(forceSHM, "0"))
            return;

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

#if USE(GBM)
Vector<DMABufRendererBufferFormat> AcceleratedBackingStoreDMABuf::preferredBufferFormats()
{
    auto mode = rendererBufferMode();
    if (!mode.contains(DMABufRendererBufferMode::Hardware))
        return { };

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    const char* formatString = getenv("WEBKIT_DMABUF_RENDERER_BUFFER_FORMAT");
    if (formatString && *formatString) {
        auto tokens = String::fromUTF8(formatString).split(':');
        if (!tokens.isEmpty() && tokens[0].length() >= 2 && tokens[0].length() <= 4) {
            uint32_t format = fourcc_code(tokens[0][0], tokens[0][1], tokens[0].length() > 2 ? tokens[0][2] : ' ', tokens[0].length() > 3 ? tokens[0][3] : ' ');
            uint64_t modifier = tokens.size() > 1 ? g_ascii_strtoull(tokens[1].ascii().data(), nullptr, 10) : DRM_FORMAT_MOD_INVALID;
            return { { display.gtkEGLDisplay() ? DMABufRendererBufferFormat::Usage::Rendering : DMABufRendererBufferFormat::Usage::Mapping, format, { modifier } } };
        }

        WTFLogAlways("Invalid format %s set in WEBKIT_DMABUF_RENDERER_BUFFER_FORMAT, ignoring...", formatString);
    }

    if (!display.gtkEGLDisplay()) {
        return {
            { DMABufRendererBufferFormat::Usage::Mapping, DRM_FORMAT_ARGB8888, { DRM_FORMAT_MOD_LINEAR } },
            { DMABufRendererBufferFormat::Usage::Mapping, DRM_FORMAT_XRGB8888, { DRM_FORMAT_MOD_LINEAR } }
        };
    }

    return display.dmabufFormats().map([](const auto& format) -> DMABufRendererBufferFormat {
        return { DMABufRendererBufferFormat::Usage::Rendering, format.fourcc, format.modifiers };
    });
}
#endif

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
    if (m_surfaceID)
        m_webPage.process().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID);

    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        m_renderer = nullptr;
        gdk_gl_context_clear_current();
    }
}

AcceleratedBackingStoreDMABuf::Buffer::Buffer(uint64_t id, const WebCore::IntSize& size)
    : m_id(id)
    , m_size(size)
{
}

RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferEGLImage::create(uint64_t id, const WebCore::IntSize& size, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
{
    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    Vector<EGLAttrib> attributes = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(format)
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, fds[planeIndex].value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLAttrib>(offsets[planeIndex]), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLAttrib>(strides[planeIndex]) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (modifier != s_dmabufInvalidModifier && display.eglExtensions().EXT_image_dma_buf_import_modifiers) { \
        std::array<EGLAttrib, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLAttrib>(modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLAttrib>(modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLAttrib> { modifierAttributes }); \
    } \
    }

    auto planeCount = fds.size();
    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBUTES

    attributes.append(EGL_NONE);

    auto* image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!image) {
        WTFLogAlways("Failed to create EGL image from DMABuf of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    return adoptRef(*new BufferEGLImage(id, size, WTFMove(fds), image));
}

AcceleratedBackingStoreDMABuf::BufferEGLImage::BufferEGLImage(uint64_t id, const WebCore::IntSize& size, Vector<UnixFileDescriptor>&& fds, EGLImage image)
    : Buffer(id, size)
    , m_fds(WTFMove(fds))
    , m_image(image)
{
}

AcceleratedBackingStoreDMABuf::BufferEGLImage::~BufferEGLImage()
{
    WebCore::PlatformDisplay::sharedDisplay().destroyEGLImage(m_image);
}

#if USE(GBM)
RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferGBM::create(uint64_t id, const WebCore::IntSize& size, uint32_t format, UnixFileDescriptor&& fd, uint32_t stride)
{
    auto* device = WebCore::PlatformDisplay::sharedDisplay().gbmDevice();
    if (!device) {
        WTFLogAlways("Failed to get GBM device");
        return nullptr;
    }

    struct gbm_import_fd_data fdData = { fd.value(), static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height()), stride, format };
    auto* buffer = gbm_bo_import(device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
    if (!buffer) {
        WTFLogAlways("Failed to import DMABuf with file descriptor %d", fd.value());
        return nullptr;
    }

    return adoptRef(*new BufferGBM(id, size, WTFMove(fd), buffer));
}

AcceleratedBackingStoreDMABuf::BufferGBM::BufferGBM(uint64_t id, const WebCore::IntSize& size, UnixFileDescriptor&& fd, struct gbm_bo* buffer)
    : Buffer(id, size)
    , m_fd(WTFMove(fd))
    , m_buffer(buffer)
{
}

AcceleratedBackingStoreDMABuf::BufferGBM::~BufferGBM()
{
    gbm_bo_destroy(m_buffer);
}

RefPtr<cairo_surface_t> AcceleratedBackingStoreDMABuf::BufferGBM::surface() const
{
    uint32_t mapStride = 0;
    void* mapData = nullptr;
    void* map = gbm_bo_map(m_buffer, 0, 0, static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), GBM_BO_TRANSFER_READ, &mapStride, &mapData);
    if (!map)
        return nullptr;

    auto cairoFormat = gbm_bo_get_format(m_buffer) == DRM_FORMAT_ARGB8888 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(map), cairoFormat, m_size.width(), m_size.height(), mapStride));
    struct BufferData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        RefPtr<BufferGBM> buffer;
        void* data;
    };
    auto bufferData = makeUnique<BufferData>(BufferData { const_cast<BufferGBM*>(this), mapData });
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, bufferData.release(), [](void* data) {
        std::unique_ptr<BufferData> bufferData(static_cast<BufferData*>(data));
        gbm_bo_unmap(bufferData->buffer->m_buffer, bufferData->data);
    });
    return surface;
}
#endif

RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferSHM::create(uint64_t id, RefPtr<ShareableBitmap>&& bitmap)
{
    if (!bitmap)
        return nullptr;

    return adoptRef(*new BufferSHM(id, WTFMove(bitmap)));
}

AcceleratedBackingStoreDMABuf::BufferSHM::BufferSHM(uint64_t id, RefPtr<ShareableBitmap>&& bitmap)
    : Buffer(id, bitmap->size())
    , m_bitmap(WTFMove(bitmap))
{
}

RefPtr<cairo_surface_t> AcceleratedBackingStoreDMABuf::BufferSHM::surface() const
{
    return m_bitmap->createCairoSurface();
}

bool AcceleratedBackingStoreDMABuf::Renderer::setBuffer(Buffer& buffer, float deviceScaleFactor)
{
    m_size = buffer.size();
    m_deviceScaleFactor = deviceScaleFactor;

    // We keep a reference of the committed buffer in the renderer because it might
    // belong to a previous surface that we want to still render to avoid flickering,
    // but we don't want to release it.
    if (m_buffer == &buffer)
        return false;

    m_buffer = &buffer;
    return true;
}

AcceleratedBackingStoreDMABuf::RendererGL::RendererGL()
{
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

AcceleratedBackingStoreDMABuf::RendererGL::~RendererGL()
{
#if USE(GTK4)
    m_texture[0] = nullptr;
    m_texture[1] = nullptr;
#endif

    if (m_textureID)
        glDeleteTextures(1, &m_textureID);
}

bool AcceleratedBackingStoreDMABuf::RendererGL::setBuffer(Buffer& buffer, float deviceScaleFactor)
{
    if (!Renderer::setBuffer(buffer, deviceScaleFactor))
        return false;

    ASSERT(buffer.type() == Buffer::Type::EglImage);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<const BufferEGLImage&>(buffer).image());

#if USE(GTK4)
    if (!m_texture[0] || gdk_texture_get_width(m_texture[0].get()) != m_size.width() || gdk_texture_get_height(m_texture[0].get()) != m_size.height()) {
        auto* context = gdk_gl_context_get_current();
        m_texture[0] = adoptGRef(gdk_gl_texture_new(context, m_textureID, m_size.width(), m_size.height(), nullptr, nullptr));
        m_texture[1] = adoptGRef(gdk_gl_texture_new(context, m_textureID, m_size.width(), m_size.height(), nullptr, nullptr));
        m_textureIndex = 0;
    }  else
        m_textureIndex = !m_textureIndex;
#endif

    return true;
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::RendererGL::snapshot(GtkSnapshot* gtkSnapshot) const
{
    if (!m_textureID)
        return;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(m_size.width() / m_deviceScaleFactor), static_cast<float>(m_size.height() / m_deviceScaleFactor));
    gtk_snapshot_append_texture(gtkSnapshot, m_texture[m_textureIndex].get(), &bounds);
}
#else
void AcceleratedBackingStoreDMABuf::RendererGL::paint(GtkWidget* widget, cairo_t* cr, const WebCore::IntRect&) const
{
    if (!m_textureID)
        return;

    cairo_save(cr);
    gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(widget), m_textureID, GL_TEXTURE, m_deviceScaleFactor, 0, 0, m_size.width(), m_size.height());
    cairo_restore(cr);
}
#endif

bool AcceleratedBackingStoreDMABuf::RendererCairo::setBuffer(Buffer& buffer, float deviceScaleFactor)
{
    if (!Renderer::setBuffer(buffer, deviceScaleFactor))
        return false;

    switch (buffer.type()) {
#if USE(GBM)
    case Buffer::Type::Gbm:
        m_surface = static_cast<const BufferGBM&>(buffer).surface();
        break;
#endif
    case Buffer::Type::SharedMemory:
        m_surface = static_cast<const BufferSHM&>(buffer).surface();
        break;
    case Buffer::Type::EglImage:
        RELEASE_ASSERT_NOT_REACHED();
    }
    if (m_surface)
        cairo_surface_set_device_scale(m_surface.get(), m_deviceScaleFactor, m_deviceScaleFactor);

    return true;
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::RendererCairo::snapshot(GtkSnapshot* gtkSnapshot) const
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
void AcceleratedBackingStoreDMABuf::RendererCairo::paint(GtkWidget* widget, cairo_t* cr, const WebCore::IntRect& clipRect) const
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

void AcceleratedBackingStoreDMABuf::didCreateBuffer(uint64_t id, const WebCore::IntSize& size, uint32_t format, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
{
#if USE(GBM)
    if (!WebCore::PlatformDisplay::sharedDisplay().gtkEGLDisplay()) {
        ASSERT(fds.size() == 1 && strides.size() == 1);
        if (auto buffer = BufferGBM::create(id, size, format, WTFMove(fds[0]), strides[0]))
            m_buffers.add(id, WTFMove(buffer));
        return;
    }
#endif

    if (auto buffer = BufferEGLImage::create(id, size, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier))
        m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStoreDMABuf::didCreateBufferSHM(uint64_t id, ShareableBitmap::Handle&& handle)
{
    if (auto buffer = BufferSHM::create(id, ShareableBitmap::create(WTFMove(handle), SharedMemory::Protection::ReadOnly)))
        m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStoreDMABuf::didDestroyBuffer(uint64_t id)
{
    m_buffers.remove(id);
}

void AcceleratedBackingStoreDMABuf::ensureRenderer()
{
    switch (m_committedBuffer->type()) {
    case Buffer::Type::EglImage:
        ensureGLContext();
        gdk_gl_context_make_current(m_gdkGLContext.get());
        if (!m_renderer)
            m_renderer = makeUnique<RendererGL>();
        ASSERT(m_renderer->type() == Renderer::Type::Gl);
        break;
#if USE(GBM)
    case Buffer::Type::Gbm:
#endif
    case Buffer::Type::SharedMemory:
        if (!m_renderer)
            m_renderer = makeUnique<RendererCairo>();
        ASSERT(m_renderer->type() == Renderer::Type::Cairo);
        break;
    }
}

void AcceleratedBackingStoreDMABuf::frame(uint64_t bufferID)
{
    ASSERT(!m_pendingBuffer);
    auto* buffer = m_buffers.get(bufferID);
    if (!buffer) {
        frameDone();
        return;
    }

    m_pendingBuffer = buffer;
    gtk_widget_queue_draw(m_webPage.viewWidget());
}

void AcceleratedBackingStoreDMABuf::frameDone()
{
    m_webPage.process().send(Messages::AcceleratedSurfaceDMABuf::FrameDone(), m_surfaceID);
}

void AcceleratedBackingStoreDMABuf::unrealize()
{
    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        m_renderer = nullptr;
        gdk_gl_context_clear_current();
        m_gdkGLContext = nullptr;
    } else
        m_renderer = nullptr;
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

void AcceleratedBackingStoreDMABuf::update(const LayerTreeContext& context)
{
    if (m_surfaceID == context.contextID)
        return;

    if (m_surfaceID) {
        if (m_pendingBuffer) {
            frameDone();
            m_pendingBuffer = nullptr;
        }
        // Clear the committed buffer that belongs to this surface to avoid releasing it
        // on the new surface. The renderer still keeps a reference to keep using it and
        // avoid flickering.
        m_committedBuffer = nullptr;
        m_buffers.clear();
        m_webPage.process().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID);
    }

    m_surfaceID = context.contextID;
    if (m_surfaceID)
        m_webPage.process().addMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID, *this);
}

bool AcceleratedBackingStoreDMABuf::prepareForRendering()
{
    if (m_gdkGLContext)
        gdk_gl_context_make_current(m_gdkGLContext.get());

    if (m_pendingBuffer) {
        if (m_committedBuffer)
            m_webPage.process().send(Messages::AcceleratedSurfaceDMABuf::ReleaseBuffer(m_committedBuffer->id()), m_surfaceID);
        m_committedBuffer = WTFMove(m_pendingBuffer);
    }

    if (m_committedBuffer) {
        ensureRenderer();
        m_renderer->setBuffer(*m_committedBuffer, m_webPage.deviceScaleFactor());
        return true;
    }

    return m_renderer && m_renderer->buffer();
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::snapshot(GtkSnapshot* gtkSnapshot)
{
    bool framePending = !!m_pendingBuffer;
    if (!prepareForRendering())
        return;

    m_renderer->snapshot(gtkSnapshot);
    if (framePending)
        frameDone();
}
#else
bool AcceleratedBackingStoreDMABuf::paint(cairo_t* cr, const WebCore::IntRect& clipRect)
{
    bool framePending = !!m_pendingBuffer;
    if (!prepareForRendering())
        return false;

    m_renderer->paint(m_webPage.viewWidget(), cr, clipRect);
    if (framePending)
        frameDone();

    return true;
}
#endif

} // namespace WebKit

#endif // USE(EGL)
