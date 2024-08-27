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
#include "DRMDevice.h"
#include "Display.h"
#include "LayerTreeContext.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SharedMemory.h>
#include <epoxy/egl.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

#if USE(GBM)
#include <WebCore/DRMDeviceManager.h>
#include <gbm.h>

static constexpr uint64_t s_dmabufInvalidModifier = DRM_FORMAT_MOD_INVALID;
#else
static constexpr uint64_t s_dmabufInvalidModifier = ((1ULL << 56) - 1);
#endif

#if PLATFORM(X11) && USE(GTK4)
#include <gdk/x11/gdkx.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedBackingStoreDMABuf);

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

        if (auto* glDisplay = Display::singleton().glDisplay()) {
            const auto& eglExtensions = glDisplay->extensions();
            if (eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import)
                mode.add(DMABufRendererBufferMode::Hardware);
        }
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

    auto& display = Display::singleton();
    const char* formatString = getenv("WEBKIT_DMABUF_RENDERER_BUFFER_FORMAT");
    if (formatString && *formatString) {
        auto tokens = String::fromUTF8(formatString).split(':');
        if (!tokens.isEmpty() && tokens[0].length() >= 2 && tokens[0].length() <= 4) {
            DMABufRendererBufferFormat format;
            format.usage = display.glDisplayIsSharedWithGtk() ? DMABufRendererBufferFormat::Usage::Rendering : DMABufRendererBufferFormat::Usage::Mapping;
            format.drmDevice = drmRenderNodeDevice().utf8();
            uint32_t fourcc = fourcc_code(tokens[0][0], tokens[0][1], tokens[0].length() > 2 ? tokens[0][2] : ' ', tokens[0].length() > 3 ? tokens[0][3] : ' ');
            char* endptr = nullptr;
            uint64_t modifier = tokens.size() > 1 ? g_ascii_strtoull(tokens[1].ascii().data(), &endptr, 16) : DRM_FORMAT_MOD_INVALID;
            if (!(modifier == G_MAXUINT64 && errno == ERANGE) && !(!modifier && !endptr)) {
                format.formats.append({ fourcc, { modifier } });
                return { WTFMove(format) };
            }
        }

        WTFLogAlways("Invalid format %s set in WEBKIT_DMABUF_RENDERER_BUFFER_FORMAT, ignoring...", formatString);
    }

    if (!display.glDisplayIsSharedWithGtk()) {
        DMABufRendererBufferFormat format;
        format.usage = DMABufRendererBufferFormat::Usage::Mapping;
        format.drmDevice = drmRenderNodeDevice().utf8();
        format.formats.append({ DRM_FORMAT_XRGB8888, { DRM_FORMAT_MOD_LINEAR } });
        format.formats.append({ DRM_FORMAT_ARGB8888, { DRM_FORMAT_MOD_LINEAR } });
        return { WTFMove(format) };
    }

    RELEASE_ASSERT(display.glDisplay());

    DMABufRendererBufferFormat format;
    format.usage = DMABufRendererBufferFormat::Usage::Rendering;
    format.drmDevice = drmRenderNodeDevice().utf8();
    format.formats = display.glDisplay()->dmabufFormats().map([](const auto& format) -> DMABufRendererBufferFormat::Format {
        return { format.fourcc, format.modifiers };
    });
    return { WTFMove(format) };
}
#endif

std::unique_ptr<AcceleratedBackingStoreDMABuf> AcceleratedBackingStoreDMABuf::create(WebPageProxy& webPage)
{
    ASSERT(checkRequirements());
    return std::unique_ptr<AcceleratedBackingStoreDMABuf>(new AcceleratedBackingStoreDMABuf(webPage));
}

AcceleratedBackingStoreDMABuf::AcceleratedBackingStoreDMABuf(WebPageProxy& webPage)
    : AcceleratedBackingStore(webPage)
    , m_fenceMonitor([this] { gtk_widget_queue_draw(m_webPage.viewWidget()); })
{
}

AcceleratedBackingStoreDMABuf::~AcceleratedBackingStoreDMABuf()
{
    if (m_surfaceID)
        m_webPage.legacyMainFrameProcess().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID);

    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        m_committedBuffer = nullptr;
        gdk_gl_context_clear_current();
    }
}

AcceleratedBackingStoreDMABuf::Buffer::Buffer(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage)
    : m_webPage(&webPage)
    , m_id(id)
    , m_surfaceID(surfaceID)
    , m_size(size)
    , m_usage(usage)
{
}

