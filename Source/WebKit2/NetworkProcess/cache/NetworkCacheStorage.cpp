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
#include "NetworkCacheFileSystemPosix.h"
#include "NetworkCacheIOChannel.h"
#include <wtf/PageBlock.h>
#include <wtf/RandomNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
namespace NetworkCache {

static const char networkCacheSubdirectory[] = "WebKitCache";
static const char versionDirectoryPrefix[] = "Version ";
static const char recordsDirectoryName[] = "Records";
static const char blobsDirectoryName[] = "Blobs";
static const char bodyPostfix[] = "-body";

static double computeRecordWorth(FileTimes);

std::unique_ptr<Storage> Storage::open(const String& cachePath)
{
    ASSERT(RunLoop::isMain());

    String networkCachePath = WebCore::pathByAppendingComponent(cachePath, networkCacheSubdirectory);
    if (!WebCore::makeAllDirectories(networkCachePath))
        return nullptr;
    return std::unique_ptr<Storage>(new Storage(networkCachePath));
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

Storage::Storage(const String& baseDirectoryPath)
    : m_basePath(baseDirectoryPath)
    , m_recordsPath(makeRecordsDirectoryPath(baseDirectoryPath))
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage", WorkQueue::Type::Concurrent))
    , m_backgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.background", WorkQueue::Type::Concurrent, WorkQueue::QOS::Background))
    , m_serialBackgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.serialBackground", WorkQueue::Type::Serial, WorkQueue::QOS::Background))
    , m_blobStorage(makeBlobDirectoryPath(baseDirectoryPath))
{
    deleteOldVersions();
    synchronize();
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
    return m_approximateSize + m_blobStorage.approximateSize();
}

void Storage::synchronize()
{
    ASSERT(RunLoop::isMain());

    if (m_synchronizationInProgress || m_shrinkInProgress)
        return;
    m_synchronizationInProgress = true;

    LOG(NetworkCacheStorage, "(NetworkProcess) synchronizing cache");

    backgroundIOQueue().dispatch([this] {
        auto filter = std::make_unique<ContentsFilter>();
        size_t size = 0;
        unsigned count = 0;
        traverseCacheFiles(recordsPath(), [&filter, &size, &count](const String& fileName, const String& partitionPath) {
            Key::HashType hash;
            if (!Key::stringToHash(fileName, hash))
                return;
            auto filePath = WebCore::pathByAppendingComponent(partitionPath, fileName);
            long long fileSize = 0;
            WebCore::getFileSize(filePath, fileSize);
            if (!fileSize)
                return;
            filter->add(hash);
            size += fileSize;
            ++count;
        });

        auto* filterPtr = filter.release();
        RunLoop::main().dispatch([this, filterPtr, size] {
            auto filter = std::unique_ptr<ContentsFilter>(filterPtr);

            for (auto hash : m_contentsFilterHashesAddedDuringSynchronization)
                filter->add(hash);
            m_contentsFilterHashesAddedDuringSynchronization.clear();

            m_contentsFilter = WTF::move(filter);
            m_approximateSize = size;
            m_synchronizationInProgress = false;
        });

        m_blobStorage.synchronize();

        LOG(NetworkCacheStorage, "(NetworkProcess) cache synchronization completed size=%zu count=%d", size, count);
    });
}

void Storage::addToContentsFilter(const Key& key)
{
    ASSERT(RunLoop::isMain());

    if (m_contentsFilter)
        m_contentsFilter->add(key.hash());

    // If we get new entries during filter synchronization take care to add them to the new filter as well.
    if (m_synchronizationInProgress)
        m_contentsFilterHashesAddedDuringSynchronization.append(key.hash());
}

bool Storage::mayContain(const Key& key) const
{
    ASSERT(RunLoop::isMain());
    return !m_contentsFilter || m_contentsFilter->mayContain(key.hash());
}

static String partitionPathForKey(const Key& key, const String& cachePath)
{
    ASSERT(!key.partition().isEmpty());
    return WebCore::pathByAppendingComponent(cachePath, key.partition());
}

static String fileNameForKey(const Key& key)
{
    return key.hashAsString();
}

static String recordPathForKey(const Key& key, const String& cachePath)
{
    return WebCore::pathByAppendingComponent(partitionPathForKey(key, cachePath), fileNameForKey(key));
}

static String bodyPathForRecordPath(const String& recordPath)
{
    return recordPath + bodyPostfix;
}

static String bodyPathForKey(const Key& key, const String& cachePath)
{
    return bodyPathForRecordPath(recordPathForKey(key, cachePath));
}

static unsigned hashData(const Data& data)
{
    StringHasher hasher;
    data.apply([&hasher](const uint8_t* data, size_t size) {
        hasher.addCharacters(data, size);
        return true;
    });
    return hasher.hash();
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
    unsigned headerChecksum;
    uint64_t headerOffset;
    uint64_t headerSize;
    SHA1::Digest bodyHash;
    uint64_t bodySize;
    bool isBodyInline;
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
        if (!decoder.decode(metaData.headerChecksum))
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
    if (metaData.headerChecksum != hashData(headerData)) {
        LOG(NetworkCacheStorage, "(NetworkProcess) header checksum mismatch");
        return false;
    }
    return true;
}

std::unique_ptr<Storage::Record> Storage::decodeRecord(const Data& recordData, const Key& key)
{
    ASSERT(!RunLoop::isMain());

    RecordMetaData metaData;
    Data headerData;
    if (!decodeRecordHeader(recordData, metaData, headerData))
        return nullptr;

    if (metaData.key != key)
        return nullptr;

    // Sanity check against time stamps in future.
    auto timeStamp = std::chrono::system_clock::time_point(metaData.epochRelativeTimeStamp);
    if (timeStamp > std::chrono::system_clock::now())
        return nullptr;

    Data bodyData;
    if (metaData.isBodyInline) {
        size_t bodyOffset = metaData.headerOffset + headerData.size();
        if (bodyOffset + metaData.bodySize != recordData.size())
            return nullptr;
        bodyData = recordData.subrange(bodyOffset, metaData.bodySize);
        if (metaData.bodyHash != computeSHA1(bodyData))
            return nullptr;
    } else {
        auto bodyPath = bodyPathForKey(key, recordsPath());
        auto bodyBlob = m_blobStorage.get(bodyPath);
        if (metaData.bodySize != bodyBlob.data.size())
            return nullptr;
        if (metaData.bodyHash != bodyBlob.hash)
            return nullptr;
        bodyData = bodyBlob.data;
    }

    return std::make_unique<Storage::Record>(Storage::Record {
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
    encoder << metaData.headerChecksum;
    encoder << metaData.headerSize;
    encoder << metaData.bodyHash;
    encoder << metaData.bodySize;
    encoder << metaData.isBodyInline;

    encoder.encodeChecksum();

    return Data(encoder.buffer(), encoder.bufferSize());
}

Optional<BlobStorage::Blob> Storage::storeBodyAsBlob(const Record& record, const MappedBodyHandler& mappedBodyHandler)
{
    auto bodyPath = bodyPathForKey(record.key, recordsPath());

    // Store the body.
    auto blob = m_blobStorage.add(bodyPath, record.body);
    if (blob.data.isNull())
        return { };

    // Tell the client we now have a disk-backed map for this data.
    if (mappedBodyHandler) {
        RunLoop::main().dispatch([blob, mappedBodyHandler] {
            mappedBodyHandler(blob.data);
        });
    }
    return blob;
}

Data Storage::encodeRecord(const Record& record, Optional<BlobStorage::Blob> blob)
{
    ASSERT(!blob || bytesEqual(blob.value().data, record.body));

    RecordMetaData metaData(record.key);
    metaData.epochRelativeTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(record.timeStamp.time_since_epoch());
    metaData.headerChecksum = hashData(record.header);
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

void Storage::remove(const Key& key)
{
    ASSERT(RunLoop::isMain());

    // We can't remove the key from the Bloom filter (but some false positives are expected anyway).
    // For simplicity we also don't reduce m_approximateSize on removals.
    // The next synchronization will update everything.

    serialBackgroundIOQueue().dispatch([this, key] {
        auto recordsPath = this->recordsPath();
        WebCore::deleteFile(recordPathForKey(key, recordsPath));
        m_blobStorage.remove(bodyPathForKey(key, recordsPath));
    });
}

void Storage::updateFileModificationTime(const String& path)
{
    StringCapture filePathCapture(path);
    serialBackgroundIOQueue().dispatch([filePathCapture] {
        updateFileModificationTimeIfNeeded(filePathCapture.string());
    });
}

void Storage::dispatchReadOperation(const ReadOperation& read)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeReadOperations.contains(&read));

    auto recordsPath = this->recordsPath();
    auto recordPath = recordPathForKey(read.key, recordsPath);

    RefPtr<IOChannel> channel = IOChannel::open(recordPath, IOChannel::Type::Read);
    channel->read(0, std::numeric_limits<size_t>::max(), &ioQueue(), [this, &read](const Data& fileData, int error) {
        auto record = error ? nullptr : decodeRecord(fileData, read.key);

        auto* recordPtr = record.release();
        RunLoop::main().dispatch([this, &read, recordPtr] {
            auto record = std::unique_ptr<Record>(recordPtr);
            finishReadOperation(read, WTF::move(record));
        });
    });
}

void Storage::finishReadOperation(const ReadOperation& read, std::unique_ptr<Record> record)
{
    ASSERT(RunLoop::isMain());

    bool success = read.completionHandler(WTF::move(record));
    if (success)
        updateFileModificationTime(recordPathForKey(read.key, recordsPath()));
    else
        remove(read.key);
    ASSERT(m_activeReadOperations.contains(&read));
    m_activeReadOperations.remove(&read);
    dispatchPendingReadOperations();

    LOG(NetworkCacheStorage, "(NetworkProcess) read complete success=%d", success);
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
        auto readOperation = pendingRetrieveQueue.takeFirst();
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

    const int maximumActiveWriteOperationCount { 3 };

    while (!m_pendingWriteOperations.isEmpty()) {
        if (m_activeWriteOperations.size() >= maximumActiveWriteOperationCount) {
            LOG(NetworkCacheStorage, "(NetworkProcess) limiting parallel writes");
            return;
        }
        auto writeOperation = m_pendingWriteOperations.takeFirst();
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

void Storage::dispatchWriteOperation(const WriteOperation& write)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeWriteOperations.contains(&write));

    // This was added already when starting the store but filter might have been wiped.
    addToContentsFilter(write.record.key);

    backgroundIOQueue().dispatch([this, &write] {
        auto recordsPath = this->recordsPath();
        auto partitionPath = partitionPathForKey(write.record.key, recordsPath);
        auto recordPath = recordPathForKey(write.record.key, recordsPath);

        WebCore::makeAllDirectories(partitionPath);

        bool shouldStoreAsBlob = shouldStoreBodyAsBlob(write.record.body);
        auto bodyBlob = shouldStoreAsBlob ? storeBodyAsBlob(write.record, write.mappedBodyHandler) : Nullopt;

        auto recordData = encodeRecord(write.record, bodyBlob);

        auto channel = IOChannel::open(recordPath, IOChannel::Type::Create);
        size_t recordSize = recordData.size();
        channel->write(0, recordData, nullptr, [this, &write, recordSize](int error) {
            // On error the entry still stays in the contents filter until next synchronization.
            m_approximateSize += recordSize;
            finishWriteOperation(write);

            LOG(NetworkCacheStorage, "(NetworkProcess) write complete error=%d", error);
        });
    });
}

void Storage::finishWriteOperation(const WriteOperation& write)
{
    ASSERT(m_activeWriteOperations.contains(&write));
    m_activeWriteOperations.remove(&write);
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

    m_pendingReadOperationsByPriority[priority].append(new ReadOperation { key, WTF::move(completionHandler) });
    dispatchPendingReadOperations();
}

void Storage::store(const Record& record, MappedBodyHandler&& mappedBodyHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!record.key.isNull());

    if (!m_capacity)
        return;

    m_pendingWriteOperations.append(new WriteOperation { record, WTF::move(mappedBodyHandler) });

    // Add key to the filter already here as we do lookups from the pending operations too.
    addToContentsFilter(record.key);

    dispatchPendingWriteOperations();
}

void Storage::traverse(TraverseFlags flags, std::function<void (const Record*, const RecordInfo&)>&& traverseHandler)
{
    ioQueue().dispatch([this, flags, traverseHandler] {
        traverseCacheFiles(recordsPath(), [this, flags, &traverseHandler](const String& fileName, const String& partitionPath) {
            auto recordPath = WebCore::pathByAppendingComponent(partitionPath, fileName);

            RecordInfo info;
            if (flags & TraverseFlag::ComputeWorth)
                info.worth = computeRecordWorth(fileTimes(recordPath));
            if (flags & TraverseFlag::ShareCount)
                info.bodyShareCount = m_blobStorage.shareCount(bodyPathForRecordPath(recordPath));

            auto channel = IOChannel::open(recordPath, IOChannel::Type::Read);
            // FIXME: Traversal is slower than it should be due to lack of parallelism.
            channel->readSync(0, std::numeric_limits<size_t>::max(), nullptr, [this, &traverseHandler, &info](Data& fileData, int) {
                RecordMetaData metaData;
                Data headerData;
                if (decodeRecordHeader(fileData, metaData, headerData)) {
                    Record record { metaData.key, std::chrono::system_clock::time_point(metaData.epochRelativeTimeStamp), headerData, { } };
                    info.bodySize = metaData.bodySize;
                    info.bodyHash = String::fromUTF8(SHA1::hexDigest(metaData.bodyHash));
                    traverseHandler(&record, info);
                }
            });
        });
        RunLoop::main().dispatch([this, traverseHandler] {
            traverseHandler(nullptr, { });
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

void Storage::clear()
{
    ASSERT(RunLoop::isMain());
    LOG(NetworkCacheStorage, "(NetworkProcess) clearing cache");

    if (m_contentsFilter)
        m_contentsFilter->clear();
    m_approximateSize = 0;

    ioQueue().dispatch([this] {
        auto recordsPath = this->recordsPath();
        traverseDirectory(recordsPath, DT_DIR, [&recordsPath](const String& subdirName) {
            String subdirPath = WebCore::pathByAppendingComponent(recordsPath, subdirName);
            traverseDirectory(subdirPath, DT_REG, [&subdirPath](const String& fileName) {
                WebCore::deleteFile(WebCore::pathByAppendingComponent(subdirPath, fileName));
            });
            WebCore::deleteEmptyDirectory(subdirPath);
        });

        // This cleans unreferences blobs.
        m_blobStorage.synchronize();
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
        traverseCacheFiles(recordsPath, [this](const String& fileName, const String& partitionPath) {
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

        // Let system figure out if they are really empty.
        traverseDirectory(recordsPath, DT_DIR, [&recordsPath](const String& subdirName) {
            auto partitionPath = WebCore::pathByAppendingComponent(recordsPath, subdirName);
            WebCore::deleteEmptyDirectory(partitionPath);
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
    // Delete V1 cache.
    backgroundIOQueue().dispatch([this] {
        auto cachePath = basePath();
        traverseDirectory(cachePath, DT_DIR, [&cachePath](const String& subdirName) {
            if (subdirName.startsWith(versionDirectoryPrefix))
                return;
            String partitionPath = WebCore::pathByAppendingComponent(cachePath, subdirName);
            traverseDirectory(partitionPath, DT_REG, [&partitionPath](const String& fileName) {
                WebCore::deleteFile(WebCore::pathByAppendingComponent(partitionPath, fileName));
            });
            WebCore::deleteEmptyDirectory(partitionPath);
        });
    });
    // FIXME: Delete V2 cache.
}

}
}

#endif
