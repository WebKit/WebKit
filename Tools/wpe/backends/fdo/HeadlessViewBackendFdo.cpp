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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "HeadlessViewBackend.h"

#include <cassert>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>

#if WPE_FDO_CHECK_VERSION(1,7,0)
#include <wayland-server.h>
#include <wpe/unstable/fdo-shm.h>
#endif

#if defined(USE_SKIA) && USE_SKIA
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPixmap.h>
#pragma GCC diagnostic pop
#endif

namespace WPEToolingBackends {

struct HeadlessInstance {
    static const HeadlessInstance& singleton()
    {
        static std::once_flag s_onceFlag;
        static HeadlessInstance s_instance;
        std::call_once(s_onceFlag,
            [] {
#if WPE_FDO_CHECK_VERSION(1,7,0)
                wpe_fdo_initialize_shm();
#endif
            });

        return s_instance;
    }
};

#if defined(USE_CAIRO) && USE_CAIRO
static cairo_user_data_key_t s_bufferKey;
#endif

static GSourceFuncs frameSourceFuncs = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean {
        if (g_source_get_ready_time(source) == -1)
            return G_SOURCE_CONTINUE;
        g_source_set_ready_time(source, -1);
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

HeadlessViewBackend::HeadlessViewBackend(uint32_t width, uint32_t height)
    : ViewBackend(width, height)
{
    // This should initialize the SHM mode.
    HeadlessInstance::singleton();

    static struct wpe_view_backend_exportable_fdo_client exportableClient = {
        nullptr,
        nullptr,
#if WPE_FDO_CHECK_VERSION(1,7,0)
        // export_shm_buffer
        [](void* data, struct wpe_fdo_shm_exported_buffer* buffer)
        {
            auto& backend = *static_cast<HeadlessViewBackend*>(data);
            backend.updateSnapshot(buffer);
            wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(backend.m_exportable, buffer);
            auto now = g_get_monotonic_time();
            if (!backend.m_update.lastFrameTime)
                backend.m_update.lastFrameTime = now;
            auto next = backend.m_update.lastFrameTime + (G_USEC_PER_SEC / 60);
            backend.m_update.lastFrameTime = now;
            if (next <= now)
                g_source_set_ready_time(backend.m_update.source, 0);
            else
                g_source_set_ready_time(backend.m_update.source, next);
        },
#else
        nullptr,
#endif
        nullptr,
        nullptr,
    };
    m_exportable = wpe_view_backend_exportable_fdo_create(&exportableClient, this, width, height);

    addActivityState(wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);

#if defined(USE_CAIRO) && USE_CAIRO
    {
        uint32_t stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, m_width);
        uint8_t* buffer = new uint8_t[stride * m_height];
        memset(buffer, 0, stride * m_height);

        m_snapshot = cairo_image_surface_create_for_data(buffer, CAIRO_FORMAT_ARGB32,
            m_width, m_height, stride);

        cairo_surface_set_user_data(m_snapshot, &s_bufferKey, buffer,
            [](void* data) {
                auto* buffer = static_cast<uint8_t*>(data);
                delete[] buffer;
            });
        cairo_surface_mark_dirty(m_snapshot);
    }
#elif defined(USE_SKIA) && USE_SKIA
    {
        auto info = SkImageInfo::MakeN32Premul(m_width, m_height, SkColorSpace::MakeSRGB());
        size_t stride = info.minRowBytes();
        uint8_t* buffer = new uint8_t[stride * m_height];
        memset(buffer, 0, stride * m_height);

        SkPixmap pixmap(info, buffer, stride);
        m_snapshot = SkImages::RasterFromPixmap(pixmap, [](const void*, void* data) {
            auto* buffer = static_cast<uint8_t*>(data);
            delete[] buffer;
        }, buffer);
    }
#endif

#if WPE_CHECK_VERSION(1, 11, 1)
    wpe_view_backend_set_fullscreen_handler(backend(), onDOMFullScreenRequest, this);
#endif

    m_update.source = g_source_new(&frameSourceFuncs, sizeof(GSource));
    g_source_set_priority(m_update.source, G_PRIORITY_DEFAULT);
    g_source_set_callback(m_update.source, [](gpointer data) -> gboolean {
        auto& backend = *static_cast<HeadlessViewBackend*>(data);

#if WPE_FDO_CHECK_VERSION(1,7,0)
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(backend.m_exportable);
#endif

        if (g_source_is_destroyed(backend.m_update.source))
            return G_SOURCE_REMOVE;
        return G_SOURCE_CONTINUE;
    }, this, nullptr);
    g_source_attach(m_update.source, g_main_context_default());
    g_source_set_ready_time(m_update.source, -1);
}

HeadlessViewBackend::~HeadlessViewBackend()
{
    if (m_update.source) {
        g_source_destroy(m_update.source);
        g_source_unref(m_update.source);
    }

#if defined(USE_CAIRO) && USE_CAIRO
    if (m_snapshot)
        cairo_surface_destroy(m_snapshot);
#endif

    if (m_exportable)
        wpe_view_backend_exportable_fdo_destroy(m_exportable);
}

struct wpe_view_backend* HeadlessViewBackend::backend() const
{
    if (m_exportable)
        return wpe_view_backend_exportable_fdo_get_view_backend(m_exportable);
    return nullptr;
}

PlatformImage HeadlessViewBackend::snapshot()
{
#if defined(USE_CAIRO) && USE_CAIRO
    return cairo_surface_reference(m_snapshot);
#elif defined(USE_SKIA) && USE_SKIA
    return SkRef(m_snapshot.get());
#endif
}

void HeadlessViewBackend::updateSnapshot(PlatformBuffer exportedBuffer)
{
#if WPE_FDO_CHECK_VERSION(1,7,0)
    struct wl_shm_buffer* shmBuffer = wpe_fdo_shm_exported_buffer_get_shm_buffer(exportedBuffer);
    {
        auto format = wl_shm_buffer_get_format(shmBuffer);
        if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888)
            return;
    }

#if defined(USE_CAIRO) && USE_CAIRO
    uint32_t bufferStride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, m_width);
