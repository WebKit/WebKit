/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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

HeadlessViewBackend::HeadlessViewBackend(uint32_t width, uint32_t height)
    : ViewBackend(width, height)
{
    static struct wpe_playstation_view_backend_exportable_client exportableClient = {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    m_exportable = wpe_playstation_view_backend_exportable_create(&exportableClient, this, width, height);
}

HeadlessViewBackend::~HeadlessViewBackend()
{
    if (m_exportable)
        wpe_playstation_view_backend_exportable_destroy(m_exportable);
}

struct wpe_view_backend* HeadlessViewBackend::backend() const
{
    if (m_exportable)
        return wpe_playstation_view_backend_exportable_get_view_backend(m_exportable);
    return nullptr;
}

PlatformImage HeadlessViewBackend::snapshot()
{
    return nullptr;
}

void HeadlessViewBackend::updateSnapshot(PlatformBuffer)
{
}

void HeadlessViewBackend::vsync()
{
}

} // namespace WPEToolingBackends
