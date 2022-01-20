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

#include "Connection.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageAreaMapIdentifier.h"
#include <WebCore/ClientOrigin.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct ClientOrigin;
}

namespace WebKit {

enum class StorageError : uint8_t {
    Database,
    ItemNotFound,
    QuotaExceeded,
};

class StorageAreaBase : public CanMakeWeakPtr<StorageAreaBase> {
    WTF_MAKE_NONCOPYABLE(StorageAreaBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static uint64_t nextMessageIdentifier();
    virtual ~StorageAreaBase();

    enum class Type : bool { SQLite, Memory };
    virtual Type type() const = 0;
    enum class StorageType : bool { Session, Local };
    virtual StorageType storageType() const = 0;
    virtual bool isEmpty() = 0;
    virtual void clear() = 0;

    StorageAreaIdentifier identifier() const { return m_identifier; }
    WebCore::ClientOrigin origin() const { return m_origin; }
    unsigned quota() const { return m_quota; }
    void addListener(IPC::Connection::UniqueID, StorageAreaMapIdentifier);
    void removeListener(IPC::Connection::UniqueID);
    bool hasListeners() const;
    void notifyListenersAboutClear();

    virtual HashMap<String, String> allItems() = 0;
    virtual Expected<void, StorageError> setItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, String&& key, String&& value, const String& urlString) = 0;
    virtual Expected<void, StorageError> removeItem(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& key, const String& urlString) = 0;
    virtual Expected<void, StorageError> clear(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& urlString) = 0;

protected:
    StorageAreaBase(unsigned quota, const WebCore::ClientOrigin&);
    void dispatchEvents(IPC::Connection::UniqueID, StorageAreaImplIdentifier, const String& key, const String& oldValue, const String& newValue, const String& urlString) const;

private:
    StorageAreaIdentifier m_identifier;
    unsigned m_quota;
    WebCore::ClientOrigin m_origin;
    HashMap<IPC::Connection::UniqueID, StorageAreaMapIdentifier> m_listeners;
};

} // namespace WebKit