#elif defined(USE_SKIA) && USE_SKIA
    auto info = SkImageInfo::MakeN32Premul(m_width, m_height, SkColorSpace::MakeSRGB());
    uint32_t bufferStride = info.minRowBytes();
#endif
    uint8_t* buffer = new uint8_t[bufferStride * m_height];
    memset(buffer, 0, bufferStride * m_height);

    {
        uint32_t width = std::min<uint32_t>(m_width, std::max(0, wl_shm_buffer_get_width(shmBuffer)));
        uint32_t height = std::min<uint32_t>(m_height, std::max(0, wl_shm_buffer_get_height(shmBuffer)));
        uint32_t stride = std::max(0, wl_shm_buffer_get_stride(shmBuffer));

        wl_shm_buffer_begin_access(shmBuffer);
        auto* data = static_cast<uint8_t*>(wl_shm_buffer_get_data(shmBuffer));

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                buffer[bufferStride * y + 4 * x + 0] = data[stride * y + 4 * x + 0];
                buffer[bufferStride * y + 4 * x + 1] = data[stride * y + 4 * x + 1];
                buffer[bufferStride * y + 4 * x + 2] = data[stride * y + 4 * x + 2];
                buffer[bufferStride * y + 4 * x + 3] = data[stride * y + 4 * x + 3];
            }
        }

        wl_shm_buffer_end_access(shmBuffer);
    }

#if defined(USE_CAIRO) && USE_CAIRO
    if (m_snapshot)
        cairo_surface_destroy(m_snapshot);

    m_snapshot = cairo_image_surface_create_for_data(buffer, CAIRO_FORMAT_ARGB32,
        m_width, m_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, m_width));

    static cairo_user_data_key_t bufferKey;
    cairo_surface_set_user_data(m_snapshot, &bufferKey, buffer,
        [](void* data) {
            auto* buffer = static_cast<uint8_t*>(data);
            delete[] buffer;
        });
    cairo_surface_mark_dirty(m_snapshot);
#elif defined(USE_SKIA) && USE_SKIA
    SkPixmap pixmap(info, buffer, bufferStride);
    m_snapshot = SkImages::RasterFromPixmap(pixmap, [](const void*, void* data) {
        auto* buffer = static_cast<uint8_t*>(data);
        delete[] buffer;
    }, buffer);
#endif

#else
    (void)exportedBuffer;
#endif
}

} // namespace WPEToolingBackends
