/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Igalia S.L.
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
#include "LegacyCustomProtocolManager.h"

#include "LegacyCustomProtocolManagerMessages.h"
#include "LegacyCustomProtocolManagerProxyMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessCreationParameters.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyCustomProtocolManager);

ASCIILiteral LegacyCustomProtocolManager::supplementName()
{
    return "LegacyCustomProtocolManager"_s;
}

LegacyCustomProtocolManager::LegacyCustomProtocolManager(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
    m_networkProcess->addMessageReceiver(Messages::LegacyCustomProtocolManager::messageReceiverName(), *this);
}

void LegacyCustomProtocolManager::ref() const
{
    m_networkProcess->ref();
}

void LegacyCustomProtocolManager::deref() const
{
    m_networkProcess->deref();
}

Ref<NetworkProcess> LegacyCustomProtocolManager::protectedNetworkProcess() const
{
    ASSERT(RunLoop::isMain());
    return m_networkProcess.get();
}

void LegacyCustomProtocolManager::initialize(const NetworkProcessCreationParameters& parameters)
{
    registerProtocolClass();

    for (const auto& scheme : parameters.urlSchemesRegisteredForCustomProtocols)
        registerScheme(scheme);
}

LegacyCustomProtocolID LegacyCustomProtocolManager::addCustomProtocol(CustomProtocol&& customProtocol)
{
    Locker locker { m_customProtocolMapLock };
    auto customProtocolID = LegacyCustomProtocolID::generate();
    m_customProtocolMap.add(customProtocolID, WTFMove(customProtocol));
    return customProtocolID;
}

void LegacyCustomProtocolManager::removeCustomProtocol(LegacyCustomProtocolID customProtocolID)
{
    Locker locker { m_customProtocolMapLock };
    m_customProtocolMap.remove(customProtocolID);
}

void LegacyCustomProtocolManager::startLoading(LegacyCustomProtocolID customProtocolID, const WebCore::ResourceRequest& request)
{
    protectedNetworkProcess()->send(Messages::LegacyCustomProtocolManagerProxy::StartLoading(customProtocolID, request));
}

void LegacyCustomProtocolManager::stopLoading(LegacyCustomProtocolID customProtocolID)
{
    protectedNetworkProcess()->send(Messages::LegacyCustomProtocolManagerProxy::StopLoading(customProtocolID), 0);
}

} // namespace WebKit
