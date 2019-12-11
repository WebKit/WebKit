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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "RegistrationDatabase.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerRegistrationData.h"
#include "ServiceWorkerRegistrationKey.h"
#include "Timer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SWServer;
class SWServerRegistration;

class RegistrationStore : public CanMakeWeakPtr<RegistrationStore> {
WTF_MAKE_FAST_ALLOCATED;
public:
    RegistrationStore(SWServer&, String&& databaseDirectory);
    ~RegistrationStore();

    void clearAll(CompletionHandler<void()>&&);
    void flushChanges(CompletionHandler<void()>&&);

    void closeDatabase(CompletionHandler<void()>&&);
    void startSuspension(CompletionHandler<void()>&&);
    void endSuspension();

    // Callbacks from the SWServer
    void updateRegistration(const ServiceWorkerContextData&);
    void removeRegistration(const ServiceWorkerRegistrationKey&);

    // Callbacks from the database
    void addRegistrationFromDatabase(ServiceWorkerContextData&&);
    void databaseFailedToOpen();
    void databaseOpenedAndRecordsImported();

    SWServer& server() { return m_server; };

private:
    void scheduleDatabasePushIfNecessary();
    void pushChangesToDatabase(CompletionHandler<void()>&&);
    void pushChangesToDatabase() { pushChangesToDatabase({ }); }

    SWServer& m_server;
    Ref<RegistrationDatabase> m_database;

    HashMap<ServiceWorkerRegistrationKey, Optional<ServiceWorkerContextData>> m_updatedRegistrations;
    Timer m_databasePushTimer;

    bool m_isSuspended { false };
    bool m_needsPushingChanges { false };
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
