/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include <WebCore/FileSystem.h>
#include <dirent.h>
#include <dispatch/dispatch.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

static const char* networkCacheSubdirectory = "WebKitCache";

template <typename Function>
static void traverseDirectory(const String& path, uint8_t type, const Function& function)
{
    DIR* dir = opendir(WebCore::fileSystemRepresentation(path).data());
    if (!dir)
        return;
    struct dirent* dp;
    while ((dp = readdir(dir))) {
        if (dp->d_type != type)
            continue;
        const char* name = dp->d_name;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        function(String(name));
    }
    closedir(dir);
}

static void traverseCacheFiles(const String& cachePath, std::function<void (const String& fileName, const String& partitionPath)> function)
{
    traverseDirectory(cachePath, DT_DIR, [&cachePath, &function](const String& subdirName) {
        String partitionPath = WebCore::pathByAppendingComponent(cachePath, subdirName);
        traverseDirectory(partitionPath, DT_REG, [&function, &partitionPath](const String& fileName) {
            if (fileName.length() != 8)
                return;
            function(fileName, partitionPath);
        });
    });
}

NetworkCacheStorage::Data::Data()
    : m_data(nullptr)
    , m_size(0)
{
}

NetworkCacheStorage::Data::Data(const uint8_t* data, size_t size)
    : m_dispatchData(adoptOSObject(dispatch_data_create(data, size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT)))
    , m_data(nullptr)
    , m_size(size)
{
}

NetworkCacheStorage::Data::Data(OSObjectPtr<dispatch_data_t> dispatchData)
{
    const void* data;
    m_dispatchData = adoptOSObject(dispatch_data_create_map(dispatchData.get(), &data, &m_size));
    m_data = static_cast<const uint8_t*>(data);
}

const uint8_t* NetworkCacheStorage::Data::data() const
{
    if (!m_data) {
        const void* data;
        size_t size;
        m_dispatchData = adoptOSObject(dispatch_data_create_map(m_dispatchData.get(), &data, &size));
        ASSERT(size == m_size);
        m_data = static_cast<const uint8_t*>(data);
    }
    return m_data;
}

bool NetworkCacheStorage::Data::isNull() const
{
    return !m_dispatchData;
}

std::unique_ptr<NetworkCacheStorage> NetworkCacheStorage::open(const String& cachePath)
{
    ASSERT(RunLoop::isMain());

    String networkCachePath = WebCore::pathByAppendingComponent(cachePath, networkCacheSubdirectory);
    if (!WebCore::makeAllDirectories(networkCachePath))
        return nullptr;
    return std::unique_ptr<NetworkCacheStorage>(new NetworkCacheStorage(networkCachePath));
}

NetworkCacheStorage::NetworkCacheStorage(const String& directoryPath)
    : m_directoryPath(directoryPath)
    , m_ioQueue(adoptOSObject(dispatch_queue_create("com.apple.WebKit.Cache.Storage", DISPATCH_QUEUE_CONCURRENT)))
    , m_backgroundIOQueue(adoptOSObject(dispatch_queue_create("com.apple.WebKit.Cache.Storage.Background", DISPATCH_QUEUE_CONCURRENT)))
{
    dispatch_set_target_queue(m_backgroundIOQueue.get(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));

    initialize();
}

void NetworkCacheStorage::initialize()
{
    ASSERT(RunLoop::isMain());

    StringCapture cachePathCapture(m_directoryPath);
    auto& keyFilter = m_keyFilter;
    auto& entryCount = m_approximateEntryCount;

    dispatch_async(m_backgroundIOQueue.get(), [cachePathCapture, &keyFilter, &entryCount] {
        String cachePath = cachePathCapture.string();
        traverseCacheFiles(cachePath, [&keyFilter, &entryCount](const String& fileName, const String&) {
            NetworkCacheKey::HashType hash;
            if (!NetworkCacheKey::stringToHash(fileName, hash))
                return;
            keyFilter.add(hash);
            ++entryCount;
        });
    });
}

static String directoryPathForKey(const NetworkCacheKey& key, const String& cachePath)
{
    ASSERT(!key.partition().isEmpty());
    return WebCore::pathByAppendingComponent(cachePath, key.partition());
}

static String filePathForKey(const NetworkCacheKey& key, const String& cachePath)
{
    return WebCore::pathByAppendingComponent(directoryPathForKey(key, cachePath), key.hashAsString());
}

