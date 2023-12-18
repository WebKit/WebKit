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
#include "ServiceWorkerStorageManager.h"

#include <WebCore/SWRegistrationDatabase.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/ServiceWorkerRegistrationKey.h>

namespace WebKit {

ServiceWorkerStorageManager::ServiceWorkerStorageManager(const String& path)
    : m_path(path)
{
}

WebCore::SWRegistrationDatabase* ServiceWorkerStorageManager::ensureDatabase()
{
    if (!m_database && !m_path.isEmpty())
        m_database = makeUnique<WebCore::SWRegistrationDatabase>(m_path);

    return m_database.get();
}

void ServiceWorkerStorageManager::closeFiles()
{
    m_database = nullptr;
}

void ServiceWorkerStorageManager::clearAllRegistrations()
{
    if (auto database = ensureDatabase())
        database->clearAllRegistrations();
}

std::optional<Vector<WebCore::ServiceWorkerContextData>> ServiceWorkerStorageManager::importRegistrations()
{
    if (auto database = ensureDatabase())
        return database->importRegistrations();

    return std::nullopt;
}

std::optional<Vector<WebCore::ServiceWorkerScripts>> ServiceWorkerStorageManager::updateRegistrations(const Vector<WebCore::ServiceWorkerContextData>& registrationsToUpdate, const Vector<WebCore::ServiceWorkerRegistrationKey>& registrationsToDelete)
{
    if (auto database = ensureDatabase())
        return database->updateRegistrations(registrationsToUpdate, registrationsToDelete);

    return std::nullopt;
}

} // namespace WebKit
