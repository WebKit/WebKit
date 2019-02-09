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

#include "config.h"
#include "NetworkCacheStorage.h"

#include "AuxiliaryProcess.h"
#include "Logging.h"
#include "NetworkCacheCoders.h"
#include "NetworkCacheFileSystem.h"
#include "NetworkCacheIOChannel.h"
#include <mutex>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/RandomNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {
namespace NetworkCache {

static const char saltFileName[] = "salt";
static const char versionDirectoryPrefix[] = "Version ";
static const char recordsDirectoryName[] = "Records";
static const char blobsDirectoryName[] = "Blobs";
static const char blobSuffix[] = "-blob";
constexpr size_t maximumInlineBodySize { 16 * 1024 };

static double computeRecordWorth(FileTimes);

struct Storage::ReadOperation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ReadOperation(Storage& storage, const Key& key, RetrieveCompletionHandler&& completionHandler)
        : storage(storage)
        , key(key)
        , completionHandler(WTFMove(completionHandler))
    { }

    void cancel();
    bool finish();

    Ref<Storage> storage;

    const Key key;
    RetrieveCompletionHandler completionHandler;
    
    std::unique_ptr<Record> resultRecord;
    SHA1::Digest expectedBodyHash;
    BlobStorage::Blob resultBodyBlob;
    std::atomic<unsigned> activeCount { 0 };
    bool isCanceled { false };
    Timings timings;
};

void Storage::ReadOperation::cancel()
{
    ASSERT(RunLoop::isMain());

    if (isCanceled)
        return;
    timings.completionTime = MonotonicTime::now();
    timings.wasCanceled = true;
    isCanceled = true;
    completionHandler(nullptr, timings);
}

bool Storage::ReadOperation::finish()
{
    ASSERT(RunLoop::isMain());

    if (isCanceled)
        return false;
    if (resultRecord && resultRecord->body.isNull()) {
        if (resultBodyBlob.hash == expectedBodyHash)
            resultRecord->body = resultBodyBlob.data;
        else
            resultRecord = nullptr;
    }
    timings.completionTime = MonotonicTime::now();
    return completionHandler(WTFMove(resultRecord), timings);
}

struct Storage::WriteOperation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WriteOperation(Storage& storage, const Record& record, MappedBodyHandler&& mappedBodyHandler, CompletionHandler<void(int)>&& completionHandler)
        : storage(storage)
        , record(record)
        , mappedBodyHandler(WTFMove(mappedBodyHandler))
        , completionHandler(WTFMove(completionHandler))
    { }

    Ref<Storage> storage;

    const Record record;
    const MappedBodyHandler mappedBodyHandler;
    CompletionHandler<void(int)> completionHandler;

    std::atomic<unsigned> activeCount { 0 };
};

struct Storage::TraverseOperation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TraverseOperation(Ref<Storage>&& storage, const String& type, OptionSet<TraverseFlag> flags, TraverseHandler&& handler)
        : storage(WTFMove(storage))
        , type(type)
        , flags(flags)
        , handler(WTFMove(handler))
    { }
    Ref<Storage> storage;

    const String type;
    const OptionSet<TraverseFlag> flags;
    const TraverseHandler handler;

    Lock activeMutex;
    Condition activeCondition;
    unsigned activeCount { 0 };
};

static String makeVersionedDirectoryPath(const String& baseDirectoryPath)
{
    String versionSubdirectory = makeString(versionDirectoryPrefix, Storage::version);
    return FileSystem::pathByAppendingComponent(baseDirectoryPath, versionSubdirectory);
}

static String makeRecordsDirectoryPath(const String& baseDirectoryPath)
{
    return FileSystem::pathByAppendingComponent(makeVersionedDirectoryPath(baseDirectoryPath), recordsDirectoryName);
}

static String makeBlobDirectoryPath(const String& baseDirectoryPath)
{
    return FileSystem::pathByAppendingComponent(makeVersionedDirectoryPath(baseDirectoryPath), blobsDirectoryName);
}

static String makeSaltFilePath(const String& baseDirectoryPath)
{
    return FileSystem::pathByAppendingComponent(makeVersionedDirectoryPath(baseDirectoryPath), saltFileName);
}

RefPtr<Storage> Storage::open(const String& cachePath, Mode mode)
{
    ASSERT(RunLoop::isMain());

    if (!FileSystem::makeAllDirectories(makeVersionedDirectoryPath(cachePath)))
        return nullptr;
    auto salt = readOrMakeSalt(makeSaltFilePath(cachePath));
    if (!salt)
        return nullptr;
    return adoptRef(new Storage(cachePath, mode, *salt));
}

