/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MemoryObjectStoreCursor_h
#define MemoryObjectStoreCursor_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorInfo.h"
#include "IDBKeyData.h"
#include "MemoryCursor.h"
#include <set>
#include <wtf/Optional.h>

namespace WebCore {
namespace IDBServer {

class MemoryObjectStore;

class MemoryObjectStoreCursor : public MemoryCursor {
    friend std::unique_ptr<MemoryObjectStoreCursor> std::make_unique<MemoryObjectStoreCursor>(WebCore::IDBServer::MemoryObjectStore&, const WebCore::IDBCursorInfo&);
public:
    static std::unique_ptr<MemoryObjectStoreCursor> create(MemoryObjectStore&, const IDBCursorInfo&);

    void objectStoreCleared();
    void keyDeleted(const IDBKeyData&);
    void keyAdded(std::set<IDBKeyData>::iterator);

private:
    MemoryObjectStoreCursor(MemoryObjectStore&, const IDBCursorInfo&);

    virtual void currentData(IDBGetResult&) override final;
    virtual void iterate(const IDBKeyData&, uint32_t count, IDBGetResult&) override final;

    void setFirstInRemainingRange(std::set<IDBKeyData>&);
    std::set<IDBKeyData>::iterator firstForwardIteratorInRemainingRange(std::set<IDBKeyData>&);
    std::set<IDBKeyData>::reverse_iterator firstReverseIteratorInRemainingRange(std::set<IDBKeyData>&);

    void incrementForwardIterator(std::set<IDBKeyData>&, const IDBKeyData&, uint32_t count);
    void incrementReverseIterator(std::set<IDBKeyData>&, const IDBKeyData&, uint32_t count);

    bool hasIterators() const;
    bool hasValidPosition() const;
    void clearIterators();

    MemoryObjectStore& m_objectStore;

    IDBKeyRangeData m_remainingRange;

    WTF::Optional<std::set<IDBKeyData>::iterator> m_forwardIterator;
    WTF::Optional<std::set<IDBKeyData>::reverse_iterator> m_reverseIterator;

    IDBKeyData m_currentPositionKey;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // MemoryObjectStoreCursor_h