float AcceleratedBackingStoreDMABuf::Buffer::deviceScaleFactor() const
{
    return m_webPage ? m_webPage->deviceScaleFactor() : 1;
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::Buffer::snapshot(GtkSnapshot* gtkSnapshot) const
{
    if (!m_webPage)
        return;

    WebCore::FloatSize unscaledSize = m_size;
    unscaledSize.scale(1. / m_webPage->deviceScaleFactor());
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, unscaledSize.width(), unscaledSize.height());

    if (auto* texture = this->texture()) {
        gtk_snapshot_append_texture(gtkSnapshot, texture, &bounds);
        return;
    }

    if (auto* surface = this->surface()) {
        RefPtr<cairo_t> cr = adoptRef(gtk_snapshot_append_cairo(gtkSnapshot, &bounds));
        cairo_set_source_surface(cr.get(), surface, 0, 0);
        cairo_set_operator(cr.get(), CAIRO_OPERATOR_OVER);
        cairo_paint(cr.get());
    }
}
#else
void AcceleratedBackingStoreDMABuf::Buffer::paint(cairo_t* cr, const WebCore::IntRect& clipRect) const
{
    if (!m_webPage)
        return;

    if (auto textureID = this->textureID()) {
        cairo_save(cr);
        gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(m_webPage->viewWidget()), textureID, GL_TEXTURE, m_webPage->deviceScaleFactor(), 0, 0, m_size.width(), m_size.height());
        cairo_restore(cr);
        return;
    }

    if (auto* surface = this->surface()) {
        cairo_save(cr);
        cairo_matrix_t transform;
        cairo_matrix_init(&transform, 1, 0, 0, -1, 0, static_cast<float>(m_size.height() / m_webPage->deviceScaleFactor()));
        cairo_transform(cr, &transform);
        cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_fill(cr);
        cairo_restore(cr);
    }
}
#endif

void AcceleratedBackingStoreDMABuf::Buffer::didRelease() const
{
    if (!m_surfaceID || !m_webPage)
        return;

    m_webPage->legacyMainFrameProcess().send(Messages::AcceleratedSurfaceDMABuf::ReleaseBuffer(m_id, { }), m_surfaceID);
}

#if GTK_CHECK_VERSION(4, 13, 4)
RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferDMABuf::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
{
    GRefPtr<GdkDmabufTextureBuilder> builder = adoptGRef(gdk_dmabuf_texture_builder_new());
    gdk_dmabuf_texture_builder_set_display(builder.get(), gtk_widget_get_display(webPage.viewWidget()));
    gdk_dmabuf_texture_builder_set_width(builder.get(), size.width());
    gdk_dmabuf_texture_builder_set_height(builder.get(), size.height());
    gdk_dmabuf_texture_builder_set_fourcc(builder.get(), format);
    gdk_dmabuf_texture_builder_set_modifier(builder.get(), modifier);
    auto planeCount = fds.size();
    gdk_dmabuf_texture_builder_set_n_planes(builder.get(), planeCount);
    for (unsigned i = 0; i < planeCount; ++i) {
        gdk_dmabuf_texture_builder_set_fd(builder.get(), i, fds[i].value());
        gdk_dmabuf_texture_builder_set_stride(builder.get(), i, strides[i]);
        gdk_dmabuf_texture_builder_set_offset(builder.get(), i, offsets[i]);
    }

    return adoptRef(*new BufferDMABuf(webPage, id, surfaceID, size, usage, WTFMove(fds), WTFMove(builder)));
}

AcceleratedBackingStoreDMABuf::BufferDMABuf::BufferDMABuf(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage, Vector<UnixFileDescriptor>&& fds, GRefPtr<GdkDmabufTextureBuilder>&& builder)
    : Buffer(webPage, id, surfaceID, size, usage)
    , m_fds(WTFMove(fds))
    , m_builder(WTFMove(builder))
{
}

