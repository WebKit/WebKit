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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEBufferDMABuf.h"

#include "WPEDisplayPrivate.h"
#include <epoxy/egl.h>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
#include <drm_fourcc.h>
#include <gbm.h>
#endif

/**
 * WPEBufferDMABuf:
 *
 */
struct _WPEBufferDMABufPrivate {
    uint32_t format;
    Vector<UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;
    uint64_t modifier;
    EGLImage eglImage;
    UnixFileDescriptor renderingFence;
    UnixFileDescriptor releaseFence;
#if USE(GBM)
    UnixFileDescriptor deviceFD;
    std::optional<struct gbm_device*> device;
    struct gbm_bo* bufferObject;
    GRefPtr<GBytes> pixels;
#endif
};
WEBKIT_DEFINE_FINAL_TYPE(WPEBufferDMABuf, wpe_buffer_dma_buf, WPE_TYPE_BUFFER, WPEBuffer)

static void wpeBufferDMABufDisposeEGLImageIfNeeded(WPEBufferDMABuf* buffer)
{
    auto* priv = buffer->priv;
    if (!priv->eglImage)
        return;

    auto* eglImage = std::exchange(priv->eglImage, nullptr);
    auto* display = wpe_buffer_get_display(WPE_BUFFER(buffer));
    if (!display)
        return;

    if (auto* eglDisplay = wpe_display_get_egl_display(display, nullptr)) {
        static PFNEGLDESTROYIMAGEPROC s_eglDestroyImageKHR;
        if (!s_eglDestroyImageKHR)
            s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEPROC>(epoxy_eglGetProcAddress("eglDestroyImageKHR"));
        s_eglDestroyImageKHR(eglDisplay, eglImage);
    }
}

static void wpeBufferDMABufDispose(GObject* object)
{
    auto* dmabufBuffer = WPE_BUFFER_DMA_BUF(object);
    wpeBufferDMABufDisposeEGLImageIfNeeded(WPE_BUFFER_DMA_BUF(object));

#if USE(GBM)
    auto* priv = dmabufBuffer->priv;
    priv->pixels = nullptr;
    g_clear_pointer(&priv->bufferObject, gbm_bo_destroy);
    if (priv->device && priv->device.has_value()) {
        gbm_device_destroy(priv->device.value());
        priv->device = std::nullopt;
    }
    priv->deviceFD = { };
#endif

    G_OBJECT_CLASS(wpe_buffer_dma_buf_parent_class)->dispose(object);
}

static gpointer wpeBufferDMABufImportToEGLImage(WPEBuffer* buffer, GError** error)
{
    auto* priv = WPE_BUFFER_DMA_BUF(buffer)->priv;
    auto* display = wpe_buffer_get_display(buffer);
    if (!display) {
        priv->eglImage = nullptr;
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "The WPE display of buffer has already been closed");
        return nullptr;
    }

    if (priv->eglImage)
        return priv->eglImage;

    GUniqueOutPtr<GError> eglError;
    auto* eglDisplay = wpe_display_get_egl_display(display, &eglError.outPtr());
    if (eglDisplay == EGL_NO_DISPLAY) {
        g_set_error(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to get EGLDisplay when importing buffer to EGL image: %s", eglError->message);
        return nullptr;
    }

    // Epoxy requires a current context for the symbol resolver to work automatically.
    static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR;
    if (!s_eglCreateImageKHR) {
        if (epoxy_has_egl_extension(eglDisplay, "EGL_KHR_image_base"))
            s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(epoxy_eglGetProcAddress("eglCreateImageKHR"));
    }
    if (!s_eglCreateImageKHR) {
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to EGL image: eglCreateImageKHR not found");
        return nullptr;
    }

    Vector<EGLint> attributes = {
        EGL_WIDTH, wpe_buffer_get_width(buffer),
        EGL_HEIGHT, wpe_buffer_get_height(buffer),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(priv->format)
    };

    static const uint64_t invalidModifier = ((1ULL << 56) - 1);
#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, priv->fds[planeIndex].value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLint>(priv->offsets[planeIndex]), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLint>(priv->strides[planeIndex]) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (priv->modifier != invalidModifier && wpeDisplayCheckEGLExtension(display, "EXT_image_dma_buf_import_modifiers")) { \
        std::array<EGLint, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLint>(priv->modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLint>(priv->modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLint> { modifierAttributes }); \
    } \
    }

    auto planeCount = priv->fds.size();
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
    priv->eglImage = s_eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes.span().data());
    if (!priv->eglImage)
        g_set_error(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to EGL image: eglCreateImageKHR failed with error %#04x", eglGetError());
    return priv->eglImage;
}

