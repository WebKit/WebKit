/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspector.h"
#include "RemoteInspectorSocketEndpoint.h"

namespace Inspector {

class RemoteInspectorServer final : public RemoteInspectorSocketEndpoint::Listener {
public:
    ~RemoteInspectorServer() final;

    JS_EXPORT_PRIVATE static RemoteInspectorServer& singleton();

    JS_EXPORT_PRIVATE bool start(const char* address, uint16_t port);
    JS_EXPORT_PRIVATE std::optional<uint16_t> getPort() const;
    bool isRunning() const { return !!m_server; }

private:
    friend class LazyNeverDestroyed<RemoteInspectorServer>;
    RemoteInspectorServer() { Socket::init(); }

    std::optional<ConnectionID> doAccept(RemoteInspectorSocketEndpoint&, PlatformSocketType) final;
    void didChangeStatus(RemoteInspectorSocketEndpoint&, ConnectionID, RemoteInspectorSocketEndpoint::Listener::Status) final { };

    std::optional<ConnectionID> m_server;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