void AcceleratedBackingStoreDMABuf::BufferDMABuf::didUpdateContents(Buffer* previousBuffer, const WebCore::Region& damageRegion)
{
    if (!damageRegion.isEmpty() && previousBuffer && previousBuffer->texture()) {
        gdk_dmabuf_texture_builder_set_update_texture(m_builder.get(), previousBuffer->texture());
        RefPtr<cairo_region_t> region = adoptRef(cairo_region_create());
        for (const auto& rect : damageRegion.rects()) {
            cairo_rectangle_int_t cairoRect = rect;
            cairo_region_union_rectangle(region.get(), &cairoRect);
        }
        gdk_dmabuf_texture_builder_set_update_region(m_builder.get(), region.get());
    } else {
        gdk_dmabuf_texture_builder_set_update_texture(m_builder.get(), nullptr);
        gdk_dmabuf_texture_builder_set_update_region(m_builder.get(), nullptr);
    }

    GUniqueOutPtr<GError> error;
    m_texture = adoptGRef(gdk_dmabuf_texture_builder_build(m_builder.get(), nullptr, nullptr, &error.outPtr()));
    if (!m_texture)
        WTFLogAlways("Failed to create DMA-BUF texture of size %dx%d: %s", m_size.width(), m_size.height(), error->message);
}

RendererBufferFormat AcceleratedBackingStoreDMABuf::BufferDMABuf::format() const
{
    return { RendererBufferFormat::Type::DMABuf, m_usage, gdk_dmabuf_texture_builder_get_fourcc(m_builder.get()), gdk_dmabuf_texture_builder_get_modifier(m_builder.get()) };
}

void AcceleratedBackingStoreDMABuf::BufferDMABuf::release()
{
    m_texture = nullptr;
    didRelease();
}
#endif

RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferEGLImage::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
{
    auto* glDisplay = Display::singleton().glDisplay();
    if (!glDisplay)
        return nullptr;

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
    if (modifier != s_dmabufInvalidModifier && glDisplay->extensions().EXT_image_dma_buf_import_modifiers) { \
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

    auto* image = glDisplay->createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!image) {
        WTFLogAlways("Failed to create EGL image from DMABuf of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    return adoptRef(*new BufferEGLImage(webPage, id, surfaceID, size, usage, format, WTFMove(fds), modifier, image));
}

AcceleratedBackingStoreDMABuf::BufferEGLImage::BufferEGLImage(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage, uint32_t format, Vector<UnixFileDescriptor>&& fds, uint64_t modifier, EGLImage image)
    : Buffer(webPage, id, surfaceID, size, usage)
    , m_fds(WTFMove(fds))
    , m_image(image)
    , m_fourcc(format)
    , m_modifier(modifier)
{
}

AcceleratedBackingStoreDMABuf::BufferEGLImage::~BufferEGLImage()
{
    if (auto* glDisplay = Display::singleton().glDisplay())
        glDisplay->destroyImage(m_image);

#if !USE(GTK4)
    if (m_textureID)
        glDeleteTextures(1, &m_textureID);
#endif
}

#if USE(GTK4)
struct Texture {
    Texture()
        : context(gdk_gl_context_get_current())
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    ~Texture()
    {
        gdk_gl_context_make_current(context.get());
        glDeleteTextures(1, &id);
    }

    GRefPtr<GdkGLContext> context;
    unsigned id { 0 };
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(Texture)

void AcceleratedBackingStoreDMABuf::BufferEGLImage::didUpdateContents(Buffer*, const WebCore::Region&)
{
    auto* texture = createTexture();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);

    m_texture = adoptGRef(gdk_gl_texture_new(texture->context.get(), texture->id, m_size.width(), m_size.height(), [](gpointer userData) {
        destroyTexture(static_cast<Texture*>(userData));
    }, texture));
}
#else
void AcceleratedBackingStoreDMABuf::BufferEGLImage::didUpdateContents(Buffer*, const WebCore::Region&)
{
    if (m_textureID)
        return;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);
}
#endif

RendererBufferFormat AcceleratedBackingStoreDMABuf::BufferEGLImage::format() const
{
    return { RendererBufferFormat::Type::DMABuf, m_usage, m_fourcc, m_modifier };
}

void AcceleratedBackingStoreDMABuf::BufferEGLImage::release()
{
#if USE(GTK4)
    m_texture = nullptr;
#endif
    didRelease();
}

#if USE(GBM)
RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferGBM::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage, uint32_t format, UnixFileDescriptor&& fd, uint32_t stride)
{
    auto& manager = WebCore::DRMDeviceManager::singleton();
    if (!manager.isInitialized())
        manager.initializeMainDevice(drmRenderNodeDevice());
    auto* device = manager.mainGBMDeviceNode(WebCore::DRMDeviceManager::NodeType::Render);
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

    return adoptRef(*new BufferGBM(webPage, id, surfaceID, size, usage, WTFMove(fd), buffer));
}

