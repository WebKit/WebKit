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
#include <wtf/PageBlock.h>
#include <wtf/RefCounted.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

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

static SHA1::Digest computeSHA1(Span<const uint8_t> span, FileSystem::Salt salt)
{
    SHA1 sha1;
    sha1.addBytes(salt.data(), salt.size());
    sha1.addBytes(span.data(), span.size());
    SHA1::Digest digest;
    sha1.computeHash(digest);
    return digest;
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
    WebCore::ResourceResponse response;
    uint64_t responseBodySize { 0 };
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
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.CacheStorageCache"))
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
    return FileSystem::pathByAppendingComponents(recordsDirectoryPath(), { key.partitionHashAsString(), key.type(), key.hashAsString() });
}

String CacheStorageDiskStore::recordBlobFilePath(const String& recordPath) const
{
    return recordPath + blobSuffix;
}

String CacheStorageDiskStore::blobsDirectoryPath() const
{
    return FileSystem::pathByAppendingComponent(versionDirectoryPath(), blobsDirectoryName);
}

String CacheStorageDiskStore::blobFilePath(const String& blobFileName) const
{
    return FileSystem::pathByAppendingComponent(blobsDirectoryPath(), blobFileName);
}

static std::optional<RecordMetaData> decodeRecordMetaData(Span<const uint8_t> fileData)
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

static std::optional<RecordHeader> decodeRecordHeader(Span<const uint8_t> headerData)
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

    WebCore::ResourceResponse response;
    if (!WebCore::ResourceResponse::decode(decoder, response))
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
        *request,
        WTFMove(options),
        WTFMove(*referrer),
        *responseHeadersGuard,
        WTFMove(response),
        WTFMove(*responseBodySize)
    };
}

std::optional<CacheStorageRecord> CacheStorageDiskStore::readRecordFromFileData(const Vector<uint8_t>& buffer, const Vector<uint8_t>& blobBuffer)
{
    if (buffer.isEmpty())
        return std::nullopt;

    auto fileData = Span { buffer.data(), buffer.size() };
    auto metaData = decodeRecordMetaData(fileData);
    if (!metaData)
        return std::nullopt;

    if (metaData->cacheStorageVersion != currentVersion || metaData->timeStamp > WallTime::now())
        return std::nullopt;

    auto headerData = fileData.subspan(metaData->headerOffset, metaData->headerSize);
    if (metaData->headerHash != computeSHA1(headerData, m_salt))
        return std::nullopt;

    auto header = decodeRecordHeader(headerData);
    if (!header)
        return std::nullopt;

    std::optional<WebCore::DOMCacheEngine::ResponseBody> responseBody;
    if (metaData->isBodyInline) {
        size_t bodyOffset = metaData->headerOffset + metaData->headerSize;
        if (bodyOffset + metaData->bodySize != fileData.size())
            return std::nullopt;

        auto bodyData = fileData.subspan(bodyOffset);
        if (metaData->bodyHash != computeSHA1(bodyData, m_salt))
            return std::nullopt;

        responseBody = WebCore::SharedBuffer::create(bodyData.data(), bodyData.size());
    } else {
        if (blobBuffer.isEmpty())
            return std::nullopt;

        auto sharedBuffer = WebCore::SharedBuffer::create(blobBuffer.data(), blobBuffer.size());
        auto bodyData = Span { sharedBuffer->data(), sharedBuffer->size() };
        if (metaData->bodyHash != computeSHA1(bodyData, m_salt))
            return std::nullopt;

        responseBody = sharedBuffer;
    }

    if (!responseBody)
        return std::nullopt;

    CacheStorageRecordInformation info { metaData->key, header->insertionTime, 0, 0, header->responseBodySize, header->request.url(), false, { } };
    info.updateVaryHeaders(header->request, header->response.httpHeaderField(WebCore::HTTPHeaderName::Vary));
    return CacheStorageRecord { info, header->requestHeadersGuard, header->request, header->options, header->referrer, header->responseHeadersGuard, header->response.crossThreadData(), header->responseBodySize, WTFMove(*responseBody) };
}

