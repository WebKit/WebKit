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
#include <unistd.h>
#include <wpe/fdo-egl.h>

// Manually provide the EGL_CAST C++ definition in case eglplatform.h doesn't provide it.
#ifndef EGL_CAST
#define EGL_CAST(type, value) (static_cast<type>(value))
#endif

// Keep this in sync with wtf/glib/RunLoopSourcePriority.h.
static int kRunLoopSourcePriorityDispatcher = -70;

static EGLDisplay getEGLDisplay()
{
    static EGLDisplay s_display = EGL_NO_DISPLAY;
    if (s_display == EGL_NO_DISPLAY) {
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY)
            return EGL_NO_DISPLAY;

        if (!eglInitialize(display, nullptr, nullptr))
            return EGL_NO_DISPLAY;

        if (!eglBindAPI(EGL_OPENGL_ES_API))
            return EGL_NO_DISPLAY;

        wpe_fdo_initialize_for_egl_display(display);
        s_display = display;
    }

    return s_display;
}

HeadlessViewBackend::HeadlessViewBackend()
{
    m_egl.display = getEGLDisplay();

    static const EGLint configAttributes[13] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    EGLBoolean ret = eglChooseConfig(m_egl.display, configAttributes, &m_egl.config, 1, &numConfigs);
    if (!ret || !numConfigs)
        return;

    static const EGLint contextAttributes[3] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_egl.context = eglCreateContext(m_egl.display, m_egl.config, EGL_NO_CONTEXT, contextAttributes);
    if (m_egl.context == EGL_NO_CONTEXT)
        return;

    if (!eglMakeCurrent(m_egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_egl.context))
        return;

    m_egl.createImage = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    m_egl.destroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    m_egl.queryBuffer = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
    m_egl.imageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    m_exportable = wpe_view_backend_exportable_fdo_create(&s_exportableClient, this, 800, 600);

    m_updateSource = g_timeout_source_new(m_frameRate / 1000);
    g_source_set_callback(m_updateSource,
        [](gpointer data) -> gboolean {
            auto& backend = *static_cast<HeadlessViewBackend*>(data);
            backend.performUpdate();
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

    if (auto image = std::get<0>(m_pendingImage.second))
        m_egl.destroyImage(m_egl.display, image);
    if (auto image = std::get<0>(m_lockedImage.second))
        m_egl.destroyImage(m_egl.display, image);

    if (m_egl.context)
        eglDestroyContext(m_egl.display, m_egl.context);

    wpe_view_backend_exportable_fdo_destroy(m_exportable);
}

struct wpe_view_backend* HeadlessViewBackend::backend() const
{
    return wpe_view_backend_exportable_fdo_get_view_backend(m_exportable);
}

cairo_surface_t* HeadlessViewBackend::createSnapshot()
{
    performUpdate();

    EGLImageKHR image = std::get<0>(m_lockedImage.second);
    if (!image)
        return nullptr;

    uint32_t width = std::get<1>(m_lockedImage.second);
    uint32_t height = std::get<2>(m_lockedImage.second);

    uint8_t* buffer = new uint8_t[4 * width * height];
    bool successfulSnapshot = false;

    if (!eglMakeCurrent(m_egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_egl.context))
        return nullptr;

    GLuint imageTexture;
    glGenTextures(1, &imageTexture);
    glBindTexture(GL_TEXTURE_2D, imageTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);

    m_egl.imageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint imageFramebuffer;
    glGenFramebuffers(1, &imageFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, imageFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, imageTexture, 0);

    glFlush();
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);
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
        CAIRO_FORMAT_ARGB32, width, height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));
    cairo_surface_mark_dirty(imageSurface);

    static cairo_user_data_key_t bufferKey;
    cairo_surface_set_user_data(imageSurface, &bufferKey, buffer,
        [](void* data) {
            auto* buffer = static_cast<uint8_t*>(data);
            delete[] buffer;
        });

    return imageSurface;
}

void HeadlessViewBackend::performUpdate()
{
    if (!m_pendingImage.first)
        return;

    wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
    if (m_lockedImage.first) {
        wpe_view_backend_exportable_fdo_dispatch_release_buffer(m_exportable, m_lockedImage.first);
        m_egl.destroyImage(m_egl.display, std::get<0>(m_lockedImage.second));
    }

    m_lockedImage = m_pendingImage;
    m_pendingImage = std::pair<struct wl_resource*, std::tuple<EGLImageKHR, uint32_t, uint32_t>> { };
}

struct wpe_view_backend_exportable_fdo_client HeadlessViewBackend::s_exportableClient = {
    // export_buffer_resource
    [](void* data, struct wl_resource* bufferResource)
    {
        auto& backend = *static_cast<HeadlessViewBackend*>(data);
        if (backend.m_pendingImage.first)
            std::abort();

        auto& egl = backend.m_egl;

        EGLint format = 0;
        if (!egl.queryBuffer(egl.display, bufferResource, EGL_TEXTURE_FORMAT, &format) || format != EGL_TEXTURE_RGBA)
            return;

        EGLint width, height;
        if (!egl.queryBuffer(egl.display, bufferResource, EGL_WIDTH, &width)
            || !egl.queryBuffer(egl.display, bufferResource, EGL_HEIGHT, &height))
            return;

        EGLint attributes[] = { EGL_WAYLAND_PLANE_WL, 0, EGL_NONE };
        EGLImageKHR image = egl.createImage(egl.display, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, bufferResource, attributes);
        backend.m_pendingImage = { bufferResource, std::make_tuple(image, width, height) };
    },
    // padding
    nullptr,
    nullptr,
    nullptr,
    nullptr
};
