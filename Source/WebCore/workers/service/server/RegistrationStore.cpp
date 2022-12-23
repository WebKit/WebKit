/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RegistrationStore.h"

#if ENABLE(SERVICE_WORKER)

#include "SWServerRegistration.h"

namespace WebCore {

RegistrationStore::RegistrationStore(SWServer& server, String&& databaseDirectory)
    : m_server(server)
    , m_database(RegistrationDatabase::create(*this, WTFMove(databaseDirectory)))
    , m_databasePushTimer(*this, &RegistrationStore::pushChangesToDatabase)
{
}

RegistrationStore::~RegistrationStore()
{
}

void RegistrationStore::scheduleDatabasePushIfNecessary()
{
    if (m_databasePushTimer.isActive())
        return;
    m_databasePushTimer.startOneShot(0_s);
}

void RegistrationStore::pushChangesToDatabase(CompletionHandler<void()>&& completionHandler)
{
    if (m_isSuspended) {
        m_needsPushingChanges = true;
        return;
    }

    m_database->pushChanges(m_updatedRegistrations, WTFMove(completionHandler));
    m_updatedRegistrations.clear();
}

void RegistrationStore::clearAll(CompletionHandler<void()>&& completionHandler)
{
    m_needsPushingChanges = false;
    m_updatedRegistrations.clear();
    m_databasePushTimer.stop();
    m_database->clearAll(WTFMove(completionHandler));
}

void RegistrationStore::flushChanges(CompletionHandler<void()>&& completionHandler)
{
    if (m_databasePushTimer.isActive()) {
        pushChangesToDatabase(WTFMove(completionHandler));
        m_databasePushTimer.stop();
        return;
    }
    completionHandler();
}

void RegistrationStore::startSuspension(CompletionHandler<void()>&& completionHandler)
{
    m_isSuspended = true;
    closeDatabase(WTFMove(completionHandler));
}

void RegistrationStore::closeDatabase(CompletionHandler<void()>&& completionHandler)
{
    m_database->close(WTFMove(completionHandler));
}

void RegistrationStore::endSuspension()
{
    m_isSuspended = false;
    if (m_needsPushingChanges)
        scheduleDatabasePushIfNecessary();
}

void RegistrationStore::updateRegistration(const ServiceWorkerContextData& data)
{
    ASSERT(isMainThread());
    ASSERT(!data.registration.key.isEmpty());
    if (data.registration.key.isEmpty() || data.serviceWorkerPageIdentifier)
        return;

    m_updatedRegistrations.set(data.registration.key, data);
    scheduleDatabasePushIfNecessary();
}

void RegistrationStore::removeRegistration(const ServiceWorkerRegistrationKey& key)
{
    ASSERT(isMainThread());
    ASSERT(!key.isEmpty());
    if (key.isEmpty())
        return;

    m_updatedRegistrations.set(key, std::nullopt);
    scheduleDatabasePushIfNecessary();
}

void RegistrationStore::addRegistrationFromDatabase(ServiceWorkerContextData&& data, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());
    ASSERT(!data.registration.key.isEmpty());
    if (data.registration.key.isEmpty())
        return completionHandler();

    m_server.addRegistrationFromStore(WTFMove(data), WTFMove(completionHandler));
}

void RegistrationStore::didSaveWorkerScriptsToDisk(ServiceWorkerIdentifier serviceWorkerIdentifier, ScriptBuffer&& mainScript, MemoryCompactRobinHoodHashMap<URL, ScriptBuffer>&& importedScripts)
{
    m_server.didSaveWorkerScriptsToDisk(serviceWorkerIdentifier, WTFMove(mainScript), WTFMove(importedScripts));
}

void RegistrationStore::databaseFailedToOpen()
{
    m_server.registrationStoreDatabaseFailedToOpen();
}

void RegistrationStore::databaseOpenedAndRecordsImported()
{
    m_server.registrationStoreImportComplete();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
