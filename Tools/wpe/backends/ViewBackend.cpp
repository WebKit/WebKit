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

namespace WPEToolingBackends {

ViewBackend::ViewBackend(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
{
    wpe_loader_init(WPE_BACKEND);
}

ViewBackend::~ViewBackend() = default;

#if !(defined(ENABLE_ACCESSIBILITY) && ENABLE_ACCESSIBILITY)
void ViewBackend::initializeAccessibility()
{
}

void ViewBackend::updateAccessibilityState(uint32_t)
{
}
#endif // !ENABLE(ACCESSIBILITY)

void ViewBackend::setInputClient(std::unique_ptr<InputClient>&& client)
{
    m_inputClient = std::move(client);
}

void ViewBackend::addActivityState(uint32_t flags)
{
    uint32_t previousFlags = wpe_view_backend_get_activity_state(backend());
    wpe_view_backend_add_activity_state(backend(), flags);
    updateAccessibilityState(previousFlags);
}

void ViewBackend::removeActivityState(uint32_t flags)
{
    uint32_t previousFlags = wpe_view_backend_get_activity_state(backend());
    wpe_view_backend_remove_activity_state(backend(), flags);
    updateAccessibilityState(previousFlags);
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
#if defined(ENABLE_ACCESSIBILITY) && ENABLE_ACCESSIBILITY
    notifyAccessibilityKeyEventListeners(event);
#endif

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
