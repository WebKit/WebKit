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

namespace WebKit {

WebSWRegistrationStore::WebSWRegistrationStore(WebCore::SWServer& server, NetworkStorageManager& manager)
    : m_server(server)
    , m_manager(manager)
    , m_updateTimer(*this, &WebSWRegistrationStore::updateTimerFired)
{
    ASSERT(RunLoop::isMain());
}

CheckedPtr<NetworkStorageManager> WebSWRegistrationStore::checkedManager() const
{
    return m_manager.get();
}

RefPtr<WebCore::SWServer> WebSWRegistrationStore::protectedServer() const
{
    return m_server.get();
}

void WebSWRegistrationStore::clearAll(CompletionHandler<void()>&& callback)
{
    m_updates.clear();
    m_updateTimer.stop();
    if (!m_manager)
        return callback();

    checkedManager()->clearServiceWorkerRegistrations(WTFMove(callback));
}

void WebSWRegistrationStore::flushChanges(CompletionHandler<void()>&& callback)
{
    if (m_updateTimer.isActive())
        m_updateTimer.stop();

    updateToStorage(WTFMove(callback));
}

void WebSWRegistrationStore::closeFiles(CompletionHandler<void()>&& callback)
{
    if (!m_manager)
        return callback();

    checkedManager()->closeServiceWorkerRegistrationFiles(WTFMove(callback));
}

void WebSWRegistrationStore::importRegistrations(CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerContextData>>)>&& callback)
{
    if (!m_manager)
        return callback(std::nullopt);

    checkedManager()->importServiceWorkerRegistrations(WTFMove(callback));
}

void WebSWRegistrationStore::updateRegistration(const WebCore::ServiceWorkerContextData& registration)
{
    m_updates.set(registration.registration.key, registration);
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
            registrationsToUpdate.append(WTFMove(*registation));
    }
    m_updates.clear();

    if (!m_manager)
        return callback();

    checkedManager()->updateServiceWorkerRegistrations(WTFMove(registrationsToUpdate), WTFMove(registrationsToDelete), [this, weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto&& result) mutable {
        ASSERT(RunLoop::isMain());

        if (!weakThis || !m_server || !result)
            return callback();

        auto allScripts = WTFMove(result.value());
        for (auto&& scripts : allScripts)
            protectedServer()->didSaveWorkerScriptsToDisk(scripts.identifier, WTFMove(scripts.mainScript), WTFMove(scripts.importedScripts));

        callback();
    });
}

} // namespace WebKit
