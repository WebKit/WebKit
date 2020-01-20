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

// This include order is necessary to enforce the GBM EGL platform.
#include <gbm.h>
#include <epoxy/egl.h>
#include <wayland-server.h>
#include <wpe/fdo-egl.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WL_bind_wayland_display 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource* buffer, EGLint attribute, EGLint* value);

#define EGL_WAYLAND_BUFFER_WL 0x31D5 // eglCreateImageKHR target
#define EGL_WAYLAND_PLANE_WL  0x31D6 // eglCreateImageKHR target
#endif

namespace WPEToolingBackends {

struct HeadlessEGLConnection {
    EGLDisplay eglDisplay { EGL_NO_DISPLAY };

    static const HeadlessEGLConnection& singleton()
    {
        static std::once_flag s_onceFlag;
        static HeadlessEGLConnection s_connection;
        std::call_once(s_onceFlag,
            [] {
                EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
                if (eglDisplay == EGL_NO_DISPLAY)
                    return;

                if (!eglInitialize(eglDisplay, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_ES_API))
                    return;

                s_connection.eglDisplay = eglDisplay;
                wpe_fdo_initialize_for_egl_display(s_connection.eglDisplay);
            });

        return s_connection;
    }
};

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture2DOES;

// Keep this in sync with wtf/glib/RunLoopSourcePriority.h.
static int kRunLoopSourcePriorityDispatcher = -70;

HeadlessViewBackend::HeadlessViewBackend(uint32_t width, uint32_t height)
    : ViewBackend(width, height)
{
    auto& connection = HeadlessEGLConnection::singleton();
    if (connection.eglDisplay == EGL_NO_DISPLAY || !initialize(connection.eglDisplay))
        return;

    addActivityState(wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);

    if (!eglMakeCurrent(connection.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
        return;

    imageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    m_updateSource = g_timeout_source_new(m_frameRate / 1000);
    g_source_set_callback(m_updateSource, [](gpointer data) -> gboolean {
        static_cast<HeadlessViewBackend*>(data)->performUpdate();
        return TRUE;
    }, this, nullptr);
    g_source_set_priority(m_updateSource, kRunLoopSourcePriorityDispatcher);
    g_source_attach(m_updateSource, g_main_context_default());
}

HeadlessViewBackend::~HeadlessViewBackend()
{
    if (m_updateSource) {
        g_source_destroy(m_updateSource);
        g_source_unref(m_updateSource);
    }

    if (m_egl.lockedImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.lockedImage);
    if (m_egl.pendingImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.pendingImage);

#if WPE_FDO_CHECK_VERSION(1, 5, 0)
    if (m_shm.lockedBuffer)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_shm_exported_buffer(m_exportable, m_shm.lockedBuffer);
    if (m_shm.pendingBuffer)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_shm_exported_buffer(m_exportable, m_shm.pendingBuffer);
#endif

    deinitialize(HeadlessEGLConnection::singleton().eglDisplay);
}

cairo_surface_t* HeadlessViewBackend::createSnapshot()
{
    performUpdate();

    if (m_egl.lockedImage)
        return createEGLSnapshot();
#if WPE_FDO_CHECK_VERSION(1, 5, 0)
    if (m_shm.lockedBuffer)
        return createSHMSnapshot();
#endif
    return nullptr;
}

cairo_surface_t* HeadlessViewBackend::createEGLSnapshot()
{
    if (!m_eglContext)
        return nullptr;

    uint8_t* buffer = new uint8_t[4 * m_width * m_height];
    bool successfulSnapshot = false;

    if (!eglMakeCurrent(HeadlessEGLConnection::singleton().eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
        return nullptr;

    GLuint imageTexture;
    glGenTextures(1, &imageTexture);
    glBindTexture(GL_TEXTURE_2D, imageTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, m_width, m_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);

    imageTargetTexture2DOES(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(m_egl.lockedImage));
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint imageFramebuffer;
    glGenFramebuffers(1, &imageFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, imageFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, imageTexture, 0);

    glFlush();
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        glReadPixels(0, 0, m_width, m_height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);
        successfulSnapshot = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &imageFramebuffer);
    glDeleteTextures(1, &imageTexture);

    if (!successfulSnapshot) {
        delete[] buffer;
        return nullptr;
    }

    cairo_surface_t* imageSurface = cairo_image_surface_create_for_data(buffer,
        CAIRO_FORMAT_ARGB32, m_width, m_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, m_width));
    cairo_surface_mark_dirty(imageSurface);

    static cairo_user_data_key_t bufferKey;
    cairo_surface_set_user_data(imageSurface, &bufferKey, buffer,
        [](void* data) {
            auto* buffer = static_cast<uint8_t*>(data);
            delete[] buffer;
        });

    return imageSurface;
}

#if WPE_FDO_CHECK_VERSION(1, 5, 0)
cairo_surface_t* HeadlessViewBackend::createSHMSnapshot()
{
    struct wl_shm_buffer* shmBuffer = wpe_fdo_shm_exported_buffer_get_shm_buffer(m_shm.lockedBuffer);
    {
        auto format = wl_shm_buffer_get_format(shmBuffer);
        if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888)
            return nullptr;
    }

    uint32_t bufferStride = 4 * m_width;
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

    cairo_surface_t* imageSurface = cairo_image_surface_create_for_data(buffer,
        CAIRO_FORMAT_ARGB32, m_width, m_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, m_width));
    cairo_surface_mark_dirty(imageSurface);

    static cairo_user_data_key_t bufferKey;
    cairo_surface_set_user_data(imageSurface, &bufferKey, buffer,
        [](void* data) {
            auto* buffer = static_cast<uint8_t*>(data);
            delete[] buffer;
        });

    return imageSurface;
}
#endif

void HeadlessViewBackend::performUpdate()
{
    if (m_egl.pendingImage) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);

        if (m_egl.lockedImage)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(m_exportable, m_egl.lockedImage);

        m_egl.lockedImage = m_egl.pendingImage;
        m_egl.pendingImage = nullptr;
        return;
    }

#if WPE_FDO_CHECK_VERSION(1, 5, 0)
    if (m_shm.pendingBuffer) {
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);

        if (m_shm.lockedBuffer)
            wpe_view_backend_exportable_fdo_egl_dispatch_release_shm_exported_buffer(m_exportable, m_shm.lockedBuffer);

        m_shm.lockedBuffer = m_shm.pendingBuffer;
        m_shm.pendingBuffer = nullptr;
        return;
    }
#endif
}

void HeadlessViewBackend::displayBuffer(struct wpe_fdo_egl_exported_image* image)
{
    if (m_egl.pendingImage)
        std::abort();

    m_egl.pendingImage = image;
}

#if WPE_FDO_CHECK_VERSION(1, 5, 0)
void HeadlessViewBackend::displayBuffer(struct wpe_fdo_shm_exported_buffer* buffer)
{
    if (m_shm.pendingBuffer)
        std::abort();

    m_shm.pendingBuffer = buffer;
}
#endif

} // namespace WPEToolingBackends
