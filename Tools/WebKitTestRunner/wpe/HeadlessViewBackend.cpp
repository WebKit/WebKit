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

#include "config.h"
#include "HeadlessViewBackend.h"

#include <cassert>
#include <fcntl.h>
#include <unistd.h>

// FIXME: Deploy good practices and clean up GBM resources at process exit.
static EGLDisplay getEGLDisplay()
{
    static EGLDisplay s_display = EGL_NO_DISPLAY;
    if (s_display == EGL_NO_DISPLAY) {
        int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC | O_NOCTTY | O_NONBLOCK);
        if (fd < 0)
            return EGL_NO_DISPLAY;

        struct gbm_device* device = gbm_create_device(fd);
        if (!device)
            return EGL_NO_DISPLAY;

        EGLDisplay display = eglGetDisplay(device);
        if (display == EGL_NO_DISPLAY)
            return EGL_NO_DISPLAY;

        if (!eglInitialize(display, nullptr, nullptr))
            return EGL_NO_DISPLAY;

        if (!eglBindAPI(EGL_OPENGL_ES_API))
            return EGL_NO_DISPLAY;

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
    m_egl.imageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    m_exportable = wpe_mesa_view_backend_exportable_dma_buf_create(&s_exportableClient, this);

    m_updateSource = g_timeout_source_new(m_frameRate / 1000);
    g_source_set_callback(m_updateSource,
        [](gpointer data) -> gboolean {
            auto& backend = *static_cast<HeadlessViewBackend*>(data);
            backend.performUpdate();
            return TRUE;
        }, this, nullptr);
    g_source_attach(m_updateSource, g_main_context_default());
}

HeadlessViewBackend::~HeadlessViewBackend()
{
    if (m_updateSource)
        g_source_destroy(m_updateSource);

    if (auto image = std::get<0>(m_pendingImage.second))
        m_egl.destroyImage(m_egl.display, image);
    if (auto image = std::get<0>(m_lockedImage.second))
        m_egl.destroyImage(m_egl.display, image);

    for (auto it : m_exportMap) {
        int fd = it.second;
        if (fd >= 0)
            close(fd);
    }

    if (m_egl.context)
        eglDestroyContext(m_egl.display, m_egl.context);
}

struct wpe_view_backend* HeadlessViewBackend::backend() const
{
    return wpe_mesa_view_backend_exportable_dma_buf_get_view_backend(m_exportable);
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

    makeCurrent();

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

bool HeadlessViewBackend::makeCurrent()
{
    return eglMakeCurrent(m_egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_egl.context);
}

void HeadlessViewBackend::performUpdate()
{
    if (!m_pendingImage.first)
        return;

    wpe_mesa_view_backend_exportable_dma_buf_dispatch_frame_complete(m_exportable);
    if (m_lockedImage.first) {
        wpe_mesa_view_backend_exportable_dma_buf_dispatch_release_buffer(m_exportable, m_lockedImage.first);
        m_egl.destroyImage(m_egl.display, std::get<0>(m_lockedImage.second));
    }

    m_lockedImage = m_pendingImage;
    m_pendingImage = std::pair<uint32_t, std::tuple<EGLImageKHR, uint32_t, uint32_t>> { };
}

struct wpe_mesa_view_backend_exportable_dma_buf_client HeadlessViewBackend::s_exportableClient = {
    // export_dma_buf
    [](void* data, struct wpe_mesa_view_backend_exportable_dma_buf_data* imageData)
    {
        auto& backend = *static_cast<HeadlessViewBackend*>(data);

        auto it = backend.m_exportMap.end();
        if (imageData->fd >= 0) {
            assert(backend.m_exportMap.find(imageData->handle) == backend.m_exportMap.end());

            it = backend.m_exportMap.insert({ imageData->handle, imageData->fd }).first;
        } else {
            assert(backend.m_exportMap.find(imageData->handle) != backend.m_exportMap.end());
            it = backend.m_exportMap.find(imageData->handle);
        }

        assert(it != backend.m_exportMap.end());
        uint32_t handle = it->first;
        int32_t fd = it->second;

        backend.makeCurrent();

        EGLint attributes[] = {
            EGL_WIDTH, static_cast<EGLint>(imageData->width),
            EGL_HEIGHT, static_cast<EGLint>(imageData->height),
            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(imageData->format),
            EGL_DMA_BUF_PLANE0_FD_EXT, fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(imageData->stride),
            EGL_NONE,
        };
        EGLImageKHR image = backend.m_egl.createImage(backend.m_egl.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
        backend.m_pendingImage = { imageData->handle, std::make_tuple(image, imageData->width, imageData->height) };
    },
};
