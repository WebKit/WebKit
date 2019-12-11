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
#include "WebCoreArgumentCoders.h"
#include <WebCore/ResourceRequest.h>

namespace WebKit {

static uint64_t generateCustomProtocolID()
{
    static uint64_t uniqueCustomProtocolID = 0;
    return ++uniqueCustomProtocolID;
}

const char* LegacyCustomProtocolManager::supplementName()
{
    return "LegacyCustomProtocolManager";
}

LegacyCustomProtocolManager::LegacyCustomProtocolManager(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
    m_networkProcess.addMessageReceiver(Messages::LegacyCustomProtocolManager::messageReceiverName(), *this);
}

void LegacyCustomProtocolManager::initialize(const NetworkProcessCreationParameters& parameters)
{
    registerProtocolClass();

    for (const auto& scheme : parameters.urlSchemesRegisteredForCustomProtocols)
        registerScheme(scheme);
}

uint64_t LegacyCustomProtocolManager::addCustomProtocol(CustomProtocol&& customProtocol)
{
    LockHolder locker(m_customProtocolMapMutex);
    auto customProtocolID = generateCustomProtocolID();
    m_customProtocolMap.add(customProtocolID, WTFMove(customProtocol));
    return customProtocolID;
}

void LegacyCustomProtocolManager::removeCustomProtocol(uint64_t customProtocolID)
{
    LockHolder locker(m_customProtocolMapMutex);
    m_customProtocolMap.remove(customProtocolID);
}

void LegacyCustomProtocolManager::startLoading(uint64_t customProtocolID, const WebCore::ResourceRequest& request)
{
    m_networkProcess.send(Messages::LegacyCustomProtocolManagerProxy::StartLoading(customProtocolID, request));
}

void LegacyCustomProtocolManager::stopLoading(uint64_t customProtocolID)
{
    m_networkProcess.send(Messages::LegacyCustomProtocolManagerProxy::StopLoading(customProtocolID), 0);
}

} // namespace WebKit
