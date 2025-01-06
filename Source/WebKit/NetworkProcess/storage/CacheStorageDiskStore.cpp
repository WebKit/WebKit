/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "CacheStorageDiskStore.h"

#include "CacheStorageRecord.h"
#include "Logging.h"
#include "NetworkCacheCoders.h"
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/PageBlock.h>
#include <wtf/RefCounted.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

static constexpr auto saltFileName = "salt"_s;
static constexpr auto versionDirectoryPrefix = "Version "_s;
static constexpr auto recordsDirectoryName = "Records"_s;
static constexpr auto blobsDirectoryName = "Blobs"_s;
static constexpr auto blobSuffix = "-blob"_s;
static const unsigned currentVersion = 16;

static bool shouldStoreBodyAsBlob(const Vector<uint8_t>& bodyData)
{
    return bodyData.size() > WTF::pageSize();
}

static SHA1::Digest computeSHA1(std::span<const uint8_t> span, FileSystem::Salt salt)
{
    SHA1 sha1;
    sha1.addBytes(salt);
    sha1.addBytes(span);
    SHA1::Digest digest;
    sha1.computeHash(digest);
    return digest;
}

static String recordFilePathWithDirectory(const String& directory, const NetworkCache::Key& key)
{
    return FileSystem::pathByAppendingComponents(directory, { key.partitionHashAsString(), key.type(), key.hashAsString() });
}

struct RecordMetaData {
    RecordMetaData() { }
    explicit RecordMetaData(const NetworkCache::Key& key)
        : cacheStorageVersion(currentVersion)
        , key(key)
    { }

    unsigned cacheStorageVersion;
    NetworkCache::Key key;
    WallTime timeStamp;
    SHA1::Digest headerHash;
    uint64_t headerSize { 0 };
    SHA1::Digest bodyHash;
    uint64_t bodySize { 0 };
    bool isBodyInline { false };
    uint64_t headerOffset { 0 };
};

struct RecordHeader {
    double insertionTime { 0 };
    uint64_t size { 0 };
    WebCore::FetchHeaders::Guard requestHeadersGuard { WebCore::FetchHeaders::Guard::None };
    WebCore::ResourceRequest request;
    WebCore::FetchOptions options;
    String referrer;
    WebCore::FetchHeaders::Guard responseHeadersGuard { WebCore::FetchHeaders::Guard::None };
    WebCore::ResourceResponse::CrossThreadData responseData;
    uint64_t responseBodySize { 0 };
};

struct StoredRecordInformation {
    CacheStorageRecordInformation info;
    RecordMetaData metaData;
    RecordHeader header;
};

Ref<CacheStorageDiskStore> CacheStorageDiskStore::create(const String& cacheName, const String& path, Ref<WorkQueue>&& queue)
{
    return adoptRef(*new CacheStorageDiskStore(cacheName, path, WTFMove(queue)));
}

CacheStorageDiskStore::CacheStorageDiskStore(const String& cacheName, const String& path, Ref<WorkQueue>&& queue)
    : m_cacheName(cacheName)
    , m_path(path)
    , m_salt(valueOrDefault(FileSystem::readOrMakeSalt(saltFilePath())))
    , m_callbackQueue(WTFMove(queue))
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.CacheStorageCache"_s))
{
    ASSERT(!m_cacheName.isEmpty());
    ASSERT(!m_path.isEmpty());
}

String CacheStorageDiskStore::versionDirectoryPath() const
{
    return FileSystem::pathByAppendingComponent(m_path, makeString(versionDirectoryPrefix, currentVersion));
}

String CacheStorageDiskStore::saltFilePath() const
{
    return FileSystem::pathByAppendingComponent(versionDirectoryPath(), saltFileName);
}

String CacheStorageDiskStore::recordsDirectoryPath() const
{
    return FileSystem::pathByAppendingComponent(versionDirectoryPath(), recordsDirectoryName);
}

String CacheStorageDiskStore::recordFilePath(const NetworkCache::Key& key) const
{
    return recordFilePathWithDirectory(recordsDirectoryPath(), key);
}

