/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebSWRegistrationStore.h"

#include "NetworkStorageManager.h"
#include <WebCore/SWServer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSWRegistrationStore);

Ref<WebSWRegistrationStore> WebSWRegistrationStore::create(WebCore::SWServer& server, NetworkStorageManager& manager)
{
    return adoptRef(*new WebSWRegistrationStore(server, manager));
}

WebSWRegistrationStore::WebSWRegistrationStore(WebCore::SWServer& server, NetworkStorageManager& manager)
    : m_server(server)
    , m_manager(manager)
    , m_updateTimer(*this, &WebSWRegistrationStore::updateTimerFired)
{
    ASSERT(RunLoop::isMain());
}

void WebSWRegistrationStore::clearAll(CompletionHandler<void()>&& callback)
{
    m_updates.clear();
    m_updateTimer.stop();

    if (RefPtr manager = m_manager.get())
        manager->clearServiceWorkerRegistrations(WTF::move(callback));
    else
        callback();
}

void WebSWRegistrationStore::flushChanges(CompletionHandler<void()>&& callback)
{
    if (m_updateTimer.isActive())
        m_updateTimer.stop();

    updateToStorage(WTF::move(callback));
}

void WebSWRegistrationStore::closeFiles(CompletionHandler<void()>&& callback)
{
    if (RefPtr manager = m_manager.get())
        manager->closeServiceWorkerRegistrationFiles(WTF::move(callback));
    else
        callback();
}

void WebSWRegistrationStore::importRegistrations(CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerContextData>>&&)>&& callback)
{
    if (RefPtr manager = m_manager.get())
        manager->importServiceWorkerRegistrations(WTF::move(callback));
    else
        callback(std::nullopt);
}

void WebSWRegistrationStore::updateRegistration(const WebCore::ServiceWorkerContextData& registration)
{
    m_updates.set(registration.registration.key, registration.copy());
    scheduleUpdateIfNecessary();
}

void WebSWRegistrationStore::removeRegistration(const WebCore::ServiceWorkerRegistrationKey& key)
{
    m_updates.set(key, std::nullopt);
    scheduleUpdateIfNecessary();
}

void WebSWRegistrationStore::scheduleUpdateIfNecessary()
{
    ASSERT(RunLoop::isMain());

    if (m_updateTimer.isActive())
        return;

    m_updateTimer.startOneShot(0_s);
}

void WebSWRegistrationStore::updateToStorage(CompletionHandler<void()>&& callback)
{
    ASSERT(RunLoop::isMain());

    Vector<WebCore::ServiceWorkerRegistrationKey> registrationsToDelete;
    Vector<WebCore::ServiceWorkerContextData> registrationsToUpdate;
    for (auto& [key, registation] : m_updates) {
        if (!registation)
            registrationsToDelete.append(key);
        else
            registrationsToUpdate.append(WTF::move(*registation));
    }
    m_updates.clear();

    RefPtr manager = m_manager.get();
    if (!manager)
        return callback();

    manager->updateServiceWorkerRegistrations(WTF::move(registrationsToUpdate), WTF::move(registrationsToDelete), [weakThis = WeakPtr { *this }, callback = WTF::move(callback)](auto&& result) mutable {
        ASSERT(RunLoop::isMain());

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_server || !result)
            return callback();

        auto allScripts = WTF::move(result.value());
        for (auto&& scripts : allScripts)
            protect(protectedThis->m_server)->didSaveWorkerScriptsToDisk(scripts.identifier, WTF::move(scripts.mainScript), WTF::move(scripts.importedScripts));

        callback();
    });
}

} // namespace WebKit