void traverseRecordsFiles(const String& recordsPath, const String& expectedType, const RecordFileTraverseFunction& function)
{
    traverseDirectory(recordsPath, [&](const String& partitionName, DirectoryEntryType entryType) {
        if (entryType != DirectoryEntryType::Directory)
            return;
        String partitionPath = FileSystem::pathByAppendingComponent(recordsPath, partitionName);
        traverseDirectory(partitionPath, [&](const String& actualType, DirectoryEntryType entryType) {
            if (entryType != DirectoryEntryType::Directory)
                return;
            if (!expectedType.isEmpty() && expectedType != actualType)
                return;
            String recordDirectoryPath = FileSystem::pathByAppendingComponent(partitionPath, actualType);
            traverseDirectory(recordDirectoryPath, [&function, &recordDirectoryPath, &actualType](const String& fileName, DirectoryEntryType entryType) {
                if (entryType != DirectoryEntryType::File || fileName.length() < Key::hashStringLength())
                    return;

                String hashString = fileName.substring(0, Key::hashStringLength());
                auto isBlob = fileName.length() > Key::hashStringLength() && fileName.endsWith(blobSuffix);
                function(fileName, hashString, actualType, isBlob, recordDirectoryPath);
            });
        });
    });
}

static void deleteEmptyRecordsDirectories(const String& recordsPath)
{
    traverseDirectory(recordsPath, [&recordsPath](const String& partitionName, DirectoryEntryType type) {
        if (type != DirectoryEntryType::Directory)
            return;

        // Delete [type] sub-folders.
        String partitionPath = FileSystem::pathByAppendingComponent(recordsPath, partitionName);
        traverseDirectory(partitionPath, [&partitionPath](const String& subdirName, DirectoryEntryType entryType) {
            if (entryType != DirectoryEntryType::Directory)
                return;

            // Let system figure out if it is really empty.
            FileSystem::deleteEmptyDirectory(FileSystem::pathByAppendingComponent(partitionPath, subdirName));
        });

        // Delete [Partition] folders.
        // Let system figure out if it is really empty.
        FileSystem::deleteEmptyDirectory(FileSystem::pathByAppendingComponent(recordsPath, partitionName));
    });
}

Storage::Storage(const String& baseDirectoryPath, Mode mode, Salt salt)
    : m_basePath(baseDirectoryPath)
    , m_recordsPath(makeRecordsDirectoryPath(baseDirectoryPath))
    , m_mode(mode)
    , m_salt(salt)
    , m_canUseBlobsForForBodyData(isSafeToUseMemoryMapForPath(baseDirectoryPath))
    , m_readOperationTimeoutTimer(*this, &Storage::cancelAllReadOperations)
    , m_writeOperationDispatchTimer(*this, &Storage::dispatchPendingWriteOperations)
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage", WorkQueue::Type::Concurrent))
    , m_backgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.background", WorkQueue::Type::Concurrent, WorkQueue::QOS::Background))
    , m_serialBackgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.serialBackground", WorkQueue::Type::Serial, WorkQueue::QOS::Background))
    , m_blobStorage(makeBlobDirectoryPath(baseDirectoryPath), m_salt)
{
    ASSERT(RunLoop::isMain());

    deleteOldVersions();
    synchronize();
}

Storage::~Storage()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeReadOperations.isEmpty());
    ASSERT(m_activeWriteOperations.isEmpty());
    ASSERT(m_activeTraverseOperations.isEmpty());
    ASSERT(!m_synchronizationInProgress);
    ASSERT(!m_shrinkInProgress);
}

String Storage::basePath() const
{
    return m_basePath.isolatedCopy();
}

String Storage::versionPath() const
{
    return makeVersionedDirectoryPath(basePath());
}

String Storage::recordsPath() const
{
    return m_recordsPath.isolatedCopy();
}

size_t Storage::approximateSize() const
{
    return m_approximateRecordsSize + m_blobStorage.approximateSize();
}

static size_t estimateRecordsSize(unsigned recordCount, unsigned blobCount)
{
    auto inlineBodyCount = recordCount - std::min(blobCount, recordCount);
    auto headerSizes = recordCount * 4096;
    auto inlineBodySizes = (maximumInlineBodySize / 2) * inlineBodyCount;
    return headerSizes + inlineBodySizes;
}