String CacheStorageDiskStore::recordBlobFilePath(const String& recordPath) const
{
    return makeString(recordPath, blobSuffix);
}

String CacheStorageDiskStore::blobsDirectoryPath() const
{
    return FileSystem::pathByAppendingComponent(versionDirectoryPath(), blobsDirectoryName);
}

String CacheStorageDiskStore::blobFilePath(const String& blobFileName) const
{
    return FileSystem::pathByAppendingComponent(blobsDirectoryPath(), blobFileName);
}

static std::optional<RecordMetaData> decodeRecordMetaData(std::span<const uint8_t> fileData)
{
    WTF::Persistence::Decoder decoder(fileData);
    RecordMetaData metaData;
    std::optional<unsigned> cacheStorageVersion;
    decoder >> cacheStorageVersion;
    if (!cacheStorageVersion)
        return std::nullopt;
    metaData.cacheStorageVersion = WTFMove(*cacheStorageVersion);

    std::optional<NetworkCache::Key> key;
    decoder >> key;
    if (!key)
        return std::nullopt;
    metaData.key = WTFMove(*key);

    std::optional<WallTime> timeStamp;
    decoder >> timeStamp;
    if (!timeStamp)
        return std::nullopt;
    metaData.timeStamp = WTFMove(*timeStamp);

    std::optional<SHA1::Digest> headerHash;
    decoder >> headerHash;
    if (!headerHash)
        return std::nullopt;
    metaData.headerHash = WTFMove(*headerHash);

    std::optional<uint64_t> headerSize;
    decoder >> headerSize;
    if (!headerSize)
        return std::nullopt;
    metaData.headerSize = WTFMove(*headerSize);

    std::optional<SHA1::Digest> bodyHash;
    decoder >> bodyHash;
    if (!bodyHash)
        return std::nullopt;
    metaData.bodyHash = WTFMove(*bodyHash);

    std::optional<uint64_t> bodySize;
    decoder >> bodySize;
    if (!bodySize)
        return std::nullopt;
    metaData.bodySize = WTFMove(*bodySize);

    std::optional<bool> isBodyInline;
    decoder >> isBodyInline;
    if (!isBodyInline)
        return std::nullopt;
    metaData.isBodyInline = WTFMove(*isBodyInline);

    if (!decoder.verifyChecksum())
        return std::nullopt;

    metaData.headerOffset = decoder.currentOffset();
    return metaData;
}

static std::optional<RecordHeader> decodeRecordHeader(std::span<const uint8_t> headerData)
{
    WTF::Persistence::Decoder decoder(headerData);
    std::optional<double> insertionTime;
    decoder >> insertionTime;
    if (!insertionTime)
        return std::nullopt;

    std::optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<WebCore::FetchHeaders::Guard> requestHeadersGuard;
    decoder >> requestHeadersGuard;
    if (!requestHeadersGuard)
        return std::nullopt;

    std::optional<WebCore::ResourceRequest> request;
    decoder >> request;
    if (!request)
        return std::nullopt;

    WebCore::FetchOptions options;
    if (!WebCore::FetchOptions::decodePersistent(decoder, options))
        return std::nullopt;

    std::optional<String> referrer;
    decoder >> referrer;
    if (!referrer)
        return std::nullopt;

    std::optional<WebCore::FetchHeaders::Guard> responseHeadersGuard;
    decoder >> responseHeadersGuard;
    if (!responseHeadersGuard)
        return std::nullopt;

    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull || *isNull)
        return std::nullopt;

    std::optional<WebCore::ResourceResponse::CrossThreadData> responseData;
    decoder >> responseData;
    if (!responseData)
        return std::nullopt;

    std::optional<uint64_t> responseBodySize;
    decoder >> responseBodySize;
    if (!responseBodySize)
        return std::nullopt;

    if (!decoder.verifyChecksum())
        return std::nullopt;

    return RecordHeader {
        *insertionTime,
        *size,
        *requestHeadersGuard,
        WTFMove(*request),
        WTFMove(options),
        WTFMove(*referrer),
        *responseHeadersGuard,
        WTFMove(*responseData),
        WTFMove(*responseBodySize)
    };
}

