/*
 * Copyright (C) 2018 Igalia S.L.
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

#include "ViewBackend.h"

#include <epoxy/egl.h>
#include <glib.h>
#include <wpe/fdo-egl.h>

namespace WPEToolingBackends {

ViewBackend::ViewBackend(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
{
    wpe_loader_init("libWPEBackend-fdo-1.0.so");
}

ViewBackend::~ViewBackend()
{
    if (m_exportable)
        wpe_view_backend_exportable_fdo_destroy(m_exportable);

    if (m_eglContext)
        eglDestroyContext(m_eglDisplay, m_eglContext);
}

bool ViewBackend::initialize()
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return false;

    eglInitialize(m_eglDisplay, nullptr, nullptr);

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        return false;

    wpe_fdo_initialize_for_egl_display(m_eglDisplay);

    static const EGLint configAttributes[13] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    {
        EGLint count = 0;
        if (!eglGetConfigs(m_eglDisplay, nullptr, 0, &count) || count < 1)
            return false;

        EGLConfig* configs = g_new0(EGLConfig, count);
        EGLint matched = 0;
        if (eglChooseConfig(m_eglDisplay, configAttributes, configs, count, &matched) && !!matched)
            m_eglConfig = configs[0];
        g_free(configs);
    }

    static const EGLint contextAttributes[3] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (!m_eglContext)
        return false;

    static struct wpe_view_backend_exportable_fdo_egl_client exportableClient = {
        // export_buffer_resource
        [](void* data, EGLImageKHR image)
        {
            static_cast<ViewBackend*>(data)->displayBuffer(image);
        },
        // padding
        nullptr, nullptr, nullptr, nullptr
    };
    m_exportable = wpe_view_backend_exportable_fdo_egl_create(&exportableClient, this, m_width, m_height);

    return true;
}

void ViewBackend::setInputClient(std::unique_ptr<InputClient>&& client)
{
    m_inputClient = std::move(client);
}

struct wpe_view_backend* ViewBackend::backend() const
{
    return m_exportable ? wpe_view_backend_exportable_fdo_get_view_backend(m_exportable) : nullptr;
}

void ViewBackend::dispatchInputPointerEvent(struct wpe_input_pointer_event* event)
{
    if (m_inputClient && m_inputClient->dispatchPointerEvent(event))
        return;
    wpe_view_backend_dispatch_pointer_event(backend(), event);
}

void ViewBackend::dispatchInputAxisEvent(struct wpe_input_axis_event* event)
{
    if (m_inputClient && m_inputClient->dispatchAxisEvent(event))
        return;
    wpe_view_backend_dispatch_axis_event(backend(), event);
}

void ViewBackend::dispatchInputKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    if (m_inputClient && m_inputClient->dispatchKeyboardEvent(event))
        return;
    wpe_view_backend_dispatch_keyboard_event(backend(), event);
}

void ViewBackend::dispatchInputTouchEvent(struct wpe_input_touch_event* event)
{
    if (m_inputClient && m_inputClient->dispatchTouchEvent(event))
        return;
    wpe_view_backend_dispatch_touch_event(backend(), event);
}

} // namespace WPEToolingBackends