void Storage::synchronize()
{
    ASSERT(RunLoop::isMain());

    if (m_synchronizationInProgress || m_shrinkInProgress)
        return;
    m_synchronizationInProgress = true;

    LOG(NetworkCacheStorage, "(NetworkProcess) synchronizing cache");

    backgroundIOQueue().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        auto recordFilter = std::make_unique<ContentsFilter>();
        auto blobFilter = std::make_unique<ContentsFilter>();

        // Most of the disk space usage is in blobs if we are using them. Approximate records file sizes to avoid expensive stat() calls.
        bool shouldComputeExactRecordsSize = !m_canUseBlobsForForBodyData;
        size_t recordsSize = 0;
        unsigned recordCount = 0;
        unsigned blobCount = 0;

        String anyType;
        traverseRecordsFiles(recordsPath(), anyType, [&](const String& fileName, const String& hashString, const String& type, bool isBlob, const String& recordDirectoryPath) {
            auto filePath = FileSystem::pathByAppendingComponent(recordDirectoryPath, fileName);

            Key::HashType hash;
            if (!Key::stringToHash(hashString, hash)) {
                FileSystem::deleteFile(filePath);
                return;
            }

            if (isBlob) {
                ++blobCount;
                blobFilter->add(hash);
                return;
            }

            ++recordCount;

            if (shouldComputeExactRecordsSize) {
                long long fileSize = 0;
                FileSystem::getFileSize(filePath, fileSize);
                recordsSize += fileSize;
            }

            recordFilter->add(hash);
        });

        if (!shouldComputeExactRecordsSize)
            recordsSize = estimateRecordsSize(recordCount, blobCount);

        m_blobStorage.synchronize();

        deleteEmptyRecordsDirectories(recordsPath());

        LOG(NetworkCacheStorage, "(NetworkProcess) cache synchronization completed size=%zu recordCount=%u", recordsSize, recordCount);

        RunLoop::main().dispatch([this, protectedThis = WTFMove(protectedThis), recordFilter = WTFMove(recordFilter), blobFilter = WTFMove(blobFilter), recordsSize]() mutable {
            for (auto& recordFilterKey : m_recordFilterHashesAddedDuringSynchronization)
                recordFilter->add(recordFilterKey);
            m_recordFilterHashesAddedDuringSynchronization.clear();

            for (auto& hash : m_blobFilterHashesAddedDuringSynchronization)
                blobFilter->add(hash);
            m_blobFilterHashesAddedDuringSynchronization.clear();

            m_recordFilter = WTFMove(recordFilter);
            m_blobFilter = WTFMove(blobFilter);
            m_approximateRecordsSize = recordsSize;
            m_synchronizationInProgress = false;
        });

    });
}

void Storage::addToRecordFilter(const Key& key)
{
    ASSERT(RunLoop::isMain());

    if (m_recordFilter)
        m_recordFilter->add(key.hash());

    // If we get new entries during filter synchronization take care to add them to the new filter as well.
    if (m_synchronizationInProgress)
        m_recordFilterHashesAddedDuringSynchronization.append(key.hash());
}

bool Storage::mayContain(const Key& key) const
{
    ASSERT(RunLoop::isMain());
    return !m_recordFilter || m_recordFilter->mayContain(key.hash());
}

bool Storage::mayContainBlob(const Key& key) const
{
    ASSERT(RunLoop::isMain());
    if (!m_canUseBlobsForForBodyData)
        return false;
    return !m_blobFilter || m_blobFilter->mayContain(key.hash());
}

String Storage::recordDirectoryPathForKey(const Key& key) const
{
    ASSERT(!key.type().isEmpty());
    return FileSystem::pathByAppendingComponent(FileSystem::pathByAppendingComponent(recordsPath(), key.partitionHashAsString()), key.type());
}

String Storage::recordPathForKey(const Key& key) const
{
    return FileSystem::pathByAppendingComponent(recordDirectoryPathForKey(key), key.hashAsString());
}

static String blobPathForRecordPath(const String& recordPath)
{
    return recordPath + blobSuffix;
}

String Storage::blobPathForKey(const Key& key) const
{
    return blobPathForRecordPath(recordPathForKey(key));
}

struct RecordMetaData {
    RecordMetaData() { }
    explicit RecordMetaData(const Key& key)
        : cacheStorageVersion(Storage::version)
        , key(key)
    { }

    unsigned cacheStorageVersion;
    Key key;
    WallTime timeStamp;
    SHA1::Digest headerHash;
    uint64_t headerSize { 0 };
    SHA1::Digest bodyHash;
    uint64_t bodySize { 0 };
    bool isBodyInline { false };

    // Not encoded as a field. Header starts immediately after meta data.
    uint64_t headerOffset { 0 };
};

static bool decodeRecordMetaData(RecordMetaData& metaData, const Data& fileData)
{
    bool success = false;
    fileData.apply([&metaData, &success](const uint8_t* data, size_t size) {
        WTF::Persistence::Decoder decoder(data, size);
        if (!decoder.decode(metaData.cacheStorageVersion))
            return false;
        if (!decoder.decode(metaData.key))
            return false;
        if (!decoder.decode(metaData.timeStamp))
            return false;
        if (!decoder.decode(metaData.headerHash))
            return false;
        if (!decoder.decode(metaData.headerSize))
            return false;
        if (!decoder.decode(metaData.bodyHash))
            return false;
        if (!decoder.decode(metaData.bodySize))
            return false;
        if (!decoder.decode(metaData.isBodyInline))
            return false;
        if (!decoder.verifyChecksum())
            return false;
        metaData.headerOffset = decoder.currentOffset();
        success = true;
        return false;
    });
    return success;
}

