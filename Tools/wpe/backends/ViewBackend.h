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

#pragma once

#include <memory>
#include <wpe/fdo.h>

typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef struct _AtkObject AtkObject;
struct wpe_fdo_egl_exported_image;

// Manually provide the EGL_CAST C++ definition in case eglplatform.h doesn't provide it.
#ifndef EGL_CAST
#define EGL_CAST(type, value) (static_cast<type>(value))
#endif

namespace WPEToolingBackends {

class ViewBackend {
public:
    virtual ~ViewBackend();

    class InputClient {
    public:
        virtual ~InputClient() = default;

        virtual bool dispatchPointerEvent(struct wpe_input_pointer_event*) { return false; }
        virtual bool dispatchAxisEvent(struct wpe_input_axis_event*) { return false; }
        virtual bool dispatchKeyboardEvent(struct wpe_input_keyboard_event*) { return false; }
        virtual bool dispatchTouchEvent(struct wpe_input_touch_event*) { return false; }
    };
    void setInputClient(std::unique_ptr<InputClient>&&);
#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY
    void setAccessibleChild(AtkObject*);
#endif

    struct wpe_view_backend* backend() const;

protected:
    ViewBackend(uint32_t width, uint32_t height);

    bool initialize();
    void initializeAccessibility();
    void updateAccessibilityState(uint32_t);

    void addActivityState(uint32_t);
    void removeActivityState(uint32_t);

    void dispatchInputPointerEvent(struct wpe_input_pointer_event*);
    void dispatchInputAxisEvent(struct wpe_input_axis_event*);
    void dispatchInputKeyboardEvent(struct wpe_input_keyboard_event*);
    void dispatchInputTouchEvent(struct wpe_input_touch_event*);

    virtual void displayBuffer(struct wpe_fdo_egl_exported_image*) = 0;

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    EGLDisplay m_eglDisplay { nullptr };
    EGLContext m_eglContext { nullptr };
    EGLConfig m_eglConfig;
    struct wpe_view_backend_exportable_fdo* m_exportable { nullptr };
    std::unique_ptr<InputClient> m_inputClient;
};

} // namespace WPEToolingBackends
