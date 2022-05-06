/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteCDM.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUProcessConnection.h"
#include "RemoteCDMInstance.h"
#include "RemoteCDMInstanceConfiguration.h"
#include "RemoteCDMInstanceIdentifier.h"
#include "RemoteCDMProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/CDMKeySystemConfiguration.h>
#include <WebCore/CDMRestrictions.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteCDM> RemoteCDM::create(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMIdentifier&& identifier, RemoteCDMConfiguration&& configuration)
{
    return std::unique_ptr<RemoteCDM>(new RemoteCDM(WTFMove(factory), WTFMove(identifier), WTFMove(configuration)));
}

RemoteCDM::RemoteCDM(WeakPtr<RemoteCDMFactory>&& factory, RemoteCDMIdentifier&& identifier, RemoteCDMConfiguration&& configuration)
    : m_factory(WTFMove(factory))
    , m_identifier(WTFMove(identifier))
    , m_configuration(WTFMove(configuration))
{
}

void RemoteCDM::getSupportedConfiguration(CDMKeySystemConfiguration&& configuration, LocalStorageAccess access, SupportedConfigurationCallback&& callback)
{
    if (!m_factory) {
        callback(std::nullopt);
        return;
    }

    m_factory->gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteCDMProxy::GetSupportedConfiguration(WTFMove(configuration), access), WTFMove(callback), m_identifier);
}


bool RemoteCDM::supportsConfiguration(const CDMKeySystemConfiguration&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool RemoteCDM::supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration&, const CDMRestrictions&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool RemoteCDM::supportsSessionTypeWithConfiguration(const CDMSessionType&, const CDMKeySystemConfiguration&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool RemoteCDM::supportsInitData(const AtomString& type, const SharedBuffer& data) const
{
    // This check will be done, later, inside RemoteCDMInstanceSessionProxy::requestLicense().
    return true;
}

CDMRequirement RemoteCDM::distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration& configuration, const CDMRestrictions& restrictions) const
{
    ASSERT_NOT_REACHED();
    return CDMRequirement::NotAllowed;
}

CDMRequirement RemoteCDM::persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const
{
    ASSERT_NOT_REACHED();
    return CDMRequirement::NotAllowed;
}

bool RemoteCDM::distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

RefPtr<CDMInstance> RemoteCDM::createInstance()
{
    if (!m_factory)
        return nullptr;

    RemoteCDMInstanceIdentifier identifier;
    RemoteCDMInstanceConfiguration configuration;
    m_factory->gpuProcessConnection().connection().sendSync(Messages::RemoteCDMProxy::CreateInstance(), Messages::RemoteCDMProxy::CreateInstance::Reply(identifier, configuration), m_identifier);
    if (!identifier)
        return nullptr;
    return RemoteCDMInstance::create(m_factory.get(), WTFMove(identifier), WTFMove(configuration));
}

void RemoteCDM::loadAndInitialize()
{
    if (!m_factory)
        return;

    m_factory->gpuProcessConnection().connection().send(Messages::RemoteCDMProxy::LoadAndInitialize(), m_identifier);
}

RefPtr<SharedBuffer> RemoteCDM::sanitizeResponse(const SharedBuffer& response) const
{
    // This check will be done, later, inside RemoteCDMInstanceSessionProxy::updateLicense().
    return response.makeContiguous();
}

std::optional<String> RemoteCDM::sanitizeSessionId(const String& sessionId) const
{
    // This check will be done, later, inside RemoteCDMInstanceSessionProxy::loadSession().
    return sessionId;
}

}

#endif
