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

// This include order is necessary to enforce the GBM EGL platform.
#include <gbm.h>
#include <epoxy/egl.h>
#include <wpe/fdo-egl.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WL_bind_wayland_display 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource* buffer, EGLint attribute, EGLint* value);

#define EGL_WAYLAND_BUFFER_WL 0x31D5 // eglCreateImageKHR target
#define EGL_WAYLAND_PLANE_WL  0x31D6 // eglCreateImageKHR target
#endif

namespace WPEToolingBackends {

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture2DOES;

// Keep this in sync with wtf/glib/RunLoopSourcePriority.h.
static int kRunLoopSourcePriorityDispatcher = -70;

static EGLDisplay getEGLDisplay()
{
    static EGLDisplay s_display = EGL_NO_DISPLAY;
    if (s_display == EGL_NO_DISPLAY) {
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY)
            return EGL_NO_DISPLAY;

        s_display = display;
    }

    return s_display;
}

HeadlessViewBackend::HeadlessViewBackend(uint32_t width, uint32_t height)
    : ViewBackend(width, height)
    , m_pendingImage(EGL_NO_IMAGE_KHR)
    , m_lockedImage(EGL_NO_IMAGE_KHR)
{
    m_eglDisplay = getEGLDisplay();
    if (!initialize())
        return;

#if defined(WPE_BACKEND_CHECK_VERSION) && WPE_BACKEND_CHECK_VERSION(1, 1, 0)
    wpe_view_backend_add_activity_state(backend(), wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);
#endif

    if (!eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
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
}

cairo_surface_t* HeadlessViewBackend::createSnapshot()
{
    if (!m_eglContext)
        return nullptr;

    performUpdate();

    if (!m_lockedImage)
        return nullptr;

    uint8_t* buffer = new uint8_t[4 * m_width * m_height];
    bool successfulSnapshot = false;

    if (!eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
        return nullptr;

    GLuint imageTexture;
    glGenTextures(1, &imageTexture);
    glBindTexture(GL_TEXTURE_2D, imageTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, m_width, m_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);

    imageTargetTexture2DOES(GL_TEXTURE_2D, m_lockedImage);
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

void HeadlessViewBackend::performUpdate()
{
    if (!m_pendingImage)
        return;

    wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
    if (m_lockedImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_image(m_exportable, m_lockedImage);

    m_lockedImage = m_pendingImage;
    m_pendingImage = EGL_NO_IMAGE_KHR;
}

void HeadlessViewBackend::displayBuffer(EGLImageKHR image)
{
    if (m_pendingImage)
        std::abort();

    m_pendingImage = image;
}

} // namespace WPEToolingBackends
