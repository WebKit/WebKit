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

#include "NetworkCacheBlobStorage.h"
#include "NetworkCacheData.h"
#include "NetworkCacheKey.h"
#include <WebCore/Timer.h>
#include <wtf/BloomFilter.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
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
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Key key;
        std::chrono::system_clock::time_point timeStamp;
        Data header;
        Data body;
    };
    // This may call completion handler synchronously on failure.
    typedef std::function<bool (std::unique_ptr<Record>)> RetrieveCompletionHandler;
    void retrieve(const Key&, unsigned priority, RetrieveCompletionHandler&&);

    typedef Function<void (const Data& mappedBody)> MappedBodyHandler;
    void store(const Record&, MappedBodyHandler&&);

    void remove(const Key&);
    void clear(const String& type, std::chrono::system_clock::time_point modifiedSinceTime, std::function<void ()>&& completionHandler);

    struct RecordInfo {
        size_t bodySize;
        double worth; // 0-1 where 1 is the most valuable.
        unsigned bodyShareCount;
        String bodyHash;
    };
    enum TraverseFlag {
        ComputeWorth = 1 << 0,
        ShareCount = 1 << 1,
    };
    typedef unsigned TraverseFlags;
    typedef Function<void (const Record*, const RecordInfo&)> TraverseHandler;
    // Null record signals end.
    void traverse(const String& type, TraverseFlags, TraverseHandler&&);

    void setCapacity(size_t);
    size_t capacity() const { return m_capacity; }
    size_t approximateSize() const;

    static const unsigned version = 10;
#if PLATFORM(MAC)
    /// Allow the last stable version of the cache to co-exist with the latest development one.
    static const unsigned lastStableVersion = 9;
#endif

    String basePath() const;
    String versionPath() const;
    String recordsPath() const;

    ~Storage();

private:
    Storage(const String& directoryPath);

    String recordDirectoryPathForKey(const Key&) const;
    String recordPathForKey(const Key&) const;
    String blobPathForKey(const Key&) const;

    void synchronize();
    void deleteOldVersions();
    void shrinkIfNeeded();
    void shrink();

    struct ReadOperation;
    void dispatchReadOperation(std::unique_ptr<ReadOperation>);
    void dispatchPendingReadOperations();
    void finishReadOperation(ReadOperation&);
    void cancelAllReadOperations();

    struct WriteOperation;
    void dispatchWriteOperation(std::unique_ptr<WriteOperation>);
    void dispatchPendingWriteOperations();
    void finishWriteOperation(WriteOperation&);

    Optional<BlobStorage::Blob> storeBodyAsBlob(WriteOperation&);
    Data encodeRecord(const Record&, Optional<BlobStorage::Blob>);
    void readRecord(ReadOperation&, const Data&);

    void updateFileModificationTime(const String& path);
    void removeFromPendingWriteOperations(const Key&);

    WorkQueue& ioQueue() { return m_ioQueue.get(); }
    WorkQueue& backgroundIOQueue() { return m_backgroundIOQueue.get(); }
    WorkQueue& serialBackgroundIOQueue() { return m_serialBackgroundIOQueue.get(); }

    bool mayContain(const Key&) const;
    bool mayContainBlob(const Key&) const;

    void addToRecordFilter(const Key&);

    const String m_basePath;
    const String m_recordsPath;

    size_t m_capacity { std::numeric_limits<size_t>::max() };
    size_t m_approximateRecordsSize { 0 };

    // 2^18 bit filter can support up to 26000 entries with false positive rate < 1%.
    using ContentsFilter = BloomFilter<18>;
    std::unique_ptr<ContentsFilter> m_recordFilter;
    std::unique_ptr<ContentsFilter> m_blobFilter;

    bool m_synchronizationInProgress { false };
    bool m_shrinkInProgress { false };

    Vector<Key::HashType> m_recordFilterHashesAddedDuringSynchronization;
    Vector<Key::HashType> m_blobFilterHashesAddedDuringSynchronization;

    static const int maximumRetrievePriority = 4;
    Deque<std::unique_ptr<ReadOperation>> m_pendingReadOperationsByPriority[maximumRetrievePriority + 1];
    HashSet<std::unique_ptr<ReadOperation>> m_activeReadOperations;
    WebCore::Timer m_readOperationTimeoutTimer;

    Deque<std::unique_ptr<WriteOperation>> m_pendingWriteOperations;
    HashSet<std::unique_ptr<WriteOperation>> m_activeWriteOperations;
    WebCore::Timer m_writeOperationDispatchTimer;

    struct TraverseOperation;
    HashSet<std::unique_ptr<TraverseOperation>> m_activeTraverseOperations;

    Ref<WorkQueue> m_ioQueue;
    Ref<WorkQueue> m_backgroundIOQueue;
    Ref<WorkQueue> m_serialBackgroundIOQueue;

    BlobStorage m_blobStorage;
};

// FIXME: Remove, used by NetworkCacheStatistics only.
void traverseRecordsFiles(const String& recordsPath, const String& type, const std::function<void (const String& fileName, const String& hashString, const String& type, bool isBodyBlob, const String& recordDirectoryPath)>&);

}
}
#endif
#endif
