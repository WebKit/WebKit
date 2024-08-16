/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ModelProcessModelPlayerManagerProxy.h"

#if ENABLE(MODEL_PROCESS)

#include "ModelProcessModelPlayerProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelProcessModelPlayerManagerProxy);

ModelProcessModelPlayerManagerProxy::ModelProcessModelPlayerManagerProxy(ModelConnectionToWebProcess& connection)
    : m_modelConnectionToWebProcess(connection)
{
}

ModelProcessModelPlayerManagerProxy::~ModelProcessModelPlayerManagerProxy()
{
    clear();
}

void ModelProcessModelPlayerManagerProxy::clear()
{
    auto proxies = std::exchange(m_proxies, { });

    for (auto& proxy : proxies.values())
        proxy->invalidate();
}

void ModelProcessModelPlayerManagerProxy::createModelPlayer(WebCore::ModelPlayerIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_modelConnectionToWebProcess);
    ASSERT(!m_proxies.contains(identifier));

    auto proxy = ModelProcessModelPlayerProxy::create(*this, identifier, m_modelConnectionToWebProcess->protectedConnection());
    m_proxies.add(identifier, WTFMove(proxy));
}

void ModelProcessModelPlayerManagerProxy::deleteModelPlayer(WebCore::ModelPlayerIdentifier identifier)
{
    ASSERT(RunLoop::isMain());

    if (auto proxy = m_proxies.take(identifier))
        proxy->invalidate();

    if (m_modelConnectionToWebProcess)
        m_modelConnectionToWebProcess->modelProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

void ModelProcessModelPlayerManagerProxy::didReceivePlayerMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(RunLoop::isMain());
    if (auto* player = m_proxies.get(WebCore::ModelPlayerIdentifier(decoder.destinationID())))
        player->didReceiveMessage(connection, decoder);
}

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
