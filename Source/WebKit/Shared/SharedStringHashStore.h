/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "SharedMemory.h"
#include "SharedStringHashTable.h"
#include <WebCore/SharedStringHash.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class SharedStringHashStore {
public:
    class Client {
    public:
        virtual ~Client() { }

        virtual void didInvalidateSharedMemory() = 0;
        virtual void didUpdateSharedStringHashes(const Vector<WebCore::SharedStringHash>& addedHashes, const Vector<WebCore::SharedStringHash>& removedHashes) { };
    };

    SharedStringHashStore(Client&);

    std::optional<SharedMemory::Handle> createSharedMemoryHandle();

    void scheduleAddition(WebCore::SharedStringHash);
    void scheduleRemoval(WebCore::SharedStringHash);

    bool contains(WebCore::SharedStringHash);
    void clear();

    bool isEmpty() const { return !m_keyCount; }

    void flushPendingChanges();

private:
    void resizeTable(unsigned newTableLength);
    void processPendingOperations();

    struct Operation {
        enum Type { Add, Remove };
        Type type;
        WebCore::SharedStringHash sharedStringHash;
    };

    Client& m_client;
    unsigned m_keyCount { 0 };
    unsigned m_tableLength { 0 };
    SharedStringHashTable m_table;
    Vector<Operation> m_pendingOperations;
    RunLoop::Timer<SharedStringHashStore> m_pendingOperationsTimer;
};

} // namespace WebKit
