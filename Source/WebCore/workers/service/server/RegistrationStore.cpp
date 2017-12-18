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

RegistrationStore::RegistrationStore(SWServer& server, const String& databaseDirectory)
    : m_server(server)
    , m_database(*this, databaseDirectory)
    , m_databasePushTimer(*this, &RegistrationStore::pushChangesToDatabase)
{
}

RegistrationStore::~RegistrationStore()
{
    ASSERT(m_database.isClosed());
}

void RegistrationStore::scheduleDatabasePushIfNecessary()
{
    if (m_databasePushTimer.isActive())
        return;
    m_databasePushTimer.startOneShot(0_s);
}

void RegistrationStore::pushChangesToDatabase(WTF::CompletionHandler<void()>&& completionHandler)
{
    Vector<ServiceWorkerContextData> changesToPush;
    changesToPush.reserveInitialCapacity(m_updatedRegistrations.size());
    for (auto& value : m_updatedRegistrations.values())
        changesToPush.uncheckedAppend(WTFMove(value));

    m_updatedRegistrations.clear();
    m_database.pushChanges(WTFMove(changesToPush), WTFMove(completionHandler));
}

void RegistrationStore::clearAll(WTF::CompletionHandler<void()>&& completionHandler)
{
    m_updatedRegistrations.clear();
    m_databasePushTimer.stop();
    m_database.clearAll(WTFMove(completionHandler));
}

void RegistrationStore::flushChanges(WTF::CompletionHandler<void()>&& completionHandler)
{
    if (m_databasePushTimer.isActive()) {
        pushChangesToDatabase(WTFMove(completionHandler));
        m_databasePushTimer.stop();
        return;
    }
    completionHandler();
}

void RegistrationStore::updateRegistration(const ServiceWorkerContextData& data)
{
    m_updatedRegistrations.set(data.registration.key, data);
    scheduleDatabasePushIfNecessary();
}

void RegistrationStore::removeRegistration(SWServerRegistration& registration)
{
    ServiceWorkerContextData contextData;
    contextData.registration.key = registration.key();
    m_updatedRegistrations.set(registration.key(), WTFMove(contextData));
    scheduleDatabasePushIfNecessary();
}

void RegistrationStore::addRegistrationFromDatabase(ServiceWorkerContextData&& context)
{
    m_server.addRegistrationFromStore(WTFMove(context));
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