AcceleratedBackingStoreDMABuf::BufferGBM::BufferGBM(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const WebCore::IntSize& size, DMABufRendererBufferFormat::Usage usage, UnixFileDescriptor&& fd, struct gbm_bo* buffer)
    : Buffer(webPage, id, surfaceID, size, usage)
    , m_fd(WTFMove(fd))
    , m_buffer(buffer)
{
}

AcceleratedBackingStoreDMABuf::BufferGBM::~BufferGBM()
{
    gbm_bo_destroy(m_buffer);
}

void AcceleratedBackingStoreDMABuf::BufferGBM::didUpdateContents(Buffer*, const WebCore::Region&)
{
    uint32_t mapStride = 0;
    void* mapData = nullptr;
    void* map = gbm_bo_map(m_buffer, 0, 0, static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), GBM_BO_TRANSFER_READ, &mapStride, &mapData);
    if (!map)
        return;

    auto cairoFormat = gbm_bo_get_format(m_buffer) == DRM_FORMAT_ARGB8888 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    m_surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(map), cairoFormat, m_size.width(), m_size.height(), mapStride));
    cairo_surface_set_device_scale(m_surface.get(), deviceScaleFactor(), deviceScaleFactor());
    struct BufferData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        RefPtr<BufferGBM> buffer;
        void* data;
    };
    auto bufferData = makeUnique<BufferData>(BufferData { const_cast<BufferGBM*>(this), mapData });
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(m_surface.get(), &s_surfaceDataKey, bufferData.release(), [](void* data) {
        std::unique_ptr<BufferData> bufferData(static_cast<BufferData*>(data));
        gbm_bo_unmap(bufferData->buffer->m_buffer, bufferData->data);
    });
}

RendererBufferFormat AcceleratedBackingStoreDMABuf::BufferGBM::format() const
{
    return { RendererBufferFormat::Type::DMABuf, m_usage, gbm_bo_get_format(m_buffer), gbm_bo_get_modifier(m_buffer) };
}

void AcceleratedBackingStoreDMABuf::BufferGBM::release()
{
    m_surface = nullptr;
    didRelease();
}
#endif

RefPtr<AcceleratedBackingStoreDMABuf::Buffer> AcceleratedBackingStoreDMABuf::BufferSHM::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, RefPtr<WebCore::ShareableBitmap>&& bitmap)
{
    if (!bitmap)
        return nullptr;

    return adoptRef(*new BufferSHM(webPage, id, surfaceID, WTFMove(bitmap)));
}

AcceleratedBackingStoreDMABuf::BufferSHM::BufferSHM(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, RefPtr<WebCore::ShareableBitmap>&& bitmap)
    : Buffer(webPage, id, surfaceID, bitmap->size(), DMABufRendererBufferFormat::Usage::Rendering)
    , m_bitmap(WTFMove(bitmap))
{
}

void AcceleratedBackingStoreDMABuf::BufferSHM::didUpdateContents(Buffer*, const WebCore::Region&)
{
#if USE(CAIRO)
    m_surface = m_bitmap->createCairoSurface();
#elif USE(SKIA)
    m_surface = cairo_image_surface_create_for_data(m_bitmap->mutableSpan().data(), CAIRO_FORMAT_ARGB32, m_size.width(), m_size.height(), m_bitmap->bytesPerRow());
    m_bitmap->ref();
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(m_surface.get(), &s_surfaceDataKey, m_bitmap.get(), [](void* userData) {
        static_cast<WebCore::ShareableBitmap*>(userData)->deref();
    });
#endif
    cairo_surface_set_device_scale(m_surface.get(), deviceScaleFactor(), deviceScaleFactor());
}

RendererBufferFormat AcceleratedBackingStoreDMABuf::BufferSHM::format() const
{
#if USE(LIBDRM)
    return { RendererBufferFormat::Type::SharedMemory, m_usage, DRM_FORMAT_ARGB8888, 0 };
#else
    return { };
#endif
}

void AcceleratedBackingStoreDMABuf::BufferSHM::release()
{
    m_surface = nullptr;
    didRelease();
}