#if USE(GBM)
static bool wpeBufferDMABufTryEnsureGBMDevice(WPEBufferDMABuf* buffer)
{
    auto* priv = buffer->priv;
    if (priv->device.has_value())
        return !!priv->device.value();

    priv->device = nullptr;
    auto* display = wpe_buffer_get_display(WPE_BUFFER(buffer));
    if (!display)
        return false;

    const char* filename = wpe_display_get_drm_render_node(display);
    if (!filename)
        return false;

    UnixFileDescriptor fd = UnixFileDescriptor { open(filename, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
    if (!fd)
        return false;

    auto* device = gbm_create_device(fd.value());
    if (!device)
        return false;

    priv->deviceFD = WTFMove(fd);
    priv->device = device;

    return true;
}

static GBytes* wpeBufferDMABufImportToPixels(WPEBuffer* buffer, GError** error)
{
    auto* dmabufBuffer = WPE_BUFFER_DMA_BUF(buffer);
    auto* priv = dmabufBuffer->priv;
    auto width = static_cast<uint32_t>(wpe_buffer_get_width(buffer));
    auto height = static_cast<uint32_t>(wpe_buffer_get_height(buffer));
    if (!priv->bufferObject) {
        if (!wpeBufferDMABufTryEnsureGBMDevice(dmabufBuffer)) {
            g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to pixels_buffer: failed to get GBM device");
            return nullptr;
        }

        if (priv->format != DRM_FORMAT_ARGB8888 && priv->format != DRM_FORMAT_XRGB8888 && priv->modifier != DRM_FORMAT_MOD_LINEAR && priv->modifier != DRM_FORMAT_MOD_INVALID) {
            g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to pixels_buffer: unsupported buffer format");
            return nullptr;
        }

        struct gbm_import_fd_data fdData = { priv->fds[0].value(), width, height, priv->strides[0], priv->format };
        priv->bufferObject = gbm_bo_import(priv->device.value(), GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
        if (!priv->bufferObject) {
            g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to pixels_buffer: gbm_bo_import failed");
            return nullptr;
        }
    }

    uint32_t mapStride = 0;
    void* mapData = nullptr;
    void* map = gbm_bo_map(priv->bufferObject, 0, 0, width, height, GBM_BO_TRANSFER_READ, &mapStride, &mapData);
    if (!map) {
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to pixels buffer: gbm_bo_map failed");
        return nullptr;
    }

    struct BufferData {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(BufferData);
        struct gbm_bo* buffer;
        void* data;
    };
    auto bufferData = makeUnique<BufferData>(BufferData { priv->bufferObject, mapData });
    priv->pixels = adoptGRef(g_bytes_new_with_free_func(map, height * mapStride, [](gpointer data) {
        std::unique_ptr<BufferData> bufferData(static_cast<BufferData*>(data));
        gbm_bo_unmap(bufferData->buffer, bufferData->data);
    }, bufferData.release()));

    return priv->pixels.get();
}
#endif

static void wpe_buffer_dma_buf_class_init(WPEBufferDMABufClass* bufferDMABufClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(bufferDMABufClass);
    objectClass->dispose = wpeBufferDMABufDispose;

    WPEBufferClass* bufferClass = WPE_BUFFER_CLASS(bufferDMABufClass);
    bufferClass->import_to_egl_image = wpeBufferDMABufImportToEGLImage;
#if USE(GBM)
    bufferClass->import_to_pixels = wpeBufferDMABufImportToPixels;
#endif
}

/**
 * wpe_buffer_dma_buf_new:
 * @display: a #WPEDisplay
 * @width: the buffer width
 * @height: the buffer height
 * @format: the buffer format
 * @n_planes: the number of planes
 * @fds: the buffer file descriptors
 * @offsets: the buffer offsets
 * @strides: the buffer strides
 * @modifier: the buffer modifier
 *
 * Create a new #WPEBufferDMABuf for the given parameters.
 * The buffer will take the ownership of the @fds.
 *
 * Returns: (transfer full): a #WPEBufferDMABuf
 */
WPEBufferDMABuf* wpe_buffer_dma_buf_new(WPEDisplay* display, int width, int height, guint32 format, guint32 planeCount, int* fds, guint32* offsets, guint32* strides, guint64 modifier)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);
    g_return_val_if_fail(planeCount > 0, nullptr);
    g_return_val_if_fail(fds, nullptr);
    g_return_val_if_fail(offsets, nullptr);
    g_return_val_if_fail(strides, nullptr);

    auto* buffer = WPE_BUFFER_DMA_BUF(g_object_new(WPE_TYPE_BUFFER_DMA_BUF,
        "display", display,
        "width", width,
        "height", height,
        nullptr));

    buffer->priv->format = format;
    buffer->priv->fds.reserveInitialCapacity(planeCount);
    for (guint32 i = 0; i < planeCount; ++i)
        buffer->priv->fds.append(UnixFileDescriptor { fds[i], UnixFileDescriptor::Adopt });
    buffer->priv->offsets.grow(planeCount);
    memcpy(buffer->priv->offsets.mutableSpan().data(), offsets, planeCount * sizeof(guint32));
    buffer->priv->strides.grow(planeCount);
    memcpy(buffer->priv->strides.mutableSpan().data(), strides, planeCount * sizeof(guint32));
    buffer->priv->modifier = modifier;

    return buffer;
}

/**
 * wpe_buffer_dma_buf_get_format:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the @buffer format
 *
 * Returns: the buffer format
 */
guint32 wpe_buffer_dma_buf_get_format(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), 0);

    return buffer->priv->format;
}

