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

#include "config.h"
#include "NetworkCacheStorage.h"

#if ENABLE(NETWORK_CACHE)

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include "NetworkCacheFileSystem.h"
#include "NetworkCacheIOChannel.h"
#include <condition_variable>
#include <wtf/RandomNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
namespace NetworkCache {

static const char versionDirectoryPrefix[] = "Version ";
static const char recordsDirectoryName[] = "Records";
static const char blobsDirectoryName[] = "Blobs";
static const char bodyPostfix[] = "-body";

static double computeRecordWorth(FileTimes);

struct Storage::ReadOperation {
    ReadOperation(const Key& key, const RetrieveCompletionHandler& completionHandler)
        : key(key)
        , completionHandler(completionHandler)
    { }

    const Key key;
    const RetrieveCompletionHandler completionHandler;
    
    std::unique_ptr<Record> resultRecord;
    SHA1::Digest expectedBodyHash;
    BlobStorage::Blob resultBodyBlob;
    std::atomic<unsigned> activeCount { 0 };
};

struct Storage::WriteOperation {
    WriteOperation(const Record& record, const MappedBodyHandler& mappedBodyHandler)
        : record(record)
        , mappedBodyHandler(mappedBodyHandler)
    { }
    
    const Record record;
    const MappedBodyHandler mappedBodyHandler;

    std::atomic<unsigned> activeCount { 0 };
};

struct Storage::TraverseOperation {
    TraverseOperation(TraverseFlags flags, const TraverseHandler& handler)
        : flags(flags)
        , handler(handler)
    { }

    const TraverseFlags flags;
    const TraverseHandler handler;

