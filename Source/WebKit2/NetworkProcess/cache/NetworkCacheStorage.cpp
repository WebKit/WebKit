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

Storage::Storage(const String& baseDirectoryPath)
    : m_baseDirectoryPath(baseDirectoryPath)
    , m_directoryPath(makeVersionedDirectoryPath(baseDirectoryPath))
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage", WorkQueue::Type::Concurrent))
    , m_backgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.background", WorkQueue::Type::Concurrent, WorkQueue::QOS::Background))
    , m_serialBackgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Storage.serialBackground", WorkQueue::Type::Serial, WorkQueue::QOS::Background))
{
    deleteOldVersions();
    initialize();
}

void Storage::initialize()
{
    ASSERT(RunLoop::isMain());

    StringCapture cachePathCapture(m_directoryPath);

    backgroundIOQueue().dispatch([this, cachePathCapture] {
        String cachePath = cachePathCapture.string();
        traverseCacheFiles(cachePath, [this](const String& fileName, const String& partitionPath) {
            Key::HashType hash;
            if (!Key::stringToHash(fileName, hash))
                return;
            unsigned shortHash = Key::toShortHash(hash);
            RunLoop::main().dispatch([this, shortHash] {
                m_contentsFilter.add(shortHash);
            });
            auto filePath = WebCore::pathByAppendingComponent(partitionPath, fileName);
            long long fileSize = 0;
            WebCore::getFileSize(filePath, fileSize);
            m_approximateSize += fileSize;
        });
        m_hasPopulatedContentsFilter = true;
    });
}

static String directoryPathForKey(const Key& key, const String& cachePath)
{
    ASSERT(!key.partition().isEmpty());
    return WebCore::pathByAppendingComponent(cachePath, key.partition());
}

static String fileNameForKey(const Key& key)
{
    return key.hashAsString();
}

static String filePathForKey(const Key& key, const String& cachePath)
{
    return WebCore::pathByAppendingComponent(directoryPathForKey(key, cachePath), fileNameForKey(key));
}

static Ref<IOChannel> openFileForKey(const Key& key, IOChannel::Type type, const String& cachePath)
{
    auto directoryPath = directoryPathForKey(key, cachePath);
    auto filePath = WebCore::pathByAppendingComponent(directoryPath, fileNameForKey(key));
    if (type == IOChannel::Type::Create)
        WebCore::makeAllDirectories(directoryPath);
    return IOChannel::open(filePath, type);
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
    std::chrono::milliseconds timeStamp;
    unsigned headerChecksum;
    uint64_t headerOffset;
    uint64_t headerSize;
    unsigned bodyChecksum;
    uint64_t bodyOffset;
    uint64_t bodySize;
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
        if (!decoder.decode(metaData.timeStamp))
            return false;
        if (!decoder.decode(metaData.headerChecksum))
            return false;
        if (!decoder.decode(metaData.headerSize))
            return false;
        if (!decoder.decode(metaData.bodyChecksum))
            return false;
        if (!decoder.decode(metaData.bodySize))
            return false;
        if (!decoder.verifyChecksum())
            return false;
        metaData.headerOffset = decoder.currentOffset();
        metaData.bodyOffset = WTF::roundUpToMultipleOf(pageSize(), metaData.headerOffset + metaData.headerSize);
        success = true;
        return false;
    });
    return success;
}

static bool decodeRecordHeader(const Data& fileData, RecordMetaData& metaData, Data& data)
{
    if (!decodeRecordMetaData(metaData, fileData)) {
        LOG(NetworkCacheStorage, "(NetworkProcess) meta data decode failure");
        return false;
    }

    if (metaData.cacheStorageVersion != Storage::version) {
        LOG(NetworkCacheStorage, "(NetworkProcess) version mismatch");
        return false;
    }
    if (metaData.headerOffset + metaData.headerSize > metaData.bodyOffset) {
        LOG(NetworkCacheStorage, "(NetworkProcess) body offset mismatch");
        return false;
    }

    auto headerData = fileData.subrange(metaData.headerOffset, metaData.headerSize);
    if (metaData.headerChecksum != hashData(headerData)) {
        LOG(NetworkCacheStorage, "(NetworkProcess) header checksum mismatch");
        return false;
    }
    data = { headerData };
    return true;
}

