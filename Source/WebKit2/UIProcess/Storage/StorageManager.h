/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef StorageManager_h
#define StorageManager_h

#include "Connection.h"
#include <wtf/PassRefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/StringHash.h>

class WorkQueue;

namespace WebKit {

struct SecurityOriginData;
class WebProcessProxy;

class StorageManager : public CoreIPC::Connection::WorkQueueMessageReceiver {
public:
    static PassRefPtr<StorageManager> create();
    ~StorageManager();

    void createSessionStorageNamespace(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection, unsigned quotaInBytes);
    void destroySessionStorageNamespace(uint64_t storageNamespaceID);
    void setAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection);
    void cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID);

    void processWillOpenConnection(WebProcessProxy*);
    void processWillCloseConnection(WebProcessProxy*);

private:
    StorageManager();

    // CoreIPC::Connection::WorkQueueMessageReceiver.
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&) OVERRIDE;
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder) OVERRIDE;

    // Message handlers.
    void createStorageArea(CoreIPC::Connection*, uint64_t storageAreaID, uint64_t storageNamespaceID, const SecurityOriginData&);
    void destroyStorageArea(CoreIPC::Connection*, uint64_t storageAreaID);
    void getValues(CoreIPC::Connection*, uint64_t storageAreaID, HashMap<String, String>& values);
    void setItem(CoreIPC::Connection*, uint64_t storageAreaID, const String& key, const String& value, const String& urlString);

    void createSessionStorageNamespaceInternal(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection, unsigned quotaInBytes);
    void destroySessionStorageNamespaceInternal(uint64_t storageNamespaceID);
    void setAllowedSessionStorageNamespaceConnectionInternal(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection);
    void cloneSessionStorageNamespaceInternal(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID);

    class StorageArea;
    StorageArea* findStorageArea(CoreIPC::Connection*, uint64_t) const;

    RefPtr<WorkQueue> m_queue;

    class SessionStorageNamespace;
    HashMap<uint64_t, RefPtr<SessionStorageNamespace> > m_sessionStorageNamespaces;

    HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> > m_storageAreas;
};

} // namespace WebKit

#endif // StorageManager_h