    std::mutex activeMutex;
    std::condition_variable activeCondition;
    unsigned activeCount { 0 };
};

std::unique_ptr<Storage> Storage::open(const String& cachePath)
{
    ASSERT(RunLoop::isMain());

    if (!WebCore::makeAllDirectories(cachePath))
        return nullptr;
    return std::unique_ptr<Storage>(new Storage(cachePath));
}

static String makeVersionedDirectoryPath(const String& baseDirectoryPath)
{
    String versionSubdirectory = versionDirectoryPrefix + String::number(Storage::version);
    return WebCore::pathByAppendingComponent(baseDirectoryPath, versionSubdirectory);
}

static String makeRecordsDirectoryPath(const String& baseDirectoryPath)
{
    return WebCore::pathByAppendingComponent(makeVersionedDirectoryPath(baseDirectoryPath), recordsDirectoryName);
}

static String makeBlobDirectoryPath(const String& baseDirectoryPath)
{
    return WebCore::pathByAppendingComponent(makeVersionedDirectoryPath(baseDirectoryPath), blobsDirectoryName);
}

void traverseRecordsFiles(const String& recordsPath, const std::function<void (const String&, const String&)>& function)
{
    traverseDirectory(recordsPath, [&recordsPath, &function](const String& subdirName, DirectoryEntryType type) {
        if (type != DirectoryEntryType::Directory)
            return;
        String partitionPath = WebCore::pathByAppendingComponent(recordsPath, subdirName);
        traverseDirectory(partitionPath, [&function, &partitionPath](const String& fileName, DirectoryEntryType type) {
            if (type != DirectoryEntryType::File)
                return;
            function(fileName, partitionPath);
        });
    });
}

static void deleteEmptyRecordsDirectories(const String& recordsPath)
{
    traverseDirectory(recordsPath, [&recordsPath](const String& subdirName, DirectoryEntryType type) {
        if (type != DirectoryEntryType::Directory)
            return;
        // Let system figure out if it is really empty.
        WebCore::deleteEmptyDirectory(WebCore::pathByAppendingComponent(recordsPath, subdirName));
    });
}

Storage::Storage(const String& baseDirectoryPath)
    : m_basePath(baseDirectoryPath)
    , m_recordsPath(makeRecordsDirectoryPath(baseDirectoryPath))
    , m_writeOperationDispatchTimer(*this, &Storage::dispatchPendingWriteOperations)
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage", WorkQueue::Type::Concurrent))
    , m_backgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.background", WorkQueue::Type::Concurrent, WorkQueue::QOS::Background))
    , m_serialBackgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.serialBackground", WorkQueue::Type::Serial, WorkQueue::QOS::Background))
    , m_blobStorage(makeBlobDirectoryPath(baseDirectoryPath))
{
    deleteOldVersions();
    synchronize();
}

Storage::~Storage()
{
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

void Storage::synchronize()
{
    ASSERT(RunLoop::isMain());

    if (m_synchronizationInProgress || m_shrinkInProgress)
        return;
    m_synchronizationInProgress = true;

    LOG(NetworkCacheStorage, "(NetworkProcess) synchronizing cache");

    backgroundIOQueue().dispatch([this] {
        auto recordFilter = std::make_unique<ContentsFilter>();
        auto bodyFilter = std::make_unique<ContentsFilter>();
        size_t recordsSize = 0;
        unsigned count = 0;
        traverseRecordsFiles(recordsPath(), [&recordFilter, &bodyFilter, &recordsSize, &count](const String& fileName, const String& partitionPath) {
            auto filePath = WebCore::pathByAppendingComponent(partitionPath, fileName);

            bool isBody = fileName.endsWith(bodyPostfix);
            String hashString = isBody ? fileName.substring(0, Key::hashStringLength()) : fileName;
            Key::HashType hash;
            if (!Key::stringToHash(hashString, hash)) {
                WebCore::deleteFile(filePath);
                return;
            }
            long long fileSize = 0;
            WebCore::getFileSize(filePath, fileSize);
            if (!fileSize) {
                WebCore::deleteFile(filePath);
                return;
            }
            if (isBody) {
                bodyFilter->add(hash);
                return;
            }
            recordFilter->add(hash);
            recordsSize += fileSize;
            ++count;
        });

        auto* recordFilterPtr = recordFilter.release();
        auto* bodyFilterPtr = bodyFilter.release();
        RunLoop::main().dispatch([this, recordFilterPtr, bodyFilterPtr, recordsSize] {
            auto recordFilter = std::unique_ptr<ContentsFilter>(recordFilterPtr);
            auto bodyFilter = std::unique_ptr<ContentsFilter>(bodyFilterPtr);

            for (auto hash : m_recordFilterHashesAddedDuringSynchronization)
                recordFilter->add(hash);
            m_recordFilterHashesAddedDuringSynchronization.clear();

            for (auto hash : m_bodyFilterHashesAddedDuringSynchronization)
                bodyFilter->add(hash);
            m_bodyFilterHashesAddedDuringSynchronization.clear();

            m_recordFilter = WTF::move(recordFilter);
            m_bodyFilter = WTF::move(bodyFilter);
            m_approximateRecordsSize = recordsSize;
            m_synchronizationInProgress = false;
        });

        m_blobStorage.synchronize();

        deleteEmptyRecordsDirectories(recordsPath());

        LOG(NetworkCacheStorage, "(NetworkProcess) cache synchronization completed size=%zu count=%d", recordsSize, count);
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

String Storage::partitionPathForKey(const Key& key) const
{
    ASSERT(!key.partition().isEmpty());
    return WebCore::pathByAppendingComponent(recordsPath(), key.partition());
}

static String fileNameForKey(const Key& key)
{
    return key.hashAsString();
}

String Storage::recordPathForKey(const Key& key) const
{
    return WebCore::pathByAppendingComponent(partitionPathForKey(key), fileNameForKey(key));
}

static String bodyPathForRecordPath(const String& recordPath)
{
    return recordPath + bodyPostfix;
}

String Storage::bodyPathForKey(const Key& key) const
{
    return bodyPathForRecordPath(recordPathForKey(key));
}

struct RecordMetaData {
    RecordMetaData() { }
    explicit RecordMetaData(const Key& key)
        : cacheStorageVersion(Storage::version)
        , key(key)
    { }

    unsigned cacheStorageVersion;
    Key key;
    // FIXME: Add encoder/decoder for time_point.
    std::chrono::milliseconds epochRelativeTimeStamp;
    SHA1::Digest headerHash;
    uint64_t headerSize;
    SHA1::Digest bodyHash;
    uint64_t bodySize;
    bool isBodyInline;

    // Not encoded as a field. Header starts immediately after meta data.
    uint64_t headerOffset;
};

static bool decodeRecordMetaData(RecordMetaData& metaData, const Data& fileData)
{
    bool success = false;
    fileData.apply([&metaData, &success](const uint8_t* data, size_t size) {
        Decoder decoder(data, size);
        if (!decoder.decode(metaData.cacheStorageVersion))
            return false;
        if (!decoder.decode(metaData.key))
            return false;
        if (!decoder.decode(metaData.epochRelativeTimeStamp))
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

static bool decodeRecordHeader(const Data& fileData, RecordMetaData& metaData, Data& headerData)
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
    if (metaData.headerHash != computeSHA1(headerData)) {
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
    if (!decodeRecordHeader(recordData, metaData, headerData))
        return;

    if (metaData.key != readOperation.key)
        return;

    // Sanity check against time stamps in future.
    auto timeStamp = std::chrono::system_clock::time_point(metaData.epochRelativeTimeStamp);
    if (timeStamp > std::chrono::system_clock::now())
        return;

    Data bodyData;
    if (metaData.isBodyInline) {
        size_t bodyOffset = metaData.headerOffset + headerData.size();
        if (bodyOffset + metaData.bodySize != recordData.size())
            return;
        bodyData = recordData.subrange(bodyOffset, metaData.bodySize);
        if (metaData.bodyHash != computeSHA1(bodyData))
            return;
    }

    readOperation.expectedBodyHash = metaData.bodyHash;
    readOperation.resultRecord = std::make_unique<Storage::Record>(Storage::Record {
        metaData.key,
        timeStamp,
        headerData,
        bodyData
    });
}

static Data encodeRecordMetaData(const RecordMetaData& metaData)
{
    Encoder encoder;

    encoder << metaData.cacheStorageVersion;
    encoder << metaData.key;
    encoder << metaData.epochRelativeTimeStamp;
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
    auto bodyPath = bodyPathForKey(writeOperation.record.key);

    // Store the body.
    auto blob = m_blobStorage.add(bodyPath, writeOperation.record.body);
    if (blob.data.isNull())
        return { };

    ++writeOperation.activeCount;

    RunLoop::main().dispatch([this, blob, &writeOperation] {
        if (m_bodyFilter)
            m_bodyFilter->add(writeOperation.record.key.hash());
        if (m_synchronizationInProgress)
            m_bodyFilterHashesAddedDuringSynchronization.append(writeOperation.record.key.hash());

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
    metaData.epochRelativeTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(record.timeStamp.time_since_epoch());
    metaData.headerHash = computeSHA1(record.header);
    metaData.headerSize = record.header.size();
    metaData.bodyHash = blob ? blob.value().hash : computeSHA1(record.body);
    metaData.bodySize = record.body.size();
    metaData.isBodyInline = !blob;

    auto encodedMetaData = encodeRecordMetaData(metaData);
    auto headerData = concatenate(encodedMetaData, record.header);

    if (metaData.isBodyInline)
        return concatenate(headerData, record.body);

    return { headerData };
}

bool Storage::removeFromPendingWriteOperations(const Key& key)
{
    auto end = m_pendingWriteOperations.end();
    for (auto it = m_pendingWriteOperations.begin(); it != end; ++it) {
        if ((*it)->record.key == key) {
            m_pendingWriteOperations.remove(it);
            return true;
        }
    }
    return false;
}

void Storage::remove(const Key& key)
{
    ASSERT(RunLoop::isMain());

    if (!mayContain(key))
        return;

    // We can't remove the key from the Bloom filter (but some false positives are expected anyway).
    // For simplicity we also don't reduce m_approximateSize on removals.
    // The next synchronization will update everything.

    removeFromPendingWriteOperations(key);

    serialBackgroundIOQueue().dispatch([this, key] {
        WebCore::deleteFile(recordPathForKey(key));
        m_blobStorage.remove(bodyPathForKey(key));
    });
}

void Storage::updateFileModificationTime(const String& path)
{
    StringCapture filePathCapture(path);
    serialBackgroundIOQueue().dispatch([filePathCapture] {
        updateFileModificationTimeIfNeeded(filePathCapture.string());
    });
}

void Storage::dispatchReadOperation(ReadOperation& readOperation)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeReadOperations.contains(&readOperation));

    bool shouldGetBodyBlob = !m_bodyFilter || m_bodyFilter->mayContain(readOperation.key.hash());

    ioQueue().dispatch([this, &readOperation, shouldGetBodyBlob] {
        auto recordPath = recordPathForKey(readOperation.key);

        ++readOperation.activeCount;
        if (shouldGetBodyBlob)
            ++readOperation.activeCount;

        auto channel = IOChannel::open(recordPath, IOChannel::Type::Read);
        channel->read(0, std::numeric_limits<size_t>::max(), &ioQueue(), [this, &readOperation](const Data& fileData, int error) {
            if (!error)
                readRecord(readOperation, fileData);
            finishReadOperation(readOperation);
        });

        if (shouldGetBodyBlob) {
            // Read the body blob in parallel with the record read.
            auto bodyPath = bodyPathForKey(readOperation.key);
            readOperation.resultBodyBlob = m_blobStorage.get(bodyPath);
            finishReadOperation(readOperation);
        }
    });
}

void Storage::finishReadOperation(ReadOperation& readOperation)
{
    ASSERT(readOperation.activeCount);
    // Record and body blob reads must finish.
    if (--readOperation.activeCount)
        return;

    RunLoop::main().dispatch([this, &readOperation] {
        if (readOperation.resultRecord && readOperation.resultRecord->body.isNull()) {
            if (readOperation.resultBodyBlob.hash == readOperation.expectedBodyHash)
                readOperation.resultRecord->body = readOperation.resultBodyBlob.data;
            else
                readOperation.resultRecord = nullptr;
        }

        bool success = readOperation.completionHandler(WTF::move(readOperation.resultRecord));
        if (success)
            updateFileModificationTime(recordPathForKey(readOperation.key));
        else
            remove(readOperation.key);
        ASSERT(m_activeReadOperations.contains(&readOperation));
        m_activeReadOperations.remove(&readOperation);
        dispatchPendingReadOperations();

        LOG(NetworkCacheStorage, "(NetworkProcess) read complete success=%d", success);
    });
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
        auto readOperation = pendingRetrieveQueue.takeLast();
        auto& read = *readOperation;
        m_activeReadOperations.add(WTF::move(readOperation));
        dispatchReadOperation(read);
    }
}

template <class T> bool retrieveFromMemory(const T& operations, const Key& key, Storage::RetrieveCompletionHandler& completionHandler)
{
    for (auto& operation : operations) {
        if (operation->record.key == key) {
            LOG(NetworkCacheStorage, "(NetworkProcess) found write operation in progress");
            auto record = operation->record;
            RunLoop::main().dispatch([record, completionHandler] {
                completionHandler(std::make_unique<Storage::Record>(record));
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
        auto writeOperation = m_pendingWriteOperations.takeLast();
        auto& write = *writeOperation;
        m_activeWriteOperations.add(WTF::move(writeOperation));

        dispatchWriteOperation(write);
    }
}

static bool shouldStoreBodyAsBlob(const Data& bodyData)
{
    const size_t maximumInlineBodySize { 16 * 1024 };
    return bodyData.size() > maximumInlineBodySize;
}

void Storage::dispatchWriteOperation(WriteOperation& writeOperation)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeWriteOperations.contains(&writeOperation));

    // This was added already when starting the store but filter might have been wiped.
    addToRecordFilter(writeOperation.record.key);

    backgroundIOQueue().dispatch([this, &writeOperation] {
        auto partitionPath = partitionPathForKey(writeOperation.record.key);
        auto recordPath = recordPathForKey(writeOperation.record.key);

        WebCore::makeAllDirectories(partitionPath);

        ++writeOperation.activeCount;

        bool shouldStoreAsBlob = shouldStoreBodyAsBlob(writeOperation.record.body);
        auto bodyBlob = shouldStoreAsBlob ? storeBodyAsBlob(writeOperation) : Nullopt;

        auto recordData = encodeRecord(writeOperation.record, bodyBlob);

        auto channel = IOChannel::open(recordPath, IOChannel::Type::Create);
        size_t recordSize = recordData.size();
        channel->write(0, recordData, nullptr, [this, &writeOperation, recordSize](int error) {
            // On error the entry still stays in the contents filter until next synchronization.
            m_approximateRecordsSize += recordSize;
            finishWriteOperation(writeOperation);

            LOG(NetworkCacheStorage, "(NetworkProcess) write complete error=%d", error);
        });
    });
}

void Storage::finishWriteOperation(WriteOperation& writeOperation)
{
    ASSERT(RunLoop::isMain());
    ASSERT(writeOperation.activeCount);
    ASSERT(m_activeWriteOperations.contains(&writeOperation));

    if (--writeOperation.activeCount)
        return;

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
        completionHandler(nullptr);
        return;
    }

    if (!mayContain(key)) {
        completionHandler(nullptr);
        return;
    }

    if (retrieveFromMemory(m_pendingWriteOperations, key, completionHandler))
        return;
    if (retrieveFromMemory(m_activeWriteOperations, key, completionHandler))
        return;

    auto readOperation = std::make_unique<ReadOperation>(key, WTF::move(completionHandler));
    m_pendingReadOperationsByPriority[priority].prepend(WTF::move(readOperation));
    dispatchPendingReadOperations();
}

void Storage::store(const Record& record, MappedBodyHandler&& mappedBodyHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!record.key.isNull());

    if (!m_capacity)
        return;

    auto writeOperation = std::make_unique<WriteOperation>(record, WTF::move(mappedBodyHandler));
    m_pendingWriteOperations.prepend(WTF::move(writeOperation));

    // Add key to the filter already here as we do lookups from the pending operations too.
    addToRecordFilter(record.key);

    bool isInitialWrite = m_pendingWriteOperations.size() == 1;
    if (!isInitialWrite)
        return;

    // Delay the start of writes a bit to avoid affecting early page load.
    // Completing writes will dispatch more writes without delay.
    static const auto initialWriteDelay = 1_s;
    m_writeOperationDispatchTimer.startOneShot(initialWriteDelay);
}

void Storage::traverse(TraverseFlags flags, TraverseHandler&& traverseHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(traverseHandler);
    // Avoid non-thread safe std::function copies.

    auto traverseOperationPtr = std::make_unique<TraverseOperation>(flags, WTF::move(traverseHandler));
    auto& traverseOperation = *traverseOperationPtr;
    m_activeTraverseOperations.add(WTF::move(traverseOperationPtr));

    ioQueue().dispatch([this, &traverseOperation] {
        traverseRecordsFiles(recordsPath(), [this, &traverseOperation](const String& fileName, const String& partitionPath) {
            if (fileName.length() != Key::hashStringLength())
                return;
            auto recordPath = WebCore::pathByAppendingComponent(partitionPath, fileName);

            double worth = -1;
            if (traverseOperation.flags & TraverseFlag::ComputeWorth)
                worth = computeRecordWorth(fileTimes(recordPath));
            unsigned bodyShareCount = 0;
            if (traverseOperation.flags & TraverseFlag::ShareCount)
                bodyShareCount = m_blobStorage.shareCount(bodyPathForRecordPath(recordPath));

            std::unique_lock<std::mutex> lock(traverseOperation.activeMutex);
            ++traverseOperation.activeCount;

            auto channel = IOChannel::open(recordPath, IOChannel::Type::Read);
            channel->read(0, std::numeric_limits<size_t>::max(), nullptr, [this, &traverseOperation, worth, bodyShareCount](Data& fileData, int) {
                RecordMetaData metaData;
                Data headerData;
                if (decodeRecordHeader(fileData, metaData, headerData)) {
                    Record record {
                        metaData.key,
                        std::chrono::system_clock::time_point(metaData.epochRelativeTimeStamp),
                        headerData,
                        { }
                    };
                    RecordInfo info {
                        static_cast<size_t>(metaData.bodySize),
                        worth,
                        bodyShareCount,
                        String::fromUTF8(SHA1::hexDigest(metaData.bodyHash))
                    };
                    traverseOperation.handler(&record, info);
                }

                std::lock_guard<std::mutex> lock(traverseOperation.activeMutex);
                --traverseOperation.activeCount;
                traverseOperation.activeCondition.notify_one();
            });

            const unsigned maximumParallelReadCount = 5;
            traverseOperation.activeCondition.wait(lock, [&traverseOperation] {
                return traverseOperation.activeCount <= maximumParallelReadCount;
            });
        });
        // Wait for all reads to finish.
        std::unique_lock<std::mutex> lock(traverseOperation.activeMutex);
        traverseOperation.activeCondition.wait(lock, [&traverseOperation] {
            return !traverseOperation.activeCount;
        });
        RunLoop::main().dispatch([this, &traverseOperation] {
            traverseOperation.handler(nullptr, { });
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

void Storage::clear(std::chrono::system_clock::time_point modifiedSinceTime, std::function<void ()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    LOG(NetworkCacheStorage, "(NetworkProcess) clearing cache");

    if (m_recordFilter)
        m_recordFilter->clear();
    if (m_bodyFilter)
        m_bodyFilter->clear();
    m_approximateRecordsSize = 0;

    // Avoid non-thread safe std::function copies.
    auto* completionHandlerPtr = completionHandler ? new std::function<void ()>(WTF::move(completionHandler)) : nullptr;

    ioQueue().dispatch([this, modifiedSinceTime, completionHandlerPtr] {
        auto recordsPath = this->recordsPath();
        traverseRecordsFiles(recordsPath, [modifiedSinceTime](const String& fileName, const String& partitionPath) {
            auto filePath = WebCore::pathByAppendingComponent(partitionPath, fileName);
            if (modifiedSinceTime > std::chrono::system_clock::time_point::min()) {
                auto times = fileTimes(filePath);
                if (times.modification < modifiedSinceTime)
                    return;
            }
            WebCore::deleteFile(filePath);
        });

        deleteEmptyRecordsDirectories(recordsPath);

        // This cleans unreferences blobs.
        m_blobStorage.synchronize();

        if (completionHandlerPtr) {
            RunLoop::main().dispatch([completionHandlerPtr] {
                (*completionHandlerPtr)();
                delete completionHandlerPtr;
            });
        }
    });
}

static double computeRecordWorth(FileTimes times)
{
    using namespace std::chrono;
    auto age = system_clock::now() - times.creation;
    // File modification time is updated manually on cache read. We don't use access time since OS may update it automatically.
    auto accessAge = times.modification - times.creation;

    // For sanity.
    if (age <= 0_s || accessAge < 0_s || accessAge > age)
        return 0;

    // We like old entries that have been accessed recently.
    return duration<double>(accessAge) / age;
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

    backgroundIOQueue().dispatch([this] {
        auto recordsPath = this->recordsPath();
        traverseRecordsFiles(recordsPath, [this](const String& fileName, const String& partitionPath) {
            if (fileName.length() != Key::hashStringLength())
                return;
            auto recordPath = WebCore::pathByAppendingComponent(partitionPath, fileName);
            auto bodyPath = bodyPathForRecordPath(recordPath);

            auto times = fileTimes(recordPath);
            unsigned bodyShareCount = m_blobStorage.shareCount(bodyPath);
            auto probability = deletionProbability(times, bodyShareCount);

            bool shouldDelete = randomNumber() < probability;

            LOG(NetworkCacheStorage, "Deletion probability=%f bodyLinkCount=%d shouldDelete=%d", probability, bodyShareCount, shouldDelete);

            if (shouldDelete) {
                WebCore::deleteFile(recordPath);
                m_blobStorage.remove(bodyPath);
            }
        });

        RunLoop::main().dispatch([this] {
            m_shrinkInProgress = false;
            // We could synchronize during the shrink traversal. However this is fast and it is better to have just one code path.
            synchronize();
        });

        LOG(NetworkCacheStorage, "(NetworkProcess) cache shrink completed");
    });
}

void Storage::deleteOldVersions()
{
    backgroundIOQueue().dispatch([this] {
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

            auto oldVersionPath = WebCore::pathByAppendingComponent(cachePath, subdirName);
            LOG(NetworkCacheStorage, "(NetworkProcess) deleting old cache version, path %s", oldVersionPath.utf8().data());

            deleteDirectoryRecursively(oldVersionPath);
        });
    });
}

}
}

#endif
