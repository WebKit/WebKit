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

#if ENABLE(WPE_PLATFORM)
#include "AcceleratedBackingStoreDMABufMessages.h"
#include "AcceleratedSurfaceDMABufMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/ShareableBitmap.h>
#include <wpe/wpe-platform.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

namespace WebKit {

std::unique_ptr<AcceleratedBackingStoreDMABuf> AcceleratedBackingStoreDMABuf::create(WebPageProxy& webPage, WPEView* view)
{
    return std::unique_ptr<AcceleratedBackingStoreDMABuf>(new AcceleratedBackingStoreDMABuf(webPage, view));
}

AcceleratedBackingStoreDMABuf::AcceleratedBackingStoreDMABuf(WebPageProxy& webPage, WPEView* view)
    : m_webPage(webPage)
    , m_wpeView(view)
{
    g_signal_connect(m_wpeView.get(), "buffer-rendered", G_CALLBACK(+[](WPEView*, WPEBuffer*, gpointer userData) {
        auto& backingStore = *static_cast<AcceleratedBackingStoreDMABuf*>(userData);
        backingStore.bufferRendered();
    }), this);
    g_signal_connect(m_wpeView.get(), "buffer-released", G_CALLBACK(+[](WPEView*, WPEBuffer* buffer, gpointer userData) {
        auto& backingStore = *static_cast<AcceleratedBackingStoreDMABuf*>(userData);
        backingStore.bufferReleased(buffer);
    }), this);
}

AcceleratedBackingStoreDMABuf::~AcceleratedBackingStoreDMABuf()
{
    g_signal_handlers_disconnect_by_data(m_wpeView.get(), this);
}

void AcceleratedBackingStoreDMABuf::updateSurfaceID(uint64_t surfaceID)
{
    if (m_surfaceID == surfaceID)
        return;

    if (m_surfaceID) {
        if (m_pendingBuffer) {
            frameDone();
            m_pendingBuffer = nullptr;
        }
        m_buffers.clear();
        m_bufferIDs.clear();
        m_webPage.process().removeMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID);
    }

    m_surfaceID = surfaceID;
    if (m_surfaceID)
        m_webPage.process().addMessageReceiver(Messages::AcceleratedBackingStoreDMABuf::messageReceiverName(), m_surfaceID, *this);

}

void AcceleratedBackingStoreDMABuf::didCreateBuffer(uint64_t id, const WebCore::IntSize& size, uint32_t format, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, DMABufRendererBufferFormat::Usage usage)
{
    Vector<int> fileDescriptors;
    fileDescriptors.reserveInitialCapacity(fds.size());
    for (auto& fd : fds)
        fileDescriptors.append(fd.release());
    GRefPtr<WPEBuffer> buffer = adoptGRef(WPE_BUFFER(wpe_buffer_dma_buf_new(m_wpeView.get(), size.width(), size.height(), format, fds.size(), fileDescriptors.data(), offsets.data(), strides.data(), modifier)));
    g_object_set_data(G_OBJECT(buffer.get()), "wk-buffer-format-usage", GUINT_TO_POINTER(usage));
    m_bufferIDs.add(buffer.get(), id);
    m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStoreDMABuf::didCreateBufferSHM(uint64_t id, WebCore::ShareableBitmap::Handle&& handle)
{
    auto bitmap = WebCore::ShareableBitmap::create(WTFMove(handle), WebCore::SharedMemory::Protection::ReadOnly);
    if (!bitmap)
        return;

    auto size = bitmap->size();
    const auto* data = bitmap->data();
    auto dataSize = bitmap->sizeInBytes();
    auto stride = bitmap->bytesPerRow();
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(data, dataSize, [](gpointer userData) {
        delete static_cast<WebCore::ShareableBitmap*>(userData);
    }, bitmap.leakRef()));

    GRefPtr<WPEBuffer> buffer = adoptGRef(WPE_BUFFER(wpe_buffer_shm_new(m_wpeView.get(), size.width(), size.height(), WPE_PIXEL_FORMAT_ARGB8888, bytes.get(), stride)));
    m_bufferIDs.add(buffer.get(), id);
    m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStoreDMABuf::didDestroyBuffer(uint64_t id)
{
    if (auto buffer = m_buffers.take(id))
        m_bufferIDs.remove(buffer.get());
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
    GUniqueOutPtr<GError> error;
    if (!wpe_view_render_buffer(m_wpeView.get(), m_pendingBuffer.get(), &error.outPtr())) {
        g_warning("Failed to render frame: %s", error->message);
        frameDone();
    }
}

void AcceleratedBackingStoreDMABuf::frameDone()
{
    m_webPage.process().send(Messages::AcceleratedSurfaceDMABuf::FrameDone(), m_surfaceID);
}

void AcceleratedBackingStoreDMABuf::bufferRendered()
{
    frameDone();
    m_committedBuffer = WTFMove(m_pendingBuffer);
}

void AcceleratedBackingStoreDMABuf::bufferReleased(WPEBuffer* buffer)
{
    if (auto id = m_bufferIDs.get(buffer))
        m_webPage.process().send(Messages::AcceleratedSurfaceDMABuf::ReleaseBuffer(id), m_surfaceID);
}

RendererBufferFormat AcceleratedBackingStoreDMABuf::bufferFormat() const
{
    RendererBufferFormat format;
    auto* buffer = m_committedBuffer ? m_committedBuffer.get() : m_pendingBuffer.get();
    if (!buffer)
        return format;

    if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
        auto* dmabuf = WPE_BUFFER_DMA_BUF(buffer);
        format.type = RendererBufferFormat::Type::DMABuf;
        format.fourcc = wpe_buffer_dma_buf_get_format(dmabuf);
        format.modifier = wpe_buffer_dma_buf_get_modifier(dmabuf);
        format.usage = static_cast<DMABufRendererBufferFormat::Usage>(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(buffer), "wk-buffer-format-usage")));
    } else if (WPE_IS_BUFFER_SHM(buffer)) {
        format.type = RendererBufferFormat::Type::SharedMemory;
        switch (wpe_buffer_shm_get_format(WPE_BUFFER_SHM(buffer))) {
        case WPE_PIXEL_FORMAT_ARGB8888:
#if USE(LIBDRM)
            format.fourcc = DRM_FORMAT_ARGB8888;
#endif
            break;
        }
        format.usage = DMABufRendererBufferFormat::Usage::Rendering;
    }

    return format;
}

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
