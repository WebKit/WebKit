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
#include <wpe/wpe.h>

#if defined(USE_ATK) && USE_ATK
typedef struct _AtkObject AtkObject;
#endif

namespace WPEToolingBackends {

class ViewBackend {
public:
    virtual ~ViewBackend();

    virtual struct wpe_view_backend* backend() const = 0;

    class InputClient {
    public:
        virtual ~InputClient() = default;

        virtual bool dispatchPointerEvent(struct wpe_input_pointer_event*) { return false; }
        virtual bool dispatchAxisEvent(struct wpe_input_axis_event*) { return false; }
        virtual bool dispatchKeyboardEvent(struct wpe_input_keyboard_event*) { return false; }
        virtual bool dispatchTouchEvent(struct wpe_input_touch_event*) { return false; }
    };
    void setInputClient(std::unique_ptr<InputClient>&&);
#if defined(USE_ATK) && USE_ATK
    void setAccessibleChild(AtkObject*);
#endif

    void addActivityState(uint32_t);
    void removeActivityState(uint32_t);

    virtual void setFullscreen(bool);
    virtual void setMaximized(bool);

    bool isFullscreen() const { return m_fullscreen; }
    bool isMaximized() const { return m_maximized; }

protected:
    ViewBackend(uint32_t width, uint32_t height);

    void initializeAccessibility();
    void updateAccessibilityState(uint32_t);
    static void notifyAccessibilityKeyEventListeners(struct wpe_input_keyboard_event* event);

    void dispatchInputPointerEvent(struct wpe_input_pointer_event*);
    void dispatchInputAxisEvent(struct wpe_input_axis_event*);
    void dispatchInputKeyboardEvent(struct wpe_input_keyboard_event*);
    void dispatchInputTouchEvent(struct wpe_input_touch_event*);

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    std::unique_ptr<InputClient> m_inputClient;
    bool m_fullscreen { false };
    bool m_maximized { false };
};

} // namespace WPEToolingBackends
