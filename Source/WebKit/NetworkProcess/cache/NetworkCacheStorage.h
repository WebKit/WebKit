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
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/PriorityQueue.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WallTime.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace NetworkCache {

class IOChannel;

class Storage : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Storage, WTF::DestructionThread::MainRunLoop> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Mode { Normal, AvoidRandomness };
    static RefPtr<Storage> open(const String& cachePath, Mode, size_t capacity);

    enum class ReadOperationIdentifierType { };
    using ReadOperationIdentifier = ObjectIdentifier<ReadOperationIdentifierType>;
    enum class WriteOperationIdentifierType { };
    using WriteOperationIdentifier = ObjectIdentifier<WriteOperationIdentifierType>;

    struct Record {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(Record);

        Record() = default;
        Record(const Key& key, WallTime timeStamp, const Data& header, const Data& body, std::optional<SHA1::Digest> bodyHash)
            : key(key)
            , timeStamp(timeStamp)
            , header(header)
            , body(body)
            , bodyHash(bodyHash)
        {
        }
        Record isolatedCopy() const & { return { crossThreadCopy(key), timeStamp, header, body, bodyHash }; }
        Record isolatedCopy() && { return { crossThreadCopy(WTFMove(key)), timeStamp, WTFMove(header), WTFMove(body), WTFMove(bodyHash) }; }
        bool isNull() const { return key.isNull(); }

        Key key;
        WallTime timeStamp;
        Data header;
        Data body;
        std::optional<SHA1::Digest> bodyHash;
    };

    struct Timings {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(Timings);

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
    };

    // This may call completion handler synchronously on failure.
    using RetrieveCompletionHandler = CompletionHandler<bool(Record&&, const Timings&)>;
    void retrieve(const Key&, unsigned priority, RetrieveCompletionHandler&&);

    using MappedBodyHandler = Function<void (const Data& mappedBody)>;
    void store(const Record&, MappedBodyHandler&&, CompletionHandler<void(int)>&& = { });

    void remove(const Key&);
    void remove(const Vector<Key>&, CompletionHandler<void()>&&);
    void clear(String&& type, WallTime modifiedSinceTime, CompletionHandler<void()>&&);

    struct RecordInfo {
        size_t bodySize;
        double worth; // 0-1 where 1 is the most valuable.
        unsigned bodyShareCount;
        String bodyHash;
    };
    enum class TraverseFlag : uint8_t {
        ComputeWorth = 1 << 0,
        ShareCount = 1 << 1,
    };
    using TraverseHandler = Function<void (const Record*, const RecordInfo&)>;
    // Null record signals end.
    void traverse(const String& type, OptionSet<TraverseFlag>, TraverseHandler&&);
    void traverse(const String& type, const String& partition, OptionSet<TraverseFlag>, TraverseHandler&&);

    void setCapacity(size_t);
    size_t capacity() const { return m_capacity; }
    size_t approximateSize() const;

    // Incrementing this number will delete all existing cache content for everyone. Do you really need to do it?
    static const unsigned version = 17;

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

    void traverseWithinRootPath(const String& rootPath, const String& type, OptionSet<TraverseFlag>, TraverseHandler&&);

    void synchronize();
    void deleteOldVersions();
    void shrinkIfNeeded();
    void shrink();

    class ReadOperation;
    void dispatchReadOperation(std::unique_ptr<ReadOperation>);
    void dispatchPendingReadOperations();
    void finishReadOperation(Storage::ReadOperationIdentifier);
    void cancelAllReadOperations();

    class WriteOperation;
    void dispatchWriteOperation(std::unique_ptr<WriteOperation>);
    void dispatchPendingWriteOperations();
    void addWriteOperationActivity(WriteOperationIdentifier);
    bool removeWriteOperationActivity(WriteOperationIdentifier);
    void finishWriteOperationActivity(WriteOperationIdentifier, int error = 0);

    bool shouldStoreBodyAsBlob(const Data& bodyData);
    std::optional<BlobStorage::Blob> storeBodyAsBlob(WriteOperationIdentifier, const Storage::Record&);
    Data encodeRecord(const Record&, std::optional<BlobStorage::Blob>);
    Record readRecord(const Data&);
    void readRecordFromData(Storage::ReadOperationIdentifier, MonotonicTime, Data&&, int error);
    void readBlobIfNecessary(Storage::ReadOperationIdentifier, const String& blobPath);

    void updateFileModificationTime(String&& path);
    void removeFromPendingWriteOperations(const Key&);

    ConcurrentWorkQueue& ioQueue() { return m_ioQueue.get(); }
    Ref<ConcurrentWorkQueue> protectedIOQueue() { return ioQueue(); }
    ConcurrentWorkQueue& backgroundIOQueue() { return m_backgroundIOQueue.get(); }
    WorkQueue& serialBackgroundIOQueue() { return m_serialBackgroundIOQueue.get(); }

    bool mayContain(const Key&) const;
    bool mayContainBlob(const Key&) const;

    void addToRecordFilter(const Key&);
    void deleteFiles(const Key&);

    static bool isHigherPriority(const std::unique_ptr<ReadOperation>&, const std::unique_ptr<ReadOperation>&);

    size_t estimateRecordsSize(unsigned recordCount, unsigned blobCount) const;
    uint32_t volumeBlockSize() const;

    const String m_basePath;
    const String m_recordsPath;
    
    const Mode m_mode;
    const Salt m_salt;

    size_t m_capacity { std::numeric_limits<size_t>::max() };
    size_t m_approximateRecordsSize { 0 };
    mutable std::optional<uint32_t> m_volumeBlockSize;

    // 2^18 bit filter can support up to 26000 entries with false positive rate < 1%.
    using ContentsFilter = BloomFilter<18>;
    std::unique_ptr<ContentsFilter> m_recordFilter;
    std::unique_ptr<ContentsFilter> m_blobFilter;

    bool m_synchronizationInProgress { false };
    bool m_shrinkInProgress { false };
    size_t m_readOperationDispatchCount { 0 };

    Vector<Key::HashType> m_recordFilterHashesAddedDuringSynchronization;
    Vector<Key::HashType> m_blobFilterHashesAddedDuringSynchronization;

    PriorityQueue<std::unique_ptr<ReadOperation>, &isHigherPriority> m_pendingReadOperations;
    HashMap<ReadOperationIdentifier, std::unique_ptr<ReadOperation>> m_activeReadOperations;
    WebCore::Timer m_readOperationTimeoutTimer;

    Lock m_activitiesLock;
    HashCountedSet<WriteOperationIdentifier> m_writeOperationActivities WTF_GUARDED_BY_LOCK(m_activitiesLock);
    Deque<std::unique_ptr<WriteOperation>> m_pendingWriteOperations;
    HashMap<WriteOperationIdentifier, std::unique_ptr<WriteOperation>> m_activeWriteOperations;
    WebCore::Timer m_writeOperationDispatchTimer;

    Ref<ConcurrentWorkQueue> m_ioQueue;
    Ref<ConcurrentWorkQueue> m_backgroundIOQueue;
    Ref<WorkQueue> m_serialBackgroundIOQueue;

    BlobStorage m_blobStorage;

    // By default, delay the start of writes a bit to avoid affecting early page load.
    // Completing writes will dispatch more writes without delay.
    Seconds m_initialWriteDelay { 1_s };
};

}
}
