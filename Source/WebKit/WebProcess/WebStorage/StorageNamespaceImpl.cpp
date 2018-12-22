/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "StorageNamespaceImpl.h"

#include "StorageAreaImpl.h"
#include "StorageAreaMap.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/PageGroup.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageType.h>

namespace WebKit {
using namespace WebCore;

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createSessionStorageNamespace(uint64_t identifier, unsigned quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Session, identifier, nullptr, quotaInBytes));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createEphemeralLocalStorageNamespace(uint64_t identifier, unsigned quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::EphemeralLocal, identifier, nullptr, quotaInBytes));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createLocalStorageNamespace(uint64_t identifier, unsigned quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Local, identifier, nullptr, quotaInBytes));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createTransientLocalStorageNamespace(uint64_t identifier, WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::TransientLocal, identifier, &topLevelOrigin, quotaInBytes));
}

StorageNamespaceImpl::StorageNamespaceImpl(WebCore::StorageType storageType, uint64_t storageNamespaceID, WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes)
    : m_storageType(storageType)
    , m_storageNamespaceID(storageNamespaceID)
    , m_topLevelOrigin(topLevelOrigin)
    , m_quotaInBytes(quotaInBytes)
{
}

StorageNamespaceImpl::~StorageNamespaceImpl()
{
}

void StorageNamespaceImpl::didDestroyStorageAreaMap(StorageAreaMap& map)
{
    m_storageAreaMaps.remove(map.securityOrigin().data());
}

Ref<StorageArea> StorageNamespaceImpl::storageArea(const SecurityOriginData& securityOrigin)
{
    if (m_storageType == StorageType::EphemeralLocal)
        return ephemeralLocalStorageArea(securityOrigin);

    RefPtr<StorageAreaMap> map;

    auto& slot = m_storageAreaMaps.add(securityOrigin, nullptr).iterator->value;
    if (!slot) {
        map = StorageAreaMap::create(this, securityOrigin.securityOrigin());
        slot = map.get();
    } else
        map = slot;

    return StorageAreaImpl::create(map.releaseNonNull());
}

class StorageNamespaceImpl::EphemeralStorageArea final : public StorageArea {
public:
    static Ref<EphemeralStorageArea> create(const SecurityOriginData& origin, unsigned quotaInBytes)
    {
        return adoptRef(*new EphemeralStorageArea(origin, quotaInBytes));
    }

    Ref<EphemeralStorageArea> copy()
    {
        return adoptRef(*new EphemeralStorageArea(*this));
    }

private:
    EphemeralStorageArea(const SecurityOriginData& origin, unsigned quotaInBytes)
        : m_securityOriginData(origin)
        , m_storageMap(StorageMap::create(quotaInBytes))
    {
    }

    EphemeralStorageArea(EphemeralStorageArea& other)
        : m_securityOriginData(other.m_securityOriginData)
        , m_storageMap(other.m_storageMap)
    {
    }

    // WebCore::StorageArea.
    unsigned length()
    {
        return m_storageMap->length();
    }

    String key(unsigned index)
    {
        return m_storageMap->key(index);
    }

    String item(const String& key)
    {
        return m_storageMap->getItem(key);
    }

    void setItem(Frame*, const String& key, const String& value, bool& quotaException)
    {
        String oldValue;
        if (auto newMap = m_storageMap->setItem(key, value, oldValue, quotaException))
            m_storageMap = WTFMove(newMap);
    }

    void removeItem(Frame*, const String& key)
    {
        String oldValue;
        if (auto newMap = m_storageMap->removeItem(key, oldValue))
            m_storageMap = WTFMove(newMap);
    }

    void clear(Frame*)
    {
        if (!m_storageMap->length())
            return;

        m_storageMap = StorageMap::create(m_storageMap->quota());
    }

    bool contains(const String& key)
    {
        return m_storageMap->contains(key);
    }

    StorageType storageType() const
    {
        return StorageType::EphemeralLocal;
    }

    size_t memoryBytesUsedByCache()
    {
        return 0;
    }

    void incrementAccessCount() { }
    void decrementAccessCount() { }
    void closeDatabaseIfIdle() { }

    const SecurityOriginData& securityOrigin() const
    {
        return m_securityOriginData;
    }

    SecurityOriginData m_securityOriginData;
    RefPtr<StorageMap> m_storageMap;
};

Ref<StorageArea> StorageNamespaceImpl::ephemeralLocalStorageArea(const SecurityOriginData& securityOrigin)
{
    auto& slot = m_ephemeralLocalStorageAreas.add(securityOrigin, nullptr).iterator->value;
    if (!slot)
        slot = StorageNamespaceImpl::EphemeralStorageArea::create(securityOrigin, m_quotaInBytes);
    ASSERT(slot);
    return *slot;
}

Ref<StorageNamespace> StorageNamespaceImpl::copy(Page* newPage)
{
    ASSERT(m_storageNamespaceID);

    if (m_storageType == StorageType::Session)
        return createSessionStorageNamespace(WebPage::fromCorePage(newPage)->pageID(), m_quotaInBytes);

    ASSERT(m_storageType == StorageType::EphemeralLocal);
    auto newNamespace = adoptRef(*new StorageNamespaceImpl(m_storageType, m_storageNamespaceID, m_topLevelOrigin.get(), m_quotaInBytes));

    for (auto& iter : m_ephemeralLocalStorageAreas)
        newNamespace->m_ephemeralLocalStorageAreas.set(iter.key, iter.value->copy());

    return WTFMove(newNamespace);
}

} // namespace WebKit