enum class IOChannelType { Read, Write };
static OSObjectPtr<dispatch_io_t> createIOChannelForKey(const NetworkCacheKey& key, IOChannelType type, const String& cachePath, int& fd)
{
    int oflag;
    mode_t mode;

    switch (type) {
    case IOChannelType::Write:
        oflag = O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        WebCore::makeAllDirectories(directoryPathForKey(key, cachePath));
        break;
    case IOChannelType::Read:
        oflag = O_RDONLY | O_NONBLOCK;
        mode = 0;
    }

    CString path = WebCore::fileSystemRepresentation(filePathForKey(key, cachePath));
    fd = ::open(path.data(), oflag, mode);

    LOG(NetworkCacheStorage, "(NetworkProcess) opening %s type=%d", path.data(), type);

    OSObjectPtr<dispatch_io_t> channel = adoptOSObject(dispatch_io_create(DISPATCH_IO_RANDOM, fd, dispatch_get_main_queue(), [fd, type](int error) {
        close(fd);
        if (error)
            LOG(NetworkCacheStorage, "(NetworkProcess) error creating io channel %d", error);
    }));

    ASSERT(channel);
    dispatch_io_set_low_water(channel.get(), std::numeric_limits<size_t>::max());

    return channel;
}

static unsigned hashData(dispatch_data_t data)
{
    if (!data || !dispatch_data_get_size(data))
        return 0;
    StringHasher hasher;
    dispatch_data_apply(data, [&hasher](dispatch_data_t, size_t, const void* data, size_t size) {
        hasher.addCharacters(static_cast<const uint8_t*>(data), size);
        return true;
    });
    return hasher.hash();
}

struct EntryMetaData {
    EntryMetaData() { }
    explicit EntryMetaData(const NetworkCacheKey& key) : cacheStorageVersion(NetworkCacheStorage::version), key(key) { }

    unsigned cacheStorageVersion;
    NetworkCacheKey key;
    std::chrono::milliseconds timeStamp;
    unsigned headerChecksum;
    uint64_t headerOffset;
    uint64_t headerSize;
    unsigned bodyChecksum;
    uint64_t bodyOffset;
    uint64_t bodySize;
};

static bool decodeEntryMetaData(EntryMetaData& metaData, dispatch_data_t fileData)
{
    bool success = false;
    dispatch_data_apply(fileData, [&metaData, &success](dispatch_data_t, size_t, const void* data, size_t size) {
        NetworkCacheDecoder decoder(reinterpret_cast<const uint8_t*>(data), size);
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
        metaData.bodyOffset = round_page(metaData.headerOffset + metaData.headerSize);
        success = true;
        return false;
    });
    return success;
}

static std::unique_ptr<NetworkCacheStorage::Entry> decodeEntry(dispatch_data_t fileData, int fd, const NetworkCacheKey& key)
{
    EntryMetaData metaData;
    if (!decodeEntryMetaData(metaData, fileData))
        return nullptr;

    if (metaData.cacheStorageVersion != NetworkCacheStorage::version)
        return nullptr;
    if (metaData.key != key)
        return nullptr;
    if (metaData.headerOffset + metaData.headerSize > metaData.bodyOffset)
        return nullptr;
    if (metaData.bodyOffset + metaData.bodySize != dispatch_data_get_size(fileData))
        return nullptr;

    auto headerData = adoptOSObject(dispatch_data_create_subrange(fileData, metaData.headerOffset, metaData.headerSize));
    if (metaData.headerChecksum != hashData(headerData.get())) {
        LOG(NetworkCacheStorage, "(NetworkProcess) header checksum mismatch");
        return nullptr;
    }
    size_t mapSize = metaData.bodySize;
    void* map = mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, fd, metaData.bodyOffset);
    if (!map) {
        LOG(NetworkCacheStorage, "(NetworkProcess) map failed");
        return nullptr;
    }

    auto bodyData = adoptOSObject(dispatch_data_create(map, metaData.bodySize, dispatch_get_main_queue(), [map, mapSize] {
        munmap(map, mapSize);
    }));

    if (metaData.bodyChecksum != hashData(bodyData.get())) {
        LOG(NetworkCacheStorage, "(NetworkProcess) data checksum mismatch");
        return nullptr;
    }

    return std::make_unique<NetworkCacheStorage::Entry>(NetworkCacheStorage::Entry {
        metaData.timeStamp,
        NetworkCacheStorage::Data(headerData),
        NetworkCacheStorage::Data(bodyData)
    });
}