void CacheStorageDiskStore::readAllRecords(ReadAllRecordsCallback&& callback)
{
    auto didReadRecordFiles = [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](auto fileDatas, auto blobDatas) mutable {
        ASSERT(fileDatas.size() == blobDatas.size());
        Vector<CacheStorageRecord> result;
        for (size_t index = 0; index < fileDatas.size(); ++index) {
            if (auto record = readRecordFromFileData(fileDatas[index], blobDatas[index]))
                result.append(WTFMove(*record));
            else
                RELEASE_LOG(CacheStorage, "%p - CacheStorageDiskStore::readAllRecords fails to decode record from file", this);
        }
        callback(WTFMove(result));
    };

    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordsDirectory = recordsDirectoryPath().isolatedCopy(), cacheName = m_cacheName.isolatedCopy(), didReadRecordFiles = WTFMove(didReadRecordFiles)]() mutable {
        Vector<Vector<uint8_t>> fileDatas;
        Vector<Vector<uint8_t>> blobDatas;
        auto partitionNames = FileSystem::listDirectory(recordsDirectory);
        for (auto& partitionName : partitionNames) {
            auto partitionDirectoryPath = FileSystem::pathByAppendingComponent(recordsDirectory, partitionName);
            auto cacheDirectory = FileSystem::pathByAppendingComponent(partitionDirectoryPath, m_cacheName);
            for (auto& recordName : FileSystem::listDirectory(cacheDirectory)) {
                if (recordName.endsWith(blobSuffix))
                    continue;

                auto recordFile = FileSystem::pathByAppendingComponent(cacheDirectory, recordName);
                auto fileData = FileSystem::readEntireFile(recordFile);
                if (fileData) {
                    fileDatas.append(WTFMove(*fileData));
                    auto recordBlobFile = recordBlobFilePath(recordFile);
                    blobDatas.append(valueOrDefault(FileSystem::readEntireFile(recordBlobFile)));
                }
            }
        }

        m_callbackQueue->dispatch([fileDatas = crossThreadCopy(WTFMove(fileDatas)), blobDatas = crossThreadCopy(WTFMove(blobDatas)), didReadRecordFiles = WTFMove(didReadRecordFiles)]() mutable {
            didReadRecordFiles(WTFMove(fileDatas), WTFMove(blobDatas));
        });
    });
}

void CacheStorageDiskStore::readRecords(const Vector<CacheStorageRecordInformation>& recordInfos, ReadRecordsCallback&& callback)
{
    auto recordFiles = WTF::map(recordInfos, [this](const auto& recordInfo) {
        return recordFilePath(recordInfo.key);
    });

    auto didReadRecordFiles = [this, protectedThis = Ref { *this }, recordInfos, callback = WTFMove(callback)](auto fileDatas, auto blobDatas) mutable {
        ASSERT(recordInfos.size() == fileDatas.size());
        ASSERT(recordInfos.size() == blobDatas.size());

        Vector<std::optional<CacheStorageRecord>> result;
        for (size_t index = 0; index < recordInfos.size(); ++index) {
            auto record = readRecordFromFileData(fileDatas[index], blobDatas[index]);
            if (record) {
                auto recordInfo = recordInfos[index];
                if (recordInfo.insertionTime != record->info.insertionTime || recordInfo.size != record->info.size || recordInfo.url != record->info.url)
                    record = std::nullopt;
                else if (recordFilePath(recordInfo.key) != recordFilePath(record->info.key))
                    record = std::nullopt;
                else {
                    record->info.identifier = recordInfo.identifier;
                    record->info.updateResponseCounter = recordInfo.updateResponseCounter;
                }
            } else
                RELEASE_LOG(CacheStorage, "%p - CacheStorageDiskStore::readRecords fails to decode record from file", this);

            result.append(WTFMove(record));
        }
        callback(WTFMove(result));
    };

    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordFiles = crossThreadCopy(WTFMove(recordFiles)), didReadRecordFiles = WTFMove(didReadRecordFiles)]() mutable {
        Vector<Vector<uint8_t>> fileDatas;
        Vector<Vector<uint8_t>> blobDatas;
        for (auto& recordFile : recordFiles) {
            auto fileData = valueOrDefault(FileSystem::readEntireFile(recordFile));
            auto blobData = fileData.isEmpty()? Vector<uint8_t> { } : valueOrDefault(FileSystem::readEntireFile(recordBlobFilePath(recordFile)));
            fileDatas.append(WTFMove(fileData));
            blobDatas.append(WTFMove(blobData));
        }

        m_callbackQueue->dispatch([fileDatas = crossThreadCopy(WTFMove(fileDatas)), blobDatas = crossThreadCopy(WTFMove(blobDatas)), didReadRecordFiles = WTFMove(didReadRecordFiles)]() mutable {
            didReadRecordFiles(WTFMove(fileDatas), WTFMove(blobDatas));
        });
    });
}

