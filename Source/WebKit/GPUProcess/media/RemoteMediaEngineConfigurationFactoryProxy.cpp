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
#include "RemoteMediaEngineConfigurationFactoryProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "SharedPreferencesForWebProcess.h"
#include <WebCore/PlatformMediaCapabilitiesDecodingInfo.h>
#include <WebCore/PlatformMediaCapabilitiesEncodingInfo.h>
#include <WebCore/PlatformMediaDecodingConfiguration.h>
#include <WebCore/PlatformMediaEncodingConfiguration.h>
#include <WebCore/PlatformMediaEngineConfigurationFactory.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaEngineConfigurationFactoryProxy);

RemoteMediaEngineConfigurationFactoryProxy::RemoteMediaEngineConfigurationFactoryProxy(GPUConnectionToWebProcess& connection)
    : m_connection(connection)
{
}

RemoteMediaEngineConfigurationFactoryProxy::~RemoteMediaEngineConfigurationFactoryProxy() = default;

void RemoteMediaEngineConfigurationFactoryProxy:: createDecodingConfiguration(WebCore::PlatformMediaDecodingConfiguration&& configuration, CompletionHandler<void(WebCore::PlatformMediaCapabilitiesDecodingInfo&&)>&& completion)
{
    WebCore::PlatformMediaEngineConfigurationFactory::createDecodingConfiguration(WTF::move(configuration), [completion = WTF::move(completion)] (auto info) mutable {
        completion(WTF::move(info));
    });
}

void RemoteMediaEngineConfigurationFactoryProxy::createEncodingConfiguration(WebCore::PlatformMediaEncodingConfiguration&& configuration, CompletionHandler<void(WebCore::PlatformMediaCapabilitiesEncodingInfo&&)>&& completion)
{
    WebCore::PlatformMediaEngineConfigurationFactory::createEncodingConfiguration(WTF::move(configuration), [completion = WTF::move(completion)] (auto info) mutable {
        completion(WTF::move(info));
    });
}

void RemoteMediaEngineConfigurationFactoryProxy::ref() const
{
    m_connection.get()->ref();
}

void RemoteMediaEngineConfigurationFactoryProxy::deref() const
{
    m_connection.get()->deref();
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaEngineConfigurationFactoryProxy::sharedPreferencesForWebProcess() const
{
    return m_connection.get()->sharedPreferencesForWebProcess();
}

}

#endif
