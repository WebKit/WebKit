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

#include "config.h"
#include "StorageAreaBase.h"

#include "StorageAreaMapMessages.h"

namespace WebKit {

uint64_t StorageAreaBase::nextMessageIdentifier()
{
    static std::atomic<uint64_t> currentIdentifier;
    return ++currentIdentifier;
}

StorageAreaBase::StorageAreaBase(unsigned quota, const WebCore::ClientOrigin& origin)
    : m_identifier(StorageAreaIdentifier::generateThreadSafe())
    , m_quota(quota)
    , m_origin(origin)
{
}

StorageAreaBase::~StorageAreaBase() = default;

void StorageAreaBase::addListener(IPC::Connection::UniqueID connection, StorageAreaMapIdentifier identifier)
{
    ASSERT(!m_listeners.contains(connection) || m_listeners.get(connection) == identifier);

    m_listeners.add(connection, identifier);
}

void StorageAreaBase::removeListener(IPC::Connection::UniqueID connection)
{
    m_listeners.remove(connection);
}

bool StorageAreaBase::hasListeners() const
{
    return !m_listeners.isEmpty();
}

void StorageAreaBase::notifyListenersAboutClear()
{
    for (auto& [connection, identifier] : m_listeners)
        IPC::Connection::send(connection, Messages::StorageAreaMap::ClearCache(StorageAreaBase::nextMessageIdentifier()), identifier.toUInt64());
}

void StorageAreaBase::dispatchEvents(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier sourceImplIdentifier, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    ASSERT(sourceImplIdentifier);

    for (auto& [connection, identifier] : m_listeners) {
        std::optional<StorageAreaImplIdentifier> implIdentifier;
        if (connection == sourceConnection)
            implIdentifier = sourceImplIdentifier;
        IPC::Connection::send(connection, Messages::StorageAreaMap::DispatchStorageEvent(implIdentifier, key, oldValue, newValue, urlString, StorageAreaBase::nextMessageIdentifier()), identifier.toUInt64());
    }
}

} // namespace WebKit