/**
 * wpe_buffer_dma_buf_get_n_planes:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the numbers of planes of @buffer
 *
 * Returns: the number of planes
 */
guint32 wpe_buffer_dma_buf_get_n_planes(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), 0);

    return buffer->priv->fds.size();
}

/**
 * wpe_buffer_dma_buf_get_fd:
 * @buffer: a #WPEBufferDMABuf
 * @plane: the plane index
 *
 * Get the @buffer file descriptor of @plane
 *
 * Return: a file descriptor, or -1
 */
int wpe_buffer_dma_buf_get_fd(WPEBufferDMABuf* buffer, guint32 plane)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), -1);
    g_return_val_if_fail(plane < buffer->priv->fds.size(), -1);

    return buffer->priv->fds[plane].value();
}

/**
 * wpe_buffer_dma_buf_get_offset:
 * @buffer: a #WPEBufferDMABuf
 * @plane: the plane index
 *
 * Get the @buffer offset of @plane
 *
 * Return: the buffer offset
 */
guint32 wpe_buffer_dma_buf_get_offset(WPEBufferDMABuf* buffer, guint32 plane)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), 0);
    g_return_val_if_fail(plane < buffer->priv->offsets.size(), 0);

    return buffer->priv->offsets[plane];
}

/**
 * wpe_buffer_dma_buf_get_stride:
 * @buffer: a #WPEBufferDMABuf
 * @plane: the plane index
 *
 * Get the @buffer stride of @plane
 *
 * Return: the buffer stride
 */
guint32 wpe_buffer_dma_buf_get_stride(WPEBufferDMABuf* buffer, guint32 plane)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), 0);
    g_return_val_if_fail(plane < buffer->priv->strides.size(), 0);

    return buffer->priv->strides[plane];
}

/**
 * wpe_buffer_dma_buf_get_modifier:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the @buffer modifier
 *
 * Return: the buffer modifier
 */
guint64 wpe_buffer_dma_buf_get_modifier(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), 0);

    return buffer->priv->modifier;
}

/**
 * wpe_buffer_dma_buf_set_rendering_fence:
 * @buffer: a #WPEBufferDMABuf
 * @fd: a file descriptor, or -1
 *
 * Set the rendering fence file descriptor to use for the @buffer. The fence
 * will be used to wait before rendering the buffer.
 * The buffer takes the ownership of the file descriptor.
 */
void wpe_buffer_dma_buf_set_rendering_fence(WPEBufferDMABuf* buffer, int fd)
{
    g_return_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer));

    if (buffer->priv->renderingFence.value() == fd)
        return;

    buffer->priv->renderingFence = UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

/**
 * wpe_buffer_dma_buf_get_rendering_fence:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the rendering fence file descriptor of @buffer.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_dma_buf_get_rendering_fence(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), -1);

    return buffer->priv->renderingFence.value();
}

/**
 * wpe_buffer_dma_buf_take_rendering_fence:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the rendering fence file descriptor of @buffer and set it to -1.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_dma_buf_take_rendering_fence(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), -1);

    return buffer->priv->renderingFence.release();
}

 /**
 * wpe_buffer_dma_buf_set_release_fence:
 * @buffer: a #WPEBufferDMABuf
 * @fd: a file descriptor, or -1
 *
 * Set the release fence file descriptor to use for the @buffer. The fence
 * will be used to wait before releasing the buffer to be destroyed or reused.
 * The buffer takes the ownership of the file descriptor.
 */
void wpe_buffer_dma_buf_set_release_fence(WPEBufferDMABuf* buffer, int fd)
{
    g_return_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer));

    if (buffer->priv->releaseFence.value() == fd)
        return;

    buffer->priv->releaseFence = UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

/**
 * wpe_buffer_dma_buf_get_release_fence:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the release fence file descriptor of @buffer.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_dma_buf_get_release_fence(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), -1);

    return buffer->priv->releaseFence.value();
}

/**
 * wpe_buffer_dma_buf_take_release_fence:
 * @buffer: a #WPEBufferDMABuf
 *
 * Get the release fence file descriptor of @buffer and set it to -1.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_dma_buf_take_release_fence(WPEBufferDMABuf* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_DMA_BUF(buffer), -1);

    return buffer->priv->releaseFence.release();
}