static std::optional<StoredRecordInformation> readRecordInfoFromFileData(const FileSystem::Salt& salt, std::span<const uint8_t> fileData)
{
    if (!fileData.size())
        return std::nullopt;

    auto metaData = decodeRecordMetaData(fileData);
    if (!metaData)
        return std::nullopt;

    if (metaData->cacheStorageVersion != currentVersion || metaData->timeStamp > WallTime::now())
        return std::nullopt;

    auto headerData = fileData.subspan(metaData->headerOffset, metaData->headerSize);
    if (metaData->headerHash != computeSHA1(headerData, salt))
        return std::nullopt;

    auto header = decodeRecordHeader(headerData);
    if (!header)
        return std::nullopt;

    auto key = metaData->key;
    auto url = header->request.url();
    CacheStorageRecordInformation info { WTFMove(key), header->insertionTime, 0, 0, header->responseBodySize, WTFMove(url), false, { } };
    info.updateVaryHeaders(header->request, header->responseData);
    return StoredRecordInformation { info, WTFMove(*metaData), WTFMove(*header) };
}

std::optional<CacheStorageRecord> CacheStorageDiskStore::readRecordFromFileData(std::span<const uint8_t> buffer, FileSystem::MappedFileData&& blobBuffer)
{
    auto storedInfo = readRecordInfoFromFileData(m_salt, buffer);
    if (!storedInfo)
        return std::nullopt;

    std::optional<WebCore::DOMCacheEngine::ResponseBody> responseBody;
    if (storedInfo->metaData.isBodyInline) {
        size_t bodyOffset = storedInfo->metaData.headerOffset + storedInfo->metaData.headerSize;
        size_t bodySize = storedInfo->metaData.bodySize;
        if (bodyOffset + bodySize != buffer.size())
            return std::nullopt;

        auto bodyData = buffer.subspan(bodyOffset, bodySize);
        if (storedInfo->metaData.bodyHash != computeSHA1(bodyData, m_salt))
            return std::nullopt;

        // FIXME: avoid copying inline body data here, perhaps by adding offset support to
        // MappedFileData, or by taking a read-only virtual copy of bodyData.
        responseBody = WebCore::SharedBuffer::create(bodyData);
    } else {
        if (!blobBuffer)
            return std::nullopt;

        auto sharedBuffer = WebCore::SharedBuffer::create(WTFMove(blobBuffer));
        if (storedInfo->metaData.bodyHash != computeSHA1(sharedBuffer->span(), m_salt))
            return std::nullopt;

        responseBody = sharedBuffer;
    }

    if (!responseBody)
        return std::nullopt;

    return CacheStorageRecord { storedInfo->info, storedInfo->header.requestHeadersGuard, storedInfo->header.request, storedInfo->header.options, storedInfo->header.referrer, storedInfo->header.responseHeadersGuard, WTFMove(storedInfo->header.responseData), storedInfo->header.responseBodySize, WTFMove(*responseBody) };
}

void CacheStorageDiskStore::readAllRecordInfosInternal(ReadAllRecordInfosCallback&& callback)
{
    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordsDirectory = recordsDirectoryPath().isolatedCopy(), cacheName = m_cacheName.isolatedCopy(), salt = m_salt, callback = WTFMove(callback)]() mutable {
        Vector<CacheStorageRecordInformation> recordInfos;
        auto partitionNames = FileSystem::listDirectory(recordsDirectory);
        for (auto& partitionName : partitionNames) {
            auto partitionDirectoryPath = FileSystem::pathByAppendingComponent(recordsDirectory, partitionName);
            auto cacheDirectory = FileSystem::pathByAppendingComponent(partitionDirectoryPath, m_cacheName);
            for (auto& recordName : FileSystem::listDirectory(cacheDirectory)) {
                if (recordName.endsWith(blobSuffix))
                    continue;

                auto recordFile = FileSystem::pathByAppendingComponent(cacheDirectory, recordName);
                auto fileData = FileSystem::MappedFileData::create(recordFile, FileSystem::MappedFileMode::Private);
                if (!fileData)
                    continue;

                if (auto storedRecordInfo = readRecordInfoFromFileData(salt, fileData->span()))
                    recordInfos.append(WTFMove(storedRecordInfo->info));
            }
        }
        m_callbackQueue->dispatch([protectedThis = WTFMove(protectedThis), recordInfos = crossThreadCopy(WTFMove(recordInfos)), callback = WTFMove(callback)]() mutable {
            callback(WTFMove(recordInfos));
        });
    });
}

