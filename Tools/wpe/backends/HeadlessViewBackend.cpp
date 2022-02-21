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

namespace WPEToolingBackends {

#if WPE_CHECK_VERSION(1, 11, 1)
bool HeadlessViewBackend::onDOMFullScreenRequest(void* data, bool fullscreen)
{
    auto& headless = *static_cast<HeadlessViewBackend*>(data);

    if (fullscreen == headless.m_is_fullscreen) {
        // Handle situations where DOM fullscreen requests are mixed with system fullscreen commands (e.g F11)
        headless.dispatchFullscreenEvent();
        return true;
    }
    headless.m_is_fullscreen = fullscreen;
    return true;
}

void HeadlessViewBackend::dispatchFullscreenEvent()
{
    if (m_is_fullscreen)
        wpe_view_backend_dispatch_did_enter_fullscreen(backend());
    else
        wpe_view_backend_dispatch_did_exit_fullscreen(backend());
}

#endif

} // namespace WPEToolingBackends