static std::unique_ptr<Storage::Record> decodeRecord(const Data& fileData, int fd, const Key& key)
{
    RecordMetaData metaData;
    Data headerData;
    if (!decodeRecordHeader(fileData, metaData, headerData))
        return nullptr;

    if (metaData.key != key)
        return nullptr;

    Data bodyData;
    if (metaData.bodySize) {
        if (metaData.bodyOffset + metaData.bodySize != fileData.size())
            return nullptr;

        bodyData = mapFile(fd, metaData.bodyOffset, metaData.bodySize);
        if (bodyData.isNull()) {
            LOG(NetworkCacheStorage, "(NetworkProcess) map failed");
            return nullptr;
        }

        if (metaData.bodyChecksum != hashData(bodyData)) {
            LOG(NetworkCacheStorage, "(NetworkProcess) data checksum mismatch");
            return nullptr;
        }
    }

    return std::make_unique<Storage::Record>(Storage::Record {
        metaData.key,
        metaData.timeStamp,
        headerData,
        bodyData
    });
}

static Data encodeRecordMetaData(const RecordMetaData& metaData)
{
    Encoder encoder;

    encoder << metaData.cacheStorageVersion;
    encoder << metaData.key;
    encoder << metaData.timeStamp;
    encoder << metaData.headerChecksum;
    encoder << metaData.headerSize;
    encoder << metaData.bodyChecksum;
    encoder << metaData.bodySize;

    encoder.encodeChecksum();

    return Data(encoder.buffer(), encoder.bufferSize());
}

static Data encodeRecordHeader(const Storage::Record& record)
{
    RecordMetaData metaData(record.key);
    metaData.timeStamp = record.timeStamp;
    metaData.headerChecksum = hashData(record.header);
    metaData.headerSize = record.header.size();
    metaData.bodyChecksum = hashData(record.body);
    metaData.bodySize = record.body.size();

    auto encodedMetaData = encodeRecordMetaData(metaData);
    auto headerData = concatenate(encodedMetaData, record.header);
    if (!record.body.size())
        return { headerData };

    size_t dataOffset = WTF::roundUpToMultipleOf(pageSize(), headerData.size());
    Vector<uint8_t, 4096> filler(dataOffset - headerData.size(), 0);
    Data alignmentData(filler.data(), filler.size());

    return concatenate(headerData, alignmentData);
}

void Storage::remove(const Key& key)
{
    ASSERT(RunLoop::isMain());

    // For simplicity we don't reduce m_approximateSize on removals.
    // The next cache shrink will update the size.

    if (m_contentsFilter.mayContain(key.shortHash()))
        m_contentsFilter.remove(key.shortHash());

    StringCapture filePathCapture(filePathForKey(key, m_directoryPath));
    serialBackgroundIOQueue().dispatch([this, filePathCapture] {
        WebCore::deleteFile(filePathCapture.string());
    });
}

void Storage::updateFileModificationTime(IOChannel& channel)
{
    StringCapture filePathCapture(channel.path());
    serialBackgroundIOQueue().dispatch([filePathCapture] {
        updateFileModificationTimeIfNeeded(filePathCapture.string());
    });
}