static bool decodeRecordHeader(const Data& fileData, RecordMetaData& metaData, Data& headerData, const Salt& salt)
{
    if (!decodeRecordMetaData(metaData, fileData)) {
        LOG(NetworkCacheStorage, "(NetworkProcess) meta data decode failure");
        return false;
    }

    if (metaData.cacheStorageVersion != Storage::version) {
        LOG(NetworkCacheStorage, "(NetworkProcess) version mismatch");
        return false;
    }

    headerData = fileData.subrange(metaData.headerOffset, metaData.headerSize);
    if (metaData.headerHash != computeSHA1(headerData, salt)) {
        LOG(NetworkCacheStorage, "(NetworkProcess) header checksum mismatch");
        return false;
    }
    return true;
}

void Storage::readRecord(ReadOperation& readOperation, const Data& recordData)
{
    ASSERT(!RunLoop::isMain());

    RecordMetaData metaData;
    Data headerData;
    if (!decodeRecordHeader(recordData, metaData, headerData, m_salt))
        return;

    if (metaData.key != readOperation.key)
        return;

    // Sanity check against time stamps in future.
    if (metaData.timeStamp > WallTime::now())
        return;

    Data bodyData;
    if (metaData.isBodyInline) {
        size_t bodyOffset = metaData.headerOffset + headerData.size();
        if (bodyOffset + metaData.bodySize != recordData.size())
            return;
        bodyData = recordData.subrange(bodyOffset, metaData.bodySize);
        if (metaData.bodyHash != computeSHA1(bodyData, m_salt))
            return;
    }

    readOperation.expectedBodyHash = metaData.bodyHash;
    readOperation.resultRecord = std::make_unique<Storage::Record>(Storage::Record {
        metaData.key,
        metaData.timeStamp,
        headerData,
        bodyData,
        metaData.bodyHash
    });
}

static Data encodeRecordMetaData(const RecordMetaData& metaData)
{
    WTF::Persistence::Encoder encoder;

    encoder << metaData.cacheStorageVersion;
    encoder << metaData.key;
    encoder << metaData.timeStamp;
    encoder << metaData.headerHash;
    encoder << metaData.headerSize;
    encoder << metaData.bodyHash;
    encoder << metaData.bodySize;
    encoder << metaData.isBodyInline;

    encoder.encodeChecksum();

    return Data(encoder.buffer(), encoder.bufferSize());
}

Optional<BlobStorage::Blob> Storage::storeBodyAsBlob(WriteOperation& writeOperation)
{
    auto blobPath = blobPathForKey(writeOperation.record.key);

    // Store the body.
    auto blob = m_blobStorage.add(blobPath, writeOperation.record.body);
    if (blob.data.isNull())
        return { };

    ++writeOperation.activeCount;

    RunLoop::main().dispatch([this, blob, &writeOperation] {
        if (m_blobFilter)
            m_blobFilter->add(writeOperation.record.key.hash());
        if (m_synchronizationInProgress)
            m_blobFilterHashesAddedDuringSynchronization.append(writeOperation.record.key.hash());

        if (writeOperation.mappedBodyHandler)
            writeOperation.mappedBodyHandler(blob.data);

        finishWriteOperation(writeOperation);
    });
    return blob;
}

Data Storage::encodeRecord(const Record& record, Optional<BlobStorage::Blob> blob)
{
    ASSERT(!blob || bytesEqual(blob.value().data, record.body));

    RecordMetaData metaData(record.key);
    metaData.timeStamp = record.timeStamp;
    metaData.headerHash = computeSHA1(record.header, m_salt);
    metaData.headerSize = record.header.size();
    metaData.bodyHash = blob ? blob.value().hash : computeSHA1(record.body, m_salt);
    metaData.bodySize = record.body.size();
    metaData.isBodyInline = !blob;

    auto encodedMetaData = encodeRecordMetaData(metaData);
    auto headerData = concatenate(encodedMetaData, record.header);

    if (metaData.isBodyInline)
        return concatenate(headerData, record.body);

    return { headerData };
}

void Storage::removeFromPendingWriteOperations(const Key& key)
{
    while (true) {
        auto found = m_pendingWriteOperations.findIf([&key](auto& operation) {
            return operation->record.key == key;
        });

        if (found == m_pendingWriteOperations.end())
            break;

        m_pendingWriteOperations.remove(found);
    }
}

void Storage::remove(const Key& key)
{
    ASSERT(RunLoop::isMain());

    if (!mayContain(key))
        return;

    auto protectedThis = makeRef(*this);

    // We can't remove the key from the Bloom filter (but some false positives are expected anyway).
    // For simplicity we also don't reduce m_approximateSize on removals.
    // The next synchronization will update everything.

    removeFromPendingWriteOperations(key);

    serialBackgroundIOQueue().dispatch([this, protectedThis = WTFMove(protectedThis), key] () mutable {
        deleteFiles(key);
    });
}

