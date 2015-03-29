/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheStorage_h
#define NetworkCacheStorage_h

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheData.h"
#include "NetworkCacheKey.h"
#include <wtf/BloomFilter.h>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace NetworkCache {

class IOChannel;

class Storage {
    WTF_MAKE_NONCOPYABLE(Storage);
public:
    static std::unique_ptr<Storage> open(const String& cachePath);

    struct Record {
        Key key;
        std::chrono::milliseconds timeStamp;
        Data header;
        Data body;
    };
    // This may call completion handler synchronously on failure.
    typedef std::function<bool (std::unique_ptr<Record>)> RetrieveCompletionHandler;
    void retrieve(const Key&, unsigned priority, RetrieveCompletionHandler&&);

    typedef std::function<void (bool success, const Data& mappedBody)> StoreCompletionHandler;
    void store(const Record&, StoreCompletionHandler&&);
    void update(const Record& updateRecord, const Record& existingRecord, StoreCompletionHandler&&);

    void remove(const Key&);

    // Null entry signals end.
    void traverse(std::function<void (const Record*)>&&);

    void setMaximumSize(size_t);
    void clear();

    static const unsigned version = 2;

    const String& baseDirectoryPath() const { return m_baseDirectoryPath; }
    const String& directoryPath() const { return m_directoryPath; }

private:
    Storage(const String& directoryPath);

    void initialize();
    void deleteOldVersions();
    void shrinkIfNeeded();

    struct ReadOperation {
        Key key;
        RetrieveCompletionHandler completionHandler;
    };
    void dispatchReadOperation(const ReadOperation&);
    void dispatchPendingReadOperations();

    struct WriteOperation {
        Record record;
        Optional<Record> existingRecord;
        StoreCompletionHandler completionHandler;
    };
    void dispatchFullWriteOperation(const WriteOperation&);
    void dispatchHeaderWriteOperation(const WriteOperation&);
    void dispatchPendingWriteOperations();

    void updateFileModificationTime(IOChannel&);

    WorkQueue& ioQueue() { return m_ioQueue.get(); }
    WorkQueue& backgroundIOQueue() { return m_backgroundIOQueue.get(); }
    WorkQueue& serialBackgroundIOQueue() { return m_serialBackgroundIOQueue.get(); }

    bool cacheMayContain(unsigned shortHash) { return !m_hasPopulatedContentsFilter || m_contentsFilter.mayContain(shortHash); }

    const String m_baseDirectoryPath;
    const String m_directoryPath;

    size_t m_maximumSize { std::numeric_limits<size_t>::max() };

    BloomFilter<20> m_contentsFilter;
    std::atomic<bool> m_hasPopulatedContentsFilter { false };

    std::atomic<size_t> m_approximateSize { 0 };
    std::atomic<bool> m_shrinkInProgress { false };

    static const int maximumRetrievePriority = 4;
    Deque<std::unique_ptr<const ReadOperation>> m_pendingReadOperationsByPriority[maximumRetrievePriority + 1];
    HashSet<std::unique_ptr<const ReadOperation>> m_activeReadOperations;

    Deque<std::unique_ptr<const WriteOperation>> m_pendingWriteOperations;
    HashSet<std::unique_ptr<const WriteOperation>> m_activeWriteOperations;

    Ref<WorkQueue> m_ioQueue;
    Ref<WorkQueue> m_backgroundIOQueue;
    Ref<WorkQueue> m_serialBackgroundIOQueue;
};

}
}
#endif
#endif