void CacheStorageDiskStore::readAllRecordInfos(ReadAllRecordInfosCallback&& callback)
{
    readAllRecordInfosInternal(WTFMove(callback));
}

void CacheStorageDiskStore::readRecordsInternal(const Vector<CacheStorageRecordInformation>& recordInfos, ReadRecordsCallback&& callback)
{
    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, directory = crossThreadCopy(recordsDirectoryPath()), recordInfos = crossThreadCopy(recordInfos), callback = WTFMove(callback)]() mutable {
        Vector<std::optional<CacheStorageRecord>> records;
        for (auto& recordInfo : recordInfos) {
            auto recordFile = recordFilePathWithDirectory(directory, recordInfo.key());
            auto fileData = valueOrDefault(FileSystem::MappedFileData::create(recordFile, FileSystem::MappedFileMode::Private));
            auto blobData = !fileData.size() ? FileSystem::MappedFileData { } : valueOrDefault(FileSystem::MappedFileData::create(recordBlobFilePath(recordFile), FileSystem::MappedFileMode::Private));
            auto record = readRecordFromFileData(fileData.span(), WTFMove(blobData));
            if (!record) {
                RELEASE_LOG(CacheStorage, "%p - CacheStorageDiskStore::readRecordsInternal fails to decode record from file", this);
                records.append(std::nullopt);
                continue;
            }

            if (recordInfo.insertionTime() != record->info.insertionTime() || recordInfo.size() != record->info.size() || recordInfo.url() != record->info.url()) {
                records.append(std::nullopt);
                continue;
            }

            if (recordFilePath(recordInfo.key()) != recordFilePath(record->info.key())) {
                records.append(std::nullopt);
                continue;
            }

            record->info.setIdentifier(recordInfo.identifier());
            record->info.setUpdateResponseCounter(recordInfo.updateResponseCounter());
            records.append(WTFMove(record));
        }

        m_callbackQueue->dispatch([protectedThis = WTFMove(protectedThis), records = crossThreadCopy(WTFMove(records)), callback = WTFMove(callback)]() mutable {
            callback(WTFMove(records));
        });
    });
}

void CacheStorageDiskStore::readRecords(const Vector<CacheStorageRecordInformation>& recordInfos, ReadRecordsCallback&& callback)
{
    readRecordsInternal(recordInfos, WTFMove(callback));
}

void CacheStorageDiskStore::deleteRecords(const Vector<CacheStorageRecordInformation>& recordInfos, WriteRecordsCallback&& callback)
{
    auto recordFiles = WTF::map(recordInfos, [&](auto recordInfo) {
        return recordFilePath(recordInfo.key());
    });

    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordFiles = crossThreadCopy(WTFMove(recordFiles)), callback = WTFMove(callback)]() mutable {
        bool result = true;
        for (auto recordFile : recordFiles) {
            FileSystem::deleteFile(recordBlobFilePath(recordFile));
            FileSystem::deleteFile(recordFile);
            // FIXME: we should probably stop writing and revert changes when result becomes false.
            if (FileSystem::fileExists(recordFile))
                result = false;
        }

        m_callbackQueue->dispatch([protectedThis = WTFMove(protectedThis), result, callback = WTFMove(callback)]() mutable {
            callback(WTFMove(result));
        });
    });
}

static Vector<uint8_t> encodeRecordHeader(CacheStorageRecord&& record)
{
    WTF::Persistence::Encoder encoder;
    encoder << record.info.insertionTime();
    encoder << record.info.size();
    encoder << record.requestHeadersGuard;
    encoder << record.request;
    record.options.encodePersistent(encoder);
    encoder << record.referrer;
    encoder << record.responseHeadersGuard;
    // isNull is needed as we switched from encoding ResourceResponse to encoding ResourceResponse::CrossThreadData,
    // and we don't want to change storage format on disk.
    bool isNull = false;
    encoder << isNull;
    encoder << record.responseData;
    encoder << record.responseBodySize;
    encoder.encodeChecksum();

    return { encoder.span() };
}