void Storage::remove(const Vector<Key>& keys, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    Vector<Key> keysToRemove;
    keysToRemove.reserveInitialCapacity(keys.size());

    for (auto& key : keys) {
        if (!mayContain(key))
            continue;
        removeFromPendingWriteOperations(key);
        keysToRemove.uncheckedAppend(key);
    }

    serialBackgroundIOQueue().dispatch([this, protectedThis = makeRef(*this), keysToRemove = WTFMove(keysToRemove), completionHandler = WTFMove(completionHandler)] () mutable {
        for (auto& key : keysToRemove)
            deleteFiles(key);

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void Storage::deleteFiles(const Key& key)
{
    ASSERT(!RunLoop::isMain());

    FileSystem::deleteFile(recordPathForKey(key));
    m_blobStorage.remove(blobPathForKey(key));
}

void Storage::updateFileModificationTime(const String& path)
{
    serialBackgroundIOQueue().dispatch([path = path.isolatedCopy()] {
        updateFileModificationTimeIfNeeded(path);
    });
}

void Storage::dispatchReadOperation(std::unique_ptr<ReadOperation> readOperationPtr)
{
    ASSERT(RunLoop::isMain());

    auto& readOperation = *readOperationPtr;
    m_activeReadOperations.add(WTFMove(readOperationPtr));

    readOperation.timings.dispatchTime = MonotonicTime::now();
    readOperation.timings.synchronizationInProgressAtDispatch = m_synchronizationInProgress;
    readOperation.timings.shrinkInProgressAtDispatch = m_shrinkInProgress;
    readOperation.timings.dispatchCountAtDispatch = m_readOperationDispatchCount;

    ++m_readOperationDispatchCount;

    // Avoid randomness during testing.
    if (m_mode != Mode::AvoidRandomness) {
        // I/O pressure may make disk operations slow. If they start taking very long time we rather go to network.
        const Seconds readTimeout = 1500_ms;
        m_readOperationTimeoutTimer.startOneShot(readTimeout);
    }

    bool shouldGetBodyBlob = mayContainBlob(readOperation.key);

    ioQueue().dispatch([this, &readOperation, shouldGetBodyBlob] {
        auto recordPath = recordPathForKey(readOperation.key);

        ++readOperation.activeCount;
        if (shouldGetBodyBlob)
            ++readOperation.activeCount;

        readOperation.timings.recordIOStartTime = MonotonicTime::now();

        auto channel = IOChannel::open(recordPath, IOChannel::Type::Read);
        channel->read(0, std::numeric_limits<size_t>::max(), &ioQueue(), [this, &readOperation](const Data& fileData, int error) {
            readOperation.timings.recordIOEndTime = MonotonicTime::now();
            if (!error)
                readRecord(readOperation, fileData);
            finishReadOperation(readOperation);
        });

        if (shouldGetBodyBlob) {
            // Read the blob in parallel with the record read.
            readOperation.timings.blobIOStartTime = MonotonicTime::now();

            auto blobPath = blobPathForKey(readOperation.key);
            readOperation.resultBodyBlob = m_blobStorage.get(blobPath);

            readOperation.timings.blobIOEndTime = MonotonicTime::now();

            finishReadOperation(readOperation);
        }
    });
}

void Storage::finishReadOperation(ReadOperation& readOperation)
{
    ASSERT(readOperation.activeCount);
    // Record and blob reads must finish.
    if (--readOperation.activeCount)
        return;

    RunLoop::main().dispatch([this, &readOperation] {
        bool success = readOperation.finish();
        if (success)
            updateFileModificationTime(recordPathForKey(readOperation.key));
        else if (!readOperation.isCanceled)
            remove(readOperation.key);

        auto protectedThis = makeRef(*this);

        ASSERT(m_activeReadOperations.contains(&readOperation));
        m_activeReadOperations.remove(&readOperation);

        if (m_activeReadOperations.isEmpty())
            m_readOperationTimeoutTimer.stop();
        
        dispatchPendingReadOperations();

        LOG(NetworkCacheStorage, "(NetworkProcess) read complete success=%d", success);
    });
}

void Storage::cancelAllReadOperations()
{
    ASSERT(RunLoop::isMain());

    for (auto& readOperation : m_activeReadOperations)
        readOperation->cancel();

    size_t pendingCount = 0;
    for (int priority = maximumRetrievePriority; priority >= 0; --priority) {
        auto& pendingRetrieveQueue = m_pendingReadOperationsByPriority[priority];
        pendingCount += pendingRetrieveQueue.size();
        for (auto it = pendingRetrieveQueue.rbegin(), end = pendingRetrieveQueue.rend(); it != end; ++it)
            (*it)->cancel();
        pendingRetrieveQueue.clear();
    }

    LOG(NetworkCacheStorage, "(NetworkProcess) retrieve timeout, canceled %u active and %zu pending", m_activeReadOperations.size(), pendingCount);
}

void Storage::dispatchPendingReadOperations()
{
    ASSERT(RunLoop::isMain());

    const int maximumActiveReadOperationCount = 5;

    for (int priority = maximumRetrievePriority; priority >= 0; --priority) {
        if (m_activeReadOperations.size() > maximumActiveReadOperationCount) {
            LOG(NetworkCacheStorage, "(NetworkProcess) limiting parallel retrieves");
            return;
        }
        auto& pendingRetrieveQueue = m_pendingReadOperationsByPriority[priority];
        if (pendingRetrieveQueue.isEmpty())
            continue;
        dispatchReadOperation(pendingRetrieveQueue.takeLast());
    }
}

template <class T> bool retrieveFromMemory(const T& operations, const Key& key, Storage::RetrieveCompletionHandler& completionHandler)
{
    for (auto& operation : operations) {
        if (operation->record.key == key) {
            LOG(NetworkCacheStorage, "(NetworkProcess) found write operation in progress");
            RunLoop::main().dispatch([record = operation->record, completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(std::make_unique<Storage::Record>(record), { });
            });
            return true;
        }
    }
    return false;
}

void Storage::dispatchPendingWriteOperations()
{
    ASSERT(RunLoop::isMain());

    const int maximumActiveWriteOperationCount { 1 };

    while (!m_pendingWriteOperations.isEmpty()) {
        if (m_activeWriteOperations.size() >= maximumActiveWriteOperationCount) {
            LOG(NetworkCacheStorage, "(NetworkProcess) limiting parallel writes");
            return;
        }
        dispatchWriteOperation(m_pendingWriteOperations.takeLast());
    }
}

bool Storage::shouldStoreBodyAsBlob(const Data& bodyData)
{
    if (!m_canUseBlobsForForBodyData)
        return false;
    return bodyData.size() > maximumInlineBodySize;
}

void Storage::dispatchWriteOperation(std::unique_ptr<WriteOperation> writeOperationPtr)
{
    ASSERT(RunLoop::isMain());

    auto& writeOperation = *writeOperationPtr;
    m_activeWriteOperations.add(WTFMove(writeOperationPtr));

    // This was added already when starting the store but filter might have been wiped.
    addToRecordFilter(writeOperation.record.key);

    backgroundIOQueue().dispatch([this, &writeOperation] {
        auto recordDirectorPath = recordDirectoryPathForKey(writeOperation.record.key);
        auto recordPath = recordPathForKey(writeOperation.record.key);

        FileSystem::makeAllDirectories(recordDirectorPath);

        ++writeOperation.activeCount;

        bool shouldStoreAsBlob = shouldStoreBodyAsBlob(writeOperation.record.body);
        auto blob = shouldStoreAsBlob ? storeBodyAsBlob(writeOperation) : WTF::nullopt;

        auto recordData = encodeRecord(writeOperation.record, blob);

        auto channel = IOChannel::open(recordPath, IOChannel::Type::Create);
        size_t recordSize = recordData.size();
        channel->write(0, recordData, nullptr, [this, &writeOperation, recordSize](int error) {
            // On error the entry still stays in the contents filter until next synchronization.
            m_approximateRecordsSize += recordSize;
            finishWriteOperation(writeOperation, error);

            LOG(NetworkCacheStorage, "(NetworkProcess) write complete error=%d", error);
        });
    });
}

void Storage::finishWriteOperation(WriteOperation& writeOperation, int error)
{
    ASSERT(RunLoop::isMain());
    ASSERT(writeOperation.activeCount);
    ASSERT(m_activeWriteOperations.contains(&writeOperation));

    if (--writeOperation.activeCount)
        return;

    auto protectedThis = makeRef(*this);

    if (writeOperation.completionHandler)
        writeOperation.completionHandler(error);

    m_activeWriteOperations.remove(&writeOperation);
    dispatchPendingWriteOperations();

    shrinkIfNeeded();
}

void Storage::retrieve(const Key& key, unsigned priority, RetrieveCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(priority <= maximumRetrievePriority);
    ASSERT(!key.isNull());

    if (!m_capacity) {
        completionHandler(nullptr, { });
        return;
    }

    if (!mayContain(key)) {
        completionHandler(nullptr, { });
        return;
    }

    if (retrieveFromMemory(m_pendingWriteOperations, key, completionHandler))
        return;
    if (retrieveFromMemory(m_activeWriteOperations, key, completionHandler))
        return;

    auto readOperation = std::make_unique<ReadOperation>(*this, key, WTFMove(completionHandler));

    readOperation->timings.startTime = MonotonicTime::now();
    readOperation->timings.dispatchCountAtStart = m_readOperationDispatchCount;

    m_pendingReadOperationsByPriority[priority].prepend(WTFMove(readOperation));
    dispatchPendingReadOperations();
}

void Storage::store(const Record& record, MappedBodyHandler&& mappedBodyHandler, CompletionHandler<void(int)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!record.key.isNull());

    if (!m_capacity)
        return;

    auto writeOperation = std::make_unique<WriteOperation>(*this, record, WTFMove(mappedBodyHandler), WTFMove(completionHandler));
    m_pendingWriteOperations.prepend(WTFMove(writeOperation));

    // Add key to the filter already here as we do lookups from the pending operations too.
    addToRecordFilter(record.key);

    bool isInitialWrite = m_pendingWriteOperations.size() == 1;
    if (!isInitialWrite)
        return;

    m_writeOperationDispatchTimer.startOneShot(m_initialWriteDelay);
}

void Storage::traverse(const String& type, OptionSet<TraverseFlag> flags, TraverseHandler&& traverseHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(traverseHandler);
    // Avoid non-thread safe Function copies.

    auto traverseOperationPtr = std::make_unique<TraverseOperation>(makeRef(*this), type, flags, WTFMove(traverseHandler));
    auto& traverseOperation = *traverseOperationPtr;
    m_activeTraverseOperations.add(WTFMove(traverseOperationPtr));

    ioQueue().dispatch([this, &traverseOperation] {
        traverseRecordsFiles(recordsPath(), traverseOperation.type, [this, &traverseOperation](const String& fileName, const String& hashString, const String& type, bool isBlob, const String& recordDirectoryPath) {
            ASSERT(type == traverseOperation.type || traverseOperation.type.isEmpty());
            if (isBlob)
                return;

            auto recordPath = FileSystem::pathByAppendingComponent(recordDirectoryPath, fileName);

            double worth = -1;
            if (traverseOperation.flags & TraverseFlag::ComputeWorth)
                worth = computeRecordWorth(fileTimes(recordPath));
            unsigned bodyShareCount = 0;
            if (traverseOperation.flags & TraverseFlag::ShareCount)
                bodyShareCount = m_blobStorage.shareCount(blobPathForRecordPath(recordPath));

            std::unique_lock<Lock> lock(traverseOperation.activeMutex);
            ++traverseOperation.activeCount;

            auto channel = IOChannel::open(recordPath, IOChannel::Type::Read);
            channel->read(0, std::numeric_limits<size_t>::max(), nullptr, [this, &traverseOperation, worth, bodyShareCount](Data& fileData, int) {
                RecordMetaData metaData;
                Data headerData;
                if (decodeRecordHeader(fileData, metaData, headerData, m_salt)) {
                    Record record {
                        metaData.key,
                        metaData.timeStamp,
                        headerData,
                        { },
                        metaData.bodyHash
                    };
                    RecordInfo info {
                        static_cast<size_t>(metaData.bodySize),
                        worth,
                        bodyShareCount,
                        String::fromUTF8(SHA1::hexDigest(metaData.bodyHash))
                    };
                    traverseOperation.handler(&record, info);
                }

                std::lock_guard<Lock> lock(traverseOperation.activeMutex);
                --traverseOperation.activeCount;
                traverseOperation.activeCondition.notifyOne();
            });

            static const unsigned maximumParallelReadCount = 5;
            traverseOperation.activeCondition.wait(lock, [&traverseOperation] {
                return traverseOperation.activeCount <= maximumParallelReadCount;
            });
        });
        {
            // Wait for all reads to finish.
            std::unique_lock<Lock> lock(traverseOperation.activeMutex);
            traverseOperation.activeCondition.wait(lock, [&traverseOperation] {
                return !traverseOperation.activeCount;
            });
        }
        RunLoop::main().dispatch([this, &traverseOperation] {
            traverseOperation.handler(nullptr, { });

            auto protectedThis = makeRef(*this);

            m_activeTraverseOperations.remove(&traverseOperation);
        });
    });
}

void Storage::setCapacity(size_t capacity)
{
    ASSERT(RunLoop::isMain());

#if !ASSERT_DISABLED
    const size_t assumedAverageRecordSize = 50 << 10;
    size_t maximumRecordCount = capacity / assumedAverageRecordSize;
    // ~10 bits per element are required for <1% false positive rate.
    size_t effectiveBloomFilterCapacity = ContentsFilter::tableSize / 10;
    // If this gets hit it might be time to increase the filter size.
    ASSERT(maximumRecordCount < effectiveBloomFilterCapacity);
#endif

    m_capacity = capacity;

    shrinkIfNeeded();
}

void Storage::clear(const String& type, WallTime modifiedSinceTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    LOG(NetworkCacheStorage, "(NetworkProcess) clearing cache");

    if (m_recordFilter)
        m_recordFilter->clear();
    if (m_blobFilter)
        m_blobFilter->clear();
    m_approximateRecordsSize = 0;

    ioQueue().dispatch([this, protectedThis = makeRef(*this), modifiedSinceTime, completionHandler = WTFMove(completionHandler), type = type.isolatedCopy()] () mutable {
        auto recordsPath = this->recordsPath();
        traverseRecordsFiles(recordsPath, type, [modifiedSinceTime](const String& fileName, const String& hashString, const String& type, bool isBlob, const String& recordDirectoryPath) {
            auto filePath = FileSystem::pathByAppendingComponent(recordDirectoryPath, fileName);
            if (modifiedSinceTime > -WallTime::infinity()) {
                auto times = fileTimes(filePath);
                if (times.modification < modifiedSinceTime)
                    return;
            }
            FileSystem::deleteFile(filePath);
        });

        deleteEmptyRecordsDirectories(recordsPath);

        // This cleans unreferenced blobs.
        m_blobStorage.synchronize();

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

static double computeRecordWorth(FileTimes times)
{
    auto age = WallTime::now() - times.creation;
    // File modification time is updated manually on cache read. We don't use access time since OS may update it automatically.
    auto accessAge = times.modification - times.creation;

    // For sanity.
    if (age <= 0_s || accessAge < 0_s || accessAge > age)
        return 0;

    // We like old entries that have been accessed recently.
    return accessAge / age;
}

static double deletionProbability(FileTimes times, unsigned bodyShareCount)
{
    static const double maximumProbability { 0.33 };
    static const unsigned maximumEffectiveShareCount { 5 };

    auto worth = computeRecordWorth(times);

    // Adjust a bit so the most valuable entries don't get deleted at all.
    auto effectiveWorth = std::min(1.1 * worth, 1.);

    auto probability =  (1 - effectiveWorth) * maximumProbability;

    // It is less useful to remove an entry that shares its body data.
    if (bodyShareCount)
        probability /= std::min(bodyShareCount, maximumEffectiveShareCount);

    return probability;
}

void Storage::shrinkIfNeeded()
{
    ASSERT(RunLoop::isMain());

    // Avoid randomness caused by cache shrinks.
    if (m_mode == Mode::AvoidRandomness)
        return;

    if (approximateSize() > m_capacity)
        shrink();
}

void Storage::shrink()
{
    ASSERT(RunLoop::isMain());

    if (m_shrinkInProgress || m_synchronizationInProgress)
        return;
    m_shrinkInProgress = true;

    LOG(NetworkCacheStorage, "(NetworkProcess) shrinking cache approximateSize=%zu capacity=%zu", approximateSize(), m_capacity);

    backgroundIOQueue().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        auto recordsPath = this->recordsPath();
        String anyType;
        traverseRecordsFiles(recordsPath, anyType, [this](const String& fileName, const String& hashString, const String& type, bool isBlob, const String& recordDirectoryPath) {
            if (isBlob)
                return;

            auto recordPath = FileSystem::pathByAppendingComponent(recordDirectoryPath, fileName);
            auto blobPath = blobPathForRecordPath(recordPath);

            auto times = fileTimes(recordPath);
            unsigned bodyShareCount = m_blobStorage.shareCount(blobPath);
            auto probability = deletionProbability(times, bodyShareCount);

            bool shouldDelete = randomNumber() < probability;

            LOG(NetworkCacheStorage, "Deletion probability=%f bodyLinkCount=%d shouldDelete=%d", probability, bodyShareCount, shouldDelete);

            if (shouldDelete) {
                FileSystem::deleteFile(recordPath);
                m_blobStorage.remove(blobPath);
            }
        });

        RunLoop::main().dispatch([this, protectedThis = WTFMove(protectedThis)] {
            m_shrinkInProgress = false;
            // We could synchronize during the shrink traversal. However this is fast and it is better to have just one code path.
            synchronize();
        });

        LOG(NetworkCacheStorage, "(NetworkProcess) cache shrink completed");
    });
}

void Storage::deleteOldVersions()
{
    backgroundIOQueue().dispatch([this, protectedThis = makeRef(*this)] () mutable {
        auto cachePath = basePath();
        traverseDirectory(cachePath, [&cachePath](const String& subdirName, DirectoryEntryType type) {
            if (type != DirectoryEntryType::Directory)
                return;
            if (!subdirName.startsWith(versionDirectoryPrefix))
                return;
            auto versionString = subdirName.substring(strlen(versionDirectoryPrefix));
            bool success;
            unsigned directoryVersion = versionString.toUIntStrict(&success);
            if (!success)
                return;
            if (directoryVersion >= version)
                return;
#if PLATFORM(MAC)
            if (!AuxiliaryProcess::isSystemWebKit() && directoryVersion == lastStableVersion)
                return;
#endif

            auto oldVersionPath = FileSystem::pathByAppendingComponent(cachePath, subdirName);
            LOG(NetworkCacheStorage, "(NetworkProcess) deleting old cache version, path %s", oldVersionPath.utf8().data());

            deleteDirectoryRecursively(oldVersionPath);
        });
    });
}

}
}