void Storage::dispatchReadOperation(const ReadOperation& read)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeReadOperations.contains(&read));

    StringCapture cachePathCapture(m_directoryPath);
    ioQueue().dispatch([this, &read, cachePathCapture] {
        RefPtr<IOChannel> channel = openFileForKey(read.key, IOChannel::Type::Read, cachePathCapture.string());
        channel->read(0, std::numeric_limits<size_t>::max(), [this, channel, &read](Data& fileData, int error) {
            if (error) {
                remove(read.key);
                read.completionHandler(nullptr);
            } else {
                auto record = decodeRecord(fileData, channel->fileDescriptor(), read.key);
                bool success = read.completionHandler(WTF::move(record));
                if (success)
                    updateFileModificationTime(*channel);
                else
                    remove(read.key);
            }

            ASSERT(m_activeReadOperations.contains(&read));
            m_activeReadOperations.remove(&read);
            dispatchPendingReadOperations();

            LOG(NetworkCacheStorage, "(NetworkProcess) read complete error=%d", error);
        });
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

void Storage::retrieve(const Key& key, unsigned priority, RetrieveCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(priority <= maximumRetrievePriority);
    ASSERT(!key.isNull());

    if (!m_maximumSize) {
        completionHandler(nullptr);
        return;
    }

    if (!cacheMayContain(key.shortHash())) {
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

void Storage::store(const Record& record, StoreCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!record.key.isNull());

    if (!m_maximumSize) {
        completionHandler(false, { });
        return;
    }

    m_pendingWriteOperations.append(new WriteOperation { record, { }, WTF::move(completionHandler) });

    // Add key to the filter already here as we do lookups from the pending operations too.
    m_contentsFilter.add(record.key.shortHash());

    dispatchPendingWriteOperations();
}

void Storage::update(const Record& updateRecord, const Record& existingRecord, StoreCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!existingRecord.key.isNull());
    ASSERT(existingRecord.key == updateRecord.key);

    if (!m_maximumSize) {
        completionHandler(false, { });
        return;
    }

    m_pendingWriteOperations.append(new WriteOperation { updateRecord, existingRecord, WTF::move(completionHandler) });

    dispatchPendingWriteOperations();
}

void Storage::traverse(TraverseFlags flags, std::function<void (const Record*, const RecordInfo&)>&& traverseHandler)
{
    StringCapture cachePathCapture(m_directoryPath);
    ioQueue().dispatch([this, flags, cachePathCapture, traverseHandler] {
        String cachePath = cachePathCapture.string();
        traverseCacheFiles(cachePath, [this, flags, &traverseHandler](const String& fileName, const String& partitionPath) {
            auto filePath = WebCore::pathByAppendingComponent(partitionPath, fileName);

            RecordInfo info;
            if (flags & TraverseFlag::ComputeWorth)
                info.worth = computeRecordWorth(fileTimes(filePath));

            auto channel = IOChannel::open(filePath, IOChannel::Type::Read);
            const size_t headerReadSize = 16 << 10;
            // FIXME: Traversal is slower than it should be due to lack of parallelism.
            channel->readSync(0, headerReadSize, [this, &traverseHandler, &info](Data& fileData, int) {
                RecordMetaData metaData;
                Data headerData;
                if (decodeRecordHeader(fileData, metaData, headerData)) {
                    Record record { metaData.key, metaData.timeStamp, headerData, { } };
                    info.bodySize = metaData.bodySize;
                    traverseHandler(&record, info);
                }
            });
        });
        RunLoop::main().dispatch([this, traverseHandler] {
            traverseHandler(nullptr, { });
        });
    });
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

        if (write.existingRecord && cacheMayContain(write.record.key.shortHash())) {
            dispatchHeaderWriteOperation(write);
            continue;
        }
        dispatchFullWriteOperation(write);
    }
}

void Storage::dispatchFullWriteOperation(const WriteOperation& write)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_activeWriteOperations.contains(&write));

    if (!m_contentsFilter.mayContain(write.record.key.shortHash()))
        m_contentsFilter.add(write.record.key.shortHash());

    StringCapture cachePathCapture(m_directoryPath);
    backgroundIOQueue().dispatch([this, &write, cachePathCapture] {
        auto encodedHeader = encodeRecordHeader(write.record);
        auto headerAndBodyData = concatenate(encodedHeader, write.record.body);

        auto channel = openFileForKey(write.record.key, IOChannel::Type::Create, cachePathCapture.string());
        int fd = channel->fileDescriptor();
        size_t bodyOffset = encodedHeader.size();

        channel->write(0, headerAndBodyData, [this, &write, bodyOffset, fd](int error) {
            LOG(NetworkCacheStorage, "(NetworkProcess) write complete error=%d", error);
            if (error) {
                if (m_contentsFilter.mayContain(write.record.key.shortHash()))
                    m_contentsFilter.remove(write.record.key.shortHash());
            }
            size_t bodySize = write.record.body.size();
            size_t totalSize = bodyOffset + bodySize;

            m_approximateSize += totalSize;

            bool shouldMapBody = !error && bodySize >= pageSize();
            auto bodyMap = shouldMapBody ? mapFile(fd, bodyOffset, bodySize) : Data();

            write.completionHandler(!error, bodyMap);

            ASSERT(m_activeWriteOperations.contains(&write));
            m_activeWriteOperations.remove(&write);
            dispatchPendingWriteOperations();
        });
    });

    shrinkIfNeeded();
}

void Storage::dispatchHeaderWriteOperation(const WriteOperation& write)
{
    ASSERT(RunLoop::isMain());
    ASSERT(write.existingRecord);
    ASSERT(m_activeWriteOperations.contains(&write));
    ASSERT(cacheMayContain(write.record.key.shortHash()));

    // Try to update the header of an existing entry.
    StringCapture cachePathCapture(m_directoryPath);
    backgroundIOQueue().dispatch([this, &write, cachePathCapture] {
        auto headerData = encodeRecordHeader(write.record);
        auto existingHeaderData = encodeRecordHeader(write.existingRecord.value());

        bool pageRoundedHeaderSizeChanged = headerData.size() != existingHeaderData.size();
        if (pageRoundedHeaderSizeChanged) {
            LOG(NetworkCacheStorage, "(NetworkProcess) page-rounded header size changed, storing full entry");
            RunLoop::main().dispatch([this, &write] {
                dispatchFullWriteOperation(write);
            });
            return;
        }

        auto channel = openFileForKey(write.record.key, IOChannel::Type::Write, cachePathCapture.string());
        channel->write(0, headerData, [this, &write](int error) {
            LOG(NetworkCacheStorage, "(NetworkProcess) update complete error=%d", error);

            if (error)
                remove(write.record.key);

            write.completionHandler(!error, { });

            ASSERT(m_activeWriteOperations.contains(&write));
            m_activeWriteOperations.remove(&write);
            dispatchPendingWriteOperations();
        });
    });
}

