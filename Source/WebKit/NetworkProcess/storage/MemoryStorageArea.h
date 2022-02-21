/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "StorageAreaBase.h"
#include <WebCore/StorageMap.h>

namespace WebCore {
class StorageMap;
}

namespace WebKit {

class MemoryStorageArea final : public StorageAreaBase {
public:
    explicit MemoryStorageArea(const WebCore::ClientOrigin&, StorageAreaBase::StorageType = StorageAreaBase::StorageType::Session);

    StorageAreaBase::Type type() const final { return StorageAreaBase::Type::Memory; };
    bool isEmpty() final;
    void clear() final;
    std::unique_ptr<MemoryStorageArea> clone() const;
    

private:
    // StorageAreaBase
    StorageAreaBase::StorageType storageType() const final { return m_storageType; }
    HashMap<String, String> allItems() final;
    Expected<void, StorageError> setItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, String&& key, String&& value, const String& urlString) final;
    Expected<void, StorageError> removeItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& key, const String& urlString) final;
    Expected<void, StorageError> clear(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& urlString) final;

    mutable WebCore::StorageMap m_map;
    StorageAreaBase::StorageType m_storageType;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::MemoryStorageArea)
    static bool isType(const WebKit::StorageAreaBase& area) { return area.type() == WebKit::StorageAreaBase::Type::Memory; }
SPECIALIZE_TYPE_TRAITS_END()
