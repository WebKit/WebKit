/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "DisplayLink.h"

#if HAVE(DISPLAY_LINK)

#include <wpe/playstation.h>

namespace WebKit {

void DisplayLink::platformInitialize()
{
    static const struct wpe_playstation_display_client_interface s_client = {
        // vblank
        [](void* data) {
            static_cast<DisplayLink*>(data)->notifyObserversDisplayDidRefresh();
        },
        nullptr, nullptr, nullptr,
    };

    m_display = wpe_playstation_display_create();
    wpe_playstation_display_set_client(m_display, &s_client, this);
}

void DisplayLink::platformFinalize()
{
    wpe_playstation_display_destroy(m_display);
}

bool DisplayLink::platformIsRunning() const
{
    return wpe_playstation_display_is_running(m_display);
}

void DisplayLink::platformStart()
{
    wpe_playstation_display_start(m_display);
}

void DisplayLink::platformStop()
{
    wpe_playstation_display_stop(m_display);
}

} // namespace WebKit

#endif
