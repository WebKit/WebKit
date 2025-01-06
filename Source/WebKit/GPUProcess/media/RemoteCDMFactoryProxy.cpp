/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "RemoteCDMFactoryProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "GPUProcess.h"
#include "RemoteCDMConfiguration.h"
#include "RemoteCDMInstanceProxy.h"
#include "RemoteCDMInstanceSessionProxy.h"
#include "RemoteCDMProxy.h"
#include <WebCore/CDMFactory.h>
#include <wtf/Algorithms.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCDMFactoryProxy);

RemoteCDMFactoryProxy::RemoteCDMFactoryProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteCDMFactoryProxy::~RemoteCDMFactoryProxy()
{
    clear();
}

void RemoteCDMFactoryProxy::clear()
{
    m_proxies.clear();
}

static CDMFactory* factoryForKeySystem(const String& keySystem)
{
    auto& factories = CDMFactory::registeredFactories();
    auto foundIndex = factories.findIf([&] (auto& factory) { return factory->supportsKeySystem(keySystem); });
    if (foundIndex == notFound)
        return nullptr;
    return factories[foundIndex];
}

void RemoteCDMFactoryProxy::createCDM(const String& keySystem, CompletionHandler<void(std::optional<RemoteCDMIdentifier>&&, RemoteCDMConfiguration&&)>&& completion)
{
    auto factory = factoryForKeySystem(keySystem);
    if (!factory) {
        completion(std::nullopt, { });
        return;
    }

    auto privateCDM = factory->createCDM(keySystem, *this);
    if (!privateCDM) {
        completion(std::nullopt, { });
        return;
    }

    auto proxy = RemoteCDMProxy::create(*this, WTFMove(privateCDM));
    auto identifier = RemoteCDMIdentifier::generate();
    RemoteCDMConfiguration configuration = proxy->configuration();
    addProxy(identifier, WTFMove(proxy));
    completion(WTFMove(identifier), WTFMove(configuration));
}

void RemoteCDMFactoryProxy::supportsKeySystem(const String& keySystem, CompletionHandler<void(bool)>&& completion)
{
    completion(factoryForKeySystem(keySystem));
}

void RemoteCDMFactoryProxy::didReceiveCDMMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (ObjectIdentifier<RemoteCDMIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr proxy = m_proxies.get(ObjectIdentifier<RemoteCDMIdentifierType>(decoder.destinationID())))
            proxy->didReceiveMessage(connection, decoder);
    }
}

void RemoteCDMFactoryProxy::didReceiveCDMInstanceMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (ObjectIdentifier<RemoteCDMInstanceIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr instance = m_instances.get(ObjectIdentifier<RemoteCDMInstanceIdentifierType>(decoder.destinationID())))
            instance->didReceiveMessage(connection, decoder);
    }
}

void RemoteCDMFactoryProxy::didReceiveCDMInstanceSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (ObjectIdentifier<RemoteCDMInstanceSessionIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr session = m_sessions.get(ObjectIdentifier<RemoteCDMInstanceSessionIdentifierType>(decoder.destinationID())))
            session->didReceiveMessage(connection, decoder);
    }
}

bool RemoteCDMFactoryProxy::didReceiveSyncCDMMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (ObjectIdentifier<RemoteCDMIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr proxy = m_proxies.get(ObjectIdentifier<RemoteCDMIdentifierType>(decoder.destinationID())))
            return proxy->didReceiveSyncMessage(connection, decoder, encoder);
    }
    return false;
}

bool RemoteCDMFactoryProxy::didReceiveSyncCDMInstanceMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (ObjectIdentifier<RemoteCDMInstanceIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr instance = m_instances.get(ObjectIdentifier<RemoteCDMInstanceIdentifierType>(decoder.destinationID())))
            return instance->didReceiveSyncMessage(connection, decoder, encoder);
    }
    return false;
}

bool RemoteCDMFactoryProxy::didReceiveSyncCDMInstanceSessionMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (ObjectIdentifier<RemoteCDMInstanceSessionIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr session = m_sessions.get(ObjectIdentifier<RemoteCDMInstanceSessionIdentifierType>(decoder.destinationID())))
            return session->didReceiveSyncMessage(connection, decoder, encoder);
    }
    return false;
}

void RemoteCDMFactoryProxy::addProxy(const RemoteCDMIdentifier& identifier, RefPtr<RemoteCDMProxy>&& proxy)
{
    ASSERT(!m_proxies.contains(identifier));
    m_proxies.set(identifier, WTFMove(proxy));
}

void RemoteCDMFactoryProxy::removeProxy(const RemoteCDMIdentifier& identifier)
{
    ASSERT(m_proxies.contains(identifier));
    m_proxies.remove(identifier);
}

void RemoteCDMFactoryProxy::addInstance(const RemoteCDMInstanceIdentifier& identifier, Ref<RemoteCDMInstanceProxy>&& instance)
{
    ASSERT(!m_instances.contains(identifier));
    m_instances.set(identifier, WTFMove(instance));
}

void RemoteCDMFactoryProxy::removeInstance(const RemoteCDMInstanceIdentifier& identifier)
{
    ASSERT(m_instances.contains(identifier));
    m_instances.remove(identifier);
    auto connection = m_gpuConnectionToWebProcess.get();
    if (connection && allowsExitUnderMemoryPressure())
        connection->protectedGPUProcess()->tryExitIfUnusedAndUnderMemoryPressure();
}

RemoteCDMInstanceProxy* RemoteCDMFactoryProxy::getInstance(const RemoteCDMInstanceIdentifier& identifier)
{
    return m_instances.get(identifier);
}

void RemoteCDMFactoryProxy::addSession(const RemoteCDMInstanceSessionIdentifier& identifier, Ref<RemoteCDMInstanceSessionProxy>&& session)
{
    ASSERT(!m_sessions.contains(identifier));
    m_sessions.set(identifier, WTFMove(session));
}

void RemoteCDMFactoryProxy::removeSession(const RemoteCDMInstanceSessionIdentifier& identifier)
{
    ASSERT(m_sessions.contains(identifier));
    m_sessions.remove(identifier);
}

bool RemoteCDMFactoryProxy::allowsExitUnderMemoryPressure() const
{
    return m_instances.isEmpty();
}

const String& RemoteCDMFactoryProxy::mediaKeysStorageDirectory() const
{
    if (RefPtr connection = m_gpuConnectionToWebProcess.get())
        return connection->mediaKeysStorageDirectory();
    return emptyString();
}

#if !RELEASE_LOG_DISABLED
const Logger& RemoteCDMFactoryProxy::logger() const
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        RefPtr connection { m_gpuConnectionToWebProcess.get() };
        m_logger->setEnabled(this, connection && connection->isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}
#endif

std::optional<SharedPreferencesForWebProcess> RemoteCDMFactoryProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

}

#endif