static Vector<uint8_t> encodeRecordBody(const WebCore::DOMCacheEngine::ResponseBody& body)
{
    return WTF::switchOn(body, [](const Ref<WebCore::FormData>& formData) {
        // FIXME: Store form data body.
        return Vector<uint8_t> { };
    }, [&](const Ref<WebCore::SharedBuffer>& buffer) {
        return Vector<uint8_t> { buffer->span() };
    }, [](const std::nullptr_t&) {
        return Vector<uint8_t> { };
    });
}

size_t CacheStorageDiskStore::computeRealBodySizeForStorage(const WebCore::DOMCacheEngine::ResponseBody& body)
{
    return encodeRecordBody(body).size();
}

static Vector<uint8_t> encodeRecord(const NetworkCache::Key& key, const Vector<uint8_t>& headerData, bool isBodyInline, const Vector<uint8_t>& bodyData, const SHA1::Digest& bodyHash, FileSystem::Salt salt)
{
    WTF::Persistence::Encoder encoder;
    encoder << currentVersion;
    encoder << key;
    encoder << WallTime { };
    encoder << computeSHA1(headerData.span(), salt);
    encoder << (uint64_t)headerData.size();
    encoder << bodyHash;
    encoder << (uint64_t)bodyData.size();
    encoder << isBodyInline;
    encoder.encodeChecksum();

    auto metaData = encoder.span();
    Vector<uint8_t> result;
    result.append(metaData);
    result.appendVector(headerData);

    if (isBodyInline)
        result.appendVector(bodyData);

    return result;
}

void CacheStorageDiskStore::writeRecords(Vector<CacheStorageRecord>&& records, WriteRecordsCallback&& callback)
{
    Vector<String> recordFiles;
    Vector<Vector<uint8_t>> recordDatas;
    Vector<Vector<uint8_t>> recordBlobDatas;
    for (auto&& record : records) {
        recordFiles.append(recordFilePath(record.info.key()));
        auto bodyData = encodeRecordBody(record.responseBody);
        auto bodyHash = computeSHA1(bodyData.span(), m_salt);
        bool shouldCreateBlob = shouldStoreBodyAsBlob(bodyData);
        auto recordInfoKey = record.info.key();
        auto headerData = encodeRecordHeader(WTFMove(record));
        auto recordData = encodeRecord(recordInfoKey, headerData, !shouldCreateBlob, bodyData, bodyHash, m_salt);
        recordDatas.append(WTFMove(recordData));
        if (!shouldCreateBlob)
            bodyData = { };
        recordBlobDatas.append(WTFMove(bodyData));
    }

    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordFiles = crossThreadCopy(WTFMove(recordFiles)), recordDatas = crossThreadCopy(WTFMove(recordDatas)), recordBlobDatas = crossThreadCopy(WTFMove(recordBlobDatas)), callback = WTFMove(callback)]() mutable {
        bool result = true;
        // FIXME: we should probably stop writing and revert changes when result becomes false.
        for (size_t index = 0; index < recordFiles.size(); ++index) {
            auto recordFile = recordFiles[index];
            auto recordData = recordDatas[index];
            auto recordBlobData = recordBlobDatas[index];
            FileSystem::makeAllDirectories(FileSystem::parentPath(recordFile));
            if (!recordBlobData.isEmpty())  {
                if (FileSystem::overwriteEntireFile(recordBlobFilePath(recordFile), recordBlobData) == -1) {
                    result = false;
                    continue;
                }
            }
            if (FileSystem::overwriteEntireFile(recordFile, recordData) == -1)
                result = false;
        }

        m_callbackQueue->dispatch([protectedThis = WTFMove(protectedThis), result, callback = WTFMove(callback)]() mutable {
            callback(WTFMove(result));
        });
    });
}

} // namespace WebKit
