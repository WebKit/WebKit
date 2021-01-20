/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "NetworkCacheBlobStorage.h"
#include "NetworkCacheData.h"
#include "NetworkCacheKey.h"
#include <WebCore/Timer.h>
#include <wtf/BloomFilter.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>
#include <wtf/WallTime.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace NetworkCache {

class IOChannel;

class Storage : public ThreadSafeRefCounted<Storage, WTF::DestructionThread::Main> {
public:
    enum class Mode { Normal, AvoidRandomness };
    static RefPtr<Storage> open(const String& cachePath, Mode, size_t capacity);

    struct Record {
        Key key;
        WallTime timeStamp;
        Data header;
        Data body;
        Optional<SHA1::Digest> bodyHash;

        WTF_MAKE_FAST_ALLOCATED;
    };

    struct Timings {
        MonotonicTime startTime;
        MonotonicTime dispatchTime;
        MonotonicTime recordIOStartTime;
        MonotonicTime recordIOEndTime;
        MonotonicTime blobIOStartTime;
        MonotonicTime blobIOEndTime;
        MonotonicTime completionTime;
        size_t dispatchCountAtStart { 0 };
        size_t dispatchCountAtDispatch { 0 };
        bool synchronizationInProgressAtDispatch { false };
        bool shrinkInProgressAtDispatch { false };
        bool wasCanceled { false };

        WTF_MAKE_FAST_ALLOCATED;
    };

    // This may call completion handler synchronously on failure.
    using RetrieveCompletionHandler = CompletionHandler<bool(std::unique_ptr<Record>, const Timings&)>;
    void retrieve(const Key&, unsigned priority, RetrieveCompletionHandler&&);

    using MappedBodyHandler = Function<void (const Data& mappedBody)>;
    void store(const Record&, MappedBodyHandler&&, CompletionHandler<void(int)>&& = { });

    void remove(const Key&);
    void remove(const Vector<Key>&, CompletionHandler<void()>&&);
    void clear(const String& type, WallTime modifiedSinceTime, CompletionHandler<void()>&&);

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
    using TraverseHandler = Function<void (const Record*, const RecordInfo&)>;
    // Null record signals end.
    void traverse(const String& type, OptionSet<TraverseFlag>, TraverseHandler&&);

    void setCapacity(size_t);
    size_t capacity() const { return m_capacity; }
    size_t approximateSize() const;

    // Incrementing this number will delete all existing cache content for everyone. Do you really need to do it?
    static const unsigned version = 16;

    String basePathIsolatedCopy() const;
    String versionPath() const;
    String recordsPathIsolatedCopy() const;

    const Salt& salt() const { return m_salt; }

    ~Storage();

    void writeWithoutWaiting() { m_initialWriteDelay = 0_s; };

private:
    Storage(const String& directoryPath, Mode, Salt, size_t capacity);

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
    void finishWriteOperation(WriteOperation&, int error = 0);

    bool shouldStoreBodyAsBlob(const Data& bodyData);
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
    void deleteFiles(const Key&);

    const String m_basePath;
    const String m_recordsPath;
    
    const Mode m_mode;
    const Salt m_salt;

    size_t m_capacity { std::numeric_limits<size_t>::max() };
    size_t m_approximateRecordsSize { 0 };

    // 2^18 bit filter can support up to 26000 entries with false positive rate < 1%.
    using ContentsFilter = BloomFilter<18>;
    std::unique_ptr<ContentsFilter> m_recordFilter;
    std::unique_ptr<ContentsFilter> m_blobFilter;

    bool m_synchronizationInProgress { false };
    bool m_shrinkInProgress { false };
    size_t m_readOperationDispatchCount { 0 };

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

    // By default, delay the start of writes a bit to avoid affecting early page load.
    // Completing writes will dispatch more writes without delay.
    Seconds m_initialWriteDelay { 1_s };
};

}
}