void CacheStorageDiskStore::deleteRecords(const Vector<CacheStorageRecordInformation>& recordInfos, WriteRecordsCallback&& callback)
{
    auto recordFiles = WTF::map(recordInfos, [&](auto recordInfo) {
        return recordFilePath(recordInfo.key);
    });

    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordFiles = WTFMove(recordFiles), callback = WTFMove(callback)]() mutable {
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
    encoder << record.info.insertionTime;
    encoder << record.info.size;
    encoder << record.requestHeadersGuard;
    encoder << record.request;
    record.options.encodePersistent(encoder);
    encoder << record.referrer;
    encoder << record.responseHeadersGuard;
    encoder << WebCore::ResourceResponse::fromCrossThreadData(WTFMove(record.responseData));
    encoder << record.responseBodySize;
    encoder.encodeChecksum();

    return { encoder.buffer(), encoder.bufferSize() };
}

static Vector<uint8_t> encodeRecordBody(const CacheStorageRecord& record)
{
    return WTF::switchOn(record.responseBody, [](const Ref<WebCore::FormData>& formData) {
        // FIXME: Store form data body.
        return Vector<uint8_t> { };
    }, [&](const Ref<WebCore::SharedBuffer>& buffer) {
        return Vector<uint8_t> { buffer->data(), buffer->size() };
    }, [](const std::nullptr_t&) {
        return Vector<uint8_t> { };
    });
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

    auto metaData = Vector<uint8_t> { encoder.buffer(), encoder.bufferSize() };
    Vector<uint8_t> result;
    result.appendVector(metaData);
    result.appendVector(headerData);

    StringBuilder sb;
    for (auto& data : bodyData) {
        sb.append(data);
        sb.append(",");
    }
    
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
        recordFiles.append(recordFilePath(record.info.key));
        auto bodyData = encodeRecordBody(record);
        auto bodyHash = computeSHA1(bodyData.span(), m_salt);
        bool shouldCreateBlob = shouldStoreBodyAsBlob(bodyData);
        auto headerData = encodeRecordHeader(WTFMove(record));
        auto recordData = encodeRecord(record.info.key, headerData, !shouldCreateBlob, bodyData, bodyHash, m_salt);
        recordDatas.append(WTFMove(recordData));
        if (!shouldCreateBlob)
            bodyData = { };
        recordBlobDatas.append(WTFMove(bodyData));
    }

    m_ioQueue->dispatch([this, protectedThis = Ref { *this }, recordFiles = WTFMove(recordFiles), recordDatas = WTFMove(recordDatas), recordBlobDatas = WTFMove(recordBlobDatas), callback = WTFMove(callback)]() mutable {
        bool result = true;
        // FIXME: we should probably stop writing and revert changes when result becomes false.
        for (size_t index = 0; index < recordFiles.size(); ++index) {
            auto recordFile = recordFiles[index];
            auto recordData = recordDatas[index];
            auto recordBlobData = recordBlobDatas[index];
            FileSystem::makeAllDirectories(FileSystem::parentPath(recordFile));
            if (!recordBlobData.isEmpty())  {
                if (FileSystem::overwriteEntireFile(recordBlobFilePath(recordFile), Span { recordBlobData.data(), recordBlobData.size() }) == -1) {
                    result = false;
                    continue;
                }
            }
            if (FileSystem::overwriteEntireFile(recordFile, Span { recordData.data(), recordData.size() }) == -1)
                result = false;
        }

        m_callbackQueue->dispatch([protectedThis = WTFMove(protectedThis), result, callback = WTFMove(callback)]() mutable {
            callback(WTFMove(result));
        });
    });
}

} // namespace WebKit