void Storage::setMaximumSize(size_t size)
{
    ASSERT(RunLoop::isMain());
    m_maximumSize = size;

    shrinkIfNeeded();
}

void Storage::clear()
{
    ASSERT(RunLoop::isMain());
    LOG(NetworkCacheStorage, "(NetworkProcess) clearing cache");

    m_contentsFilter.clear();
    m_approximateSize = 0;

    StringCapture directoryPathCapture(m_directoryPath);

    ioQueue().dispatch([directoryPathCapture] {
        String directoryPath = directoryPathCapture.string();
        traverseDirectory(directoryPath, DT_DIR, [&directoryPath](const String& subdirName) {
            String subdirPath = WebCore::pathByAppendingComponent(directoryPath, subdirName);
            traverseDirectory(subdirPath, DT_REG, [&subdirPath](const String& fileName) {
                WebCore::deleteFile(WebCore::pathByAppendingComponent(subdirPath, fileName));
            });
            WebCore::deleteEmptyDirectory(subdirPath);
        });
    });
}

static double computeRecordWorth(FileTimes times)
{
    using namespace std::chrono;
    auto age = system_clock::now() - times.creation;
    // File modification time is updated manually on cache read. We don't use access time since OS may update it automatically.
    auto accessAge = times.modification - times.creation;

    // For sanity.
    if (age <= seconds::zero() || accessAge < seconds::zero() || accessAge > age)
        return 0;

    // We like old entries that have been accessed recently.
    return duration<double>(accessAge) / age;
}


static double deletionProbability(FileTimes times)
{
    static const double maximumProbability { 0.33 };

    auto worth = computeRecordWorth(times);

    // Adjust a bit so the most valuable entries don't get deleted at all.
    auto effectiveWorth = std::min(1.1 * worth, 1.);

    return (1 - effectiveWorth) * maximumProbability;
}

void Storage::shrinkIfNeeded()
{
    ASSERT(RunLoop::isMain());

    if (m_approximateSize <= m_maximumSize)
        return;
    if (m_shrinkInProgress)
        return;
    m_shrinkInProgress = true;

    LOG(NetworkCacheStorage, "(NetworkProcess) shrinking cache approximateSize=%zu, m_maximumSize=%zu", static_cast<size_t>(m_approximateSize), m_maximumSize);

    m_approximateSize = 0;

    StringCapture cachePathCapture(m_directoryPath);
    backgroundIOQueue().dispatch([this, cachePathCapture] {
        String cachePath = cachePathCapture.string();
        traverseCacheFiles(cachePath, [this](const String& fileName, const String& partitionPath) {
            auto filePath = WebCore::pathByAppendingComponent(partitionPath, fileName);

            auto times = fileTimes(filePath);
            auto probability = deletionProbability(times);
            bool shouldDelete = randomNumber() < probability;

            LOG(NetworkCacheStorage, "Deletion probability=%f shouldDelete=%d", probability, shouldDelete);

            if (!shouldDelete) {
                long long fileSize = 0;
                WebCore::getFileSize(filePath, fileSize);
                m_approximateSize += fileSize;
                return;
            }

            WebCore::deleteFile(filePath);
            Key::HashType hash;
            if (!Key::stringToHash(fileName, hash))
                return;
            unsigned shortHash = Key::toShortHash(hash);
            RunLoop::main().dispatch([this, shortHash] {
                if (m_contentsFilter.mayContain(shortHash))
                    m_contentsFilter.remove(shortHash);
            });
        });

        // Let system figure out if they are really empty.
        traverseDirectory(cachePath, DT_DIR, [&cachePath](const String& subdirName) {
            auto partitionPath = WebCore::pathByAppendingComponent(cachePath, subdirName);
            WebCore::deleteEmptyDirectory(partitionPath);
        });

        m_shrinkInProgress = false;

        LOG(NetworkCacheStorage, "(NetworkProcess) cache shrink completed approximateSize=%zu", static_cast<size_t>(m_approximateSize));
    });
}

void Storage::deleteOldVersions()
{
    // Delete V1 cache.
    StringCapture cachePathCapture(m_baseDirectoryPath);
    backgroundIOQueue().dispatch([cachePathCapture] {
        String cachePath = cachePathCapture.string();
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
}

}
}

#endif
