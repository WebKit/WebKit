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

#include "config.h"
#include "RemoteInspectorServer.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorMessageParser.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace Inspector {

RemoteInspectorServer& RemoteInspectorServer::singleton()
{
    static LazyNeverDestroyed<RemoteInspectorServer> shared;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        shared.construct();
    });
    return shared;
}

RemoteInspectorServer::~RemoteInspectorServer()
{
    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    endpoint.invalidateListener(*this);
}

bool RemoteInspectorServer::start(const char* address, uint16_t port)
{
    if (isRunning())
        return false;

    auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    m_server = endpoint.listenInet(address, port, *this);
    return isRunning();
}

std::optional<uint16_t> RemoteInspectorServer::getPort() const
{
    if (!isRunning())
        return std::nullopt;

    const auto& endpoint = Inspector::RemoteInspectorSocketEndpoint::singleton();
    return endpoint.getPort(m_server.value());
}

std::optional<ConnectionID> RemoteInspectorServer::doAccept(RemoteInspectorSocketEndpoint& endpoint, PlatformSocketType socket)
{
    ASSERT(!isMainThread());

    auto& inspector = RemoteInspector::singleton();
    if (inspector.isConnected()) {
        LOG_ERROR("RemoteInspector can accept only 1 client");
        return std::nullopt;
    }

    if (auto newID = endpoint.createClient(socket, inspector)) {
        inspector.connect(newID.value());
        return newID;
    }

    return std::nullopt;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
