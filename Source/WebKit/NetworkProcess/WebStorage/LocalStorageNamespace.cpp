/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "LocalStorageNamespace.h"

#include "StorageArea.h"
#include "StorageManager.h"
#include <WebCore/SecurityOriginData.h>

namespace WebKit {

using namespace WebCore;

// We should investigate a way to share it with WebCore.
LocalStorageNamespace::LocalStorageNamespace(StorageManager& storageManager, StorageNamespaceIdentifier storageNamespaceID)
    : m_storageManager(storageManager)
    , m_quotaInBytes(StorageManager::localStorageDatabaseQuotaInBytes)
{
    ASSERT(!RunLoop::isMain());
}

LocalStorageNamespace::~LocalStorageNamespace()
{
    ASSERT(!RunLoop::isMain());
}

StorageArea& LocalStorageNamespace::getOrCreateStorageArea(SecurityOriginData&& securityOrigin, IsEphemeral isEphemeral, Ref<WorkQueue>&& workQueue)
{
    ASSERT(!RunLoop::isMain());
    return *m_storageAreaMap.ensure(securityOrigin, [&]() mutable {
        return makeUnique<StorageArea>(isEphemeral == IsEphemeral::Yes ? nullptr : this, WTFMove(securityOrigin), m_quotaInBytes, WTFMove(workQueue));
    }).iterator->value.get();
}

void LocalStorageNamespace::clearStorageAreasMatchingOrigin(const SecurityOriginData& securityOrigin)
{
    ASSERT(!RunLoop::isMain());
    auto originAndStorageArea = m_storageAreaMap.find(securityOrigin);
    if (originAndStorageArea != m_storageAreaMap.end())
        originAndStorageArea->value->clear();
}

void LocalStorageNamespace::clearAllStorageAreas()
{
    ASSERT(!RunLoop::isMain());
    for (auto& storageArea : m_storageAreaMap.values())
        storageArea->clear();
}

Vector<SecurityOriginData> LocalStorageNamespace::ephemeralOrigins() const
{
    ASSERT(!RunLoop::isMain());
    Vector<SecurityOriginData> origins;
    for (const auto& storageArea : m_storageAreaMap.values()) {
        if (!storageArea->items().isEmpty())
            origins.append(storageArea->securityOrigin());
    }
    return origins;
}

} // namespace WebKit