void AcceleratedBackingStoreDMABuf::didCreateBuffer(uint64_t id, const WebCore::IntSize& size, uint32_t format, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, DMABufRendererBufferFormat::Usage usage)
{
#if USE(GBM)
    if (!Display::singleton().glDisplayIsSharedWithGtk()) {
        ASSERT(fds.size() == 1 && strides.size() == 1);
        if (auto buffer = BufferGBM::create(m_webPage, id, m_surfaceID, size, usage, format, WTFMove(fds[0]), strides[0]))
            m_buffers.add(id, WTFMove(buffer));
        return;
    }
#endif

#if GTK_CHECK_VERSION(4, 13, 4)
    if (auto buffer = BufferDMABuf::create(m_webPage, id, m_surfaceID, size, usage, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier)) {
        m_buffers.add(id, WTFMove(buffer));
        return;
    }
#endif

    if (auto buffer = BufferEGLImage::create(m_webPage, id, m_surfaceID, size, usage, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier))
        m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStoreDMABuf::didCreateBufferSHM(uint64_t id, WebCore::ShareableBitmap::Handle&& handle)
{
    if (auto buffer = BufferSHM::create(m_webPage, id, m_surfaceID, WebCore::ShareableBitmap::create(WTFMove(handle), WebCore::SharedMemory::Protection::ReadOnly)))
        m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStoreDMABuf::didDestroyBuffer(uint64_t id)
{
    m_buffers.remove(id);
}

void AcceleratedBackingStoreDMABuf::frame(uint64_t bufferID, WebCore::Region&& damageRegion, WTF::UnixFileDescriptor&& renderingFenceFD)
{
    ASSERT(!m_pendingBuffer);
    auto* buffer = m_buffers.get(bufferID);
    if (!buffer) {
        frameDone();
        return;
    }

    m_pendingBuffer = buffer;
    m_pendingDamageRegion = WTFMove(damageRegion);
    m_fenceMonitor.addFileDescriptor(WTFMove(renderingFenceFD));
}

void AcceleratedBackingStoreDMABuf::frameDone()
{
    m_webPage.legacyMainFrameProcess().send(Messages::AcceleratedSurfaceDMABuf::FrameDone(), m_surfaceID);
}

void AcceleratedBackingStoreDMABuf::unrealize()
{
    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        m_committedBuffer = nullptr;
        gdk_gl_context_clear_current();
        m_gdkGLContext = nullptr;
    } else
        m_committedBuffer = nullptr;
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
            m_pendingDamageRegion = { };
        }

        while (!m_buffers.isEmpty()) {
            auto buffer = m_buffers.takeFirst();
            buffer->setSurfaceID(0);
        }

        m_webPage.legacyMainFrameProcess().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID);
    }

    m_surfaceID = context.contextID;
    if (m_surfaceID)
        m_webPage.legacyMainFrameProcess().addMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID, *this);
}

bool AcceleratedBackingStoreDMABuf::swapBuffersIfNeeded()
{
    if (!m_pendingBuffer || m_fenceMonitor.hasFileDescriptor())
        return false;

    if (m_pendingBuffer->type() == Buffer::Type::EglImage) {
        ensureGLContext();
        gdk_gl_context_make_current(m_gdkGLContext.get());
    }
    m_pendingBuffer->didUpdateContents(m_committedBuffer.get(), m_pendingDamageRegion);
    m_pendingDamageRegion = { };

    if (m_committedBuffer)
        m_committedBuffer->release();

    m_committedBuffer = WTFMove(m_pendingBuffer);
    return true;
}

#if USE(GTK4)
void AcceleratedBackingStoreDMABuf::snapshot(GtkSnapshot* gtkSnapshot)
{
    bool didSwapBuffers = swapBuffersIfNeeded();
    if (!m_committedBuffer)
        return;

    m_committedBuffer->snapshot(gtkSnapshot);
    if (didSwapBuffers)
        frameDone();
}
#else
bool AcceleratedBackingStoreDMABuf::paint(cairo_t* cr, const WebCore::IntRect& clipRect)
{
    bool didSwapBuffers = swapBuffersIfNeeded();
    if (!m_committedBuffer)
        return false;

    m_committedBuffer->paint(cr, clipRect);
    if (didSwapBuffers)
        frameDone();

    return true;
}
#endif

RendererBufferFormat AcceleratedBackingStoreDMABuf::bufferFormat() const
{
    auto* buffer = m_committedBuffer ? m_committedBuffer.get() : m_pendingBuffer.get();
    return buffer ? buffer->format() : RendererBufferFormat { };
}

} // namespace WebKit