static OSObjectPtr<dispatch_data_t> encodeEntryMetaData(const EntryMetaData& entry)
{
    NetworkCacheEncoder encoder;

    encoder << entry.cacheStorageVersion;
    encoder << entry.key;
    encoder << entry.timeStamp;
    encoder << entry.headerChecksum;
    encoder << entry.headerSize;
    encoder << entry.bodyChecksum;
    encoder << entry.bodySize;

    encoder.encodeChecksum();

    return adoptOSObject(dispatch_data_create(encoder.buffer(), encoder.bufferSize(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT));
}

static OSObjectPtr<dispatch_data_t> encodeEntry(const NetworkCacheKey& key, const NetworkCacheStorage::Entry& entry)
{
    EntryMetaData metaData(key);
    metaData.timeStamp = entry.timeStamp;
    metaData.headerChecksum = hashData(entry.header.dispatchData());
    metaData.headerSize = entry.header.size();
    metaData.bodyChecksum = hashData(entry.body.dispatchData());
    metaData.bodySize = entry.body.size();

    auto encodedMetaData = encodeEntryMetaData(metaData);
    auto headerData = adoptOSObject(dispatch_data_create_concat(encodedMetaData.get(), entry.header.dispatchData()));
    if (!entry.body.size())
        return headerData;

    size_t headerSize = dispatch_data_get_size(headerData.get());
    size_t dataOffset = round_page(headerSize);
    Vector<uint8_t, 4096> filler(dataOffset - headerSize, 0);
    OSObjectPtr<dispatch_data_t> alignmentData = adoptOSObject(dispatch_data_create(filler.data(), filler.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT));

    OSObjectPtr<dispatch_data_t> headerWithAlignmentData = adoptOSObject(dispatch_data_create_concat(headerData.get(), alignmentData.get()));

    return adoptOSObject(dispatch_data_create_concat(headerWithAlignmentData.get(), entry.body.dispatchData()));
}

void NetworkCacheStorage::removeEntry(const NetworkCacheKey& key)
{
    ASSERT(RunLoop::isMain());

    if (m_keyFilter.mayContain(key.hash()))
        m_keyFilter.remove(key.hash());

    StringCapture filePathCapture(filePathForKey(key, m_directoryPath));
    dispatch_async(m_ioQueue.get(), [this, filePathCapture] {
        CString path = WebCore::fileSystemRepresentation(filePathCapture.string());
        unlink(path.data());
        if (m_approximateEntryCount)
            --m_approximateEntryCount;
    });
}

void NetworkCacheStorage::dispatchRetrieveOperation(const RetrieveOperation& retrieve)
{
    ASSERT(RunLoop::isMain());

    ++m_activeRetrieveOperationCount;

    StringCapture cachePathCapture(m_directoryPath);
    dispatch_async(m_ioQueue.get(), [this, retrieve, cachePathCapture] {
        int fd;
        auto channel = createIOChannelForKey(retrieve.key, IOChannelType::Read, cachePathCapture.string(), fd);

        dispatch_io_read(channel.get(), 0, std::numeric_limits<size_t>::max(), dispatch_get_main_queue(), [this, fd, retrieve](bool done, dispatch_data_t fileData, int error) {
            if (done) {
                ASSERT(m_activeRetrieveOperationCount);
                --m_activeRetrieveOperationCount;
                dispatchPendingRetrieveOperations();
            }
            if (error) {
                retrieve.completionHandler(nullptr);
                removeEntry(retrieve.key);
                return;
            }
            if (done)
                return;
            auto entry = decodeEntry(fileData, fd, retrieve.key);
            bool success = retrieve.completionHandler(WTF::move(entry));
            if (!success)
                removeEntry(retrieve.key);
        });
    });
}

void NetworkCacheStorage::dispatchPendingRetrieveOperations()
{
    ASSERT(RunLoop::isMain());

    const unsigned maximumActiveRetrieveOperationCount = 5;

    for (int priority = m_pendingRetrieveOperationsByPriority.size() - 1; priority >= 0; --priority) {
        if (m_activeRetrieveOperationCount > maximumActiveRetrieveOperationCount) {
            LOG(NetworkCacheStorage, "(NetworkProcess) limiting parallel retrieves");
            return;
        }
        auto& pendingRetrieveQueue = m_pendingRetrieveOperationsByPriority[priority];
        if (pendingRetrieveQueue.isEmpty())
            continue;
        dispatchRetrieveOperation(pendingRetrieveQueue.takeFirst());
    }
}

void NetworkCacheStorage::retrieve(const NetworkCacheKey& key, unsigned priority, std::function<bool (std::unique_ptr<Entry>)> completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (!m_keyFilter.mayContain(key.hash())) {
        completionHandler(nullptr);
        return;
    }

    if (m_pendingRetrieveOperationsByPriority.size() < priority + 1)
        m_pendingRetrieveOperationsByPriority.grow(priority + 1);
    m_pendingRetrieveOperationsByPriority[priority].append(RetrieveOperation { key, completionHandler });

    dispatchPendingRetrieveOperations();
}

void NetworkCacheStorage::store(const NetworkCacheKey& key, const Entry& entry, std::function<void (bool success)> completionHandler)
{
    ASSERT(RunLoop::isMain());

    StringCapture cachePathCapture(m_directoryPath);
    dispatch_async(m_backgroundIOQueue.get(), [this, key, entry, cachePathCapture, completionHandler] {
        auto data = encodeEntry(key, entry);
        int fd;
        auto channel = createIOChannelForKey(key, IOChannelType::Write, cachePathCapture.string(), fd);
        dispatch_io_write(channel.get(), 0, data.get(), dispatch_get_main_queue(), [this, key, completionHandler](bool done, dispatch_data_t, int error) {
            ASSERT_UNUSED(done, done);
            LOG(NetworkCacheStorage, "(NetworkProcess) write complete error=%d", error);
            if (!error) {
                m_keyFilter.add(key.hash());
                ++m_approximateEntryCount;
                shrinkIfNeeded();
            }
            completionHandler(!error);
        });
    });
}

void NetworkCacheStorage::setMaximumSize(size_t size)
{
    ASSERT(RunLoop::isMain());
    m_maximumSize = size;

    shrinkIfNeeded();
}

void NetworkCacheStorage::clear()
{
    ASSERT(RunLoop::isMain());
    LOG(NetworkCacheStorage, "(NetworkProcess) clearing cache");

    m_keyFilter.clear();
    m_approximateEntryCount = 0;

    StringCapture directoryPathCapture(m_directoryPath);

    dispatch_async(m_ioQueue.get(), [directoryPathCapture] {
        String directoryPath = directoryPathCapture.string();
        traverseDirectory(directoryPath, DT_DIR, [&directoryPath](const String& subdirName) {
            String subdirPath = WebCore::pathByAppendingComponent(directoryPath, subdirName);
            traverseDirectory(subdirPath, DT_REG, [&subdirPath](const String& fileName) {
                String filePath = WebCore::pathByAppendingComponent(subdirPath, fileName);
                unlink(WebCore::fileSystemRepresentation(filePath).data());
            });
            rmdir(WebCore::fileSystemRepresentation(subdirPath).data());
        });
    });
}

void NetworkCacheStorage::shrinkIfNeeded()
{
    const size_t assumedAverageResourceSize { 48 * 1024 };
    const size_t everyNthResourceToDelete { 4 };

    size_t estimatedCacheSize = assumedAverageResourceSize * m_approximateEntryCount;

    if (estimatedCacheSize <= m_maximumSize)
        return;
    if (m_shrinkInProgress)
        return;
    m_shrinkInProgress = true;

    LOG(NetworkCacheStorage, "(NetworkProcess) shrinking cache m_approximateEntryCount=%d estimatedCacheSize=%d, m_maximumSize=%d", static_cast<size_t>(m_approximateEntryCount), estimatedCacheSize, m_maximumSize);

    StringCapture cachePathCapture(m_directoryPath);
    dispatch_async(m_backgroundIOQueue.get(), [this, cachePathCapture] {
        String cachePath = cachePathCapture.string();
        size_t foundEntryCount = 0;
        size_t deletedCount = 0;
        traverseCacheFiles(cachePath, [this, &foundEntryCount, &deletedCount](const String& fileName, const String& directory) {
            String partitionPath = WebCore::pathByAppendingComponent(directory, fileName);
            CString path = WebCore::fileSystemRepresentation(partitionPath);
            ++foundEntryCount;
            if (foundEntryCount % everyNthResourceToDelete)
                return;
            ++deletedCount;

            unlink(path.data());

            NetworkCacheKey::HashType hash;
            if (!NetworkCacheKey::stringToHash(fileName, hash))
                return;
            dispatch_async(dispatch_get_main_queue(), [this, hash] {
                if (m_keyFilter.mayContain(hash))
                    m_keyFilter.remove(hash);
            });
        });
        m_approximateEntryCount = foundEntryCount - deletedCount;
        m_shrinkInProgress = false;

        LOG(NetworkCacheStorage, "(NetworkProcess) cache shrink completed m_approximateEntryCount=%d", static_cast<size_t>(m_approximateEntryCount));
    });
}

}

#endif
