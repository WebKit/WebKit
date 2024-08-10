/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BackgroundFetchStoreManager.h"

#include "Logging.h"
#include <WebCore/ResourceError.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/PageBlock.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace WebCore;

static constexpr auto fetchSuffix = "-backgroundfetch"_s;

static bool shouldUseFileMapping(uint64_t fileSize)
{
    return fileSize >= pageSize();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(BackgroundFetchStoreManager);

String BackgroundFetchStoreManager::createNewStorageIdentifier()
{
    return makeString(createVersion4UUIDString(), fetchSuffix);
}

BackgroundFetchStoreManager::BackgroundFetchStoreManager(const String& path, Ref<WorkQueue>&& taskQueue, QuotaCheckFunction&& quotaCheckFunction)
    : m_path(path)
    , m_taskQueue(WTFMove(taskQueue))
    , m_ioQueue(WorkQueue::create("com.apple.WebKit.BackgroundFetchStoreManager"_s))
    , m_quotaCheckFunction(WTFMove(quotaCheckFunction))
{
    m_ioQueue->dispatch([directoryPath = m_path.isolatedCopy()]() mutable {
        FileSystem::makeAllDirectories(directoryPath);
    });
}

BackgroundFetchStoreManager::~BackgroundFetchStoreManager()
{
}

void BackgroundFetchStoreManager::initializeFetches(InitializeCallback&& callback)
{
    assertIsCurrent(m_taskQueue.get());

    if (m_path.isEmpty()) {
        callback({ });
        return;
    }

    m_ioQueue->dispatch([queue = Ref { m_taskQueue }, directoryPath = m_path.isolatedCopy(), callback = WTFMove(callback)]() mutable {
        Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>> fetches;
        for (auto& fileName : FileSystem::listDirectory(directoryPath)) {
            if (!fileName.endsWith(fetchSuffix))
                continue;

            auto fetchPath = FileSystem::pathByAppendingComponent(directoryPath, fileName);
            auto fileSize = FileSystem::fileSize(fetchPath);
            if (!fileSize)
                continue;

            auto fetchData = SharedBuffer::createWithContentsOfFile(fetchPath, FileSystem::MappedFileMode::Private, shouldUseFileMapping(*fileSize) ? SharedBuffer::MayUseFileMapping::Yes : SharedBuffer::MayUseFileMapping::No);
            fetches.append(std::pair(WTFMove(fetchData), WTFMove(fileName)));
        }

        queue->dispatch([callback = WTFMove(callback), fetches = WTFMove(fetches)]() mutable {
            callback(WTFMove(fetches));
        });
    });
}

void BackgroundFetchStoreManager::clearFetch(const String& identifier, CompletionHandler<void()>&& callback)
{
    assertIsCurrent(m_taskQueue.get());

    if (m_path.isEmpty()) {
        m_nonPersistentChunks.remove(identifier);
        callback();
        return;
    }

    auto filePath = FileSystem::pathByAppendingComponents(m_path, { identifier });
    m_ioQueue->dispatch([queue = Ref { m_taskQueue }, directoryPath = m_path.isolatedCopy(), identifier = identifier.isolatedCopy(), callback = WTFMove(callback)]() mutable {
        for (auto& fileName : FileSystem::listDirectory(directoryPath)) {
            if (fileName.startsWith(identifier))
                FileSystem::deleteFile(FileSystem::pathByAppendingComponent(directoryPath, fileName));
        }
        queue->dispatch(WTFMove(callback));
    });
}

void BackgroundFetchStoreManager::clearAllFetches(const Vector<String>& identifiers, CompletionHandler<void()>&& callback)
{
    assertIsCurrent(m_taskQueue.get());

    if (m_path.isEmpty()) {
        for (auto& identifier : identifiers)
            m_nonPersistentChunks.remove(identifier);
        callback();
        return;
    }

    auto filePaths = map(identifiers, [this](auto& identifier) -> String {
        return FileSystem::pathByAppendingComponents(m_path, { identifier });
    });
    m_ioQueue->dispatch([queue = Ref { m_taskQueue }, filePaths = crossThreadCopy(WTFMove(filePaths)), callback = WTFMove(callback)]() mutable {
        for (auto& filePath : filePaths) {
            FileSystem::deleteFile(filePath);

            size_t index = 0;
            String bodyPath;
            do {
                bodyPath = makeString(filePath, '-', index++);
            } while (FileSystem::deleteFile(bodyPath));
        }
        queue->dispatch(WTFMove(callback));
    });
}

void BackgroundFetchStoreManager::storeFetch(const String& identifier, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&& data, CompletionHandler<void(StoreResult)>&& callback)
{
    assertIsCurrent(m_taskQueue.get());
    
    uint64_t expectedSpace;
    bool isSafe = WTF::safeAdd(downloadTotal, uploadTotal, expectedSpace)
        && WTF::safeAdd(expectedSpace, data.size(), expectedSpace);
    if (!isSafe) {
        callback(StoreResult::QuotaError);
        return;
    }

    m_quotaCheckFunction(expectedSpace, [weakThis = WeakPtr { *this }, identifier, downloadTotal, uploadTotal, responseBodyIndexToClear, data = WTFMove(data), callback = WTFMove(callback)](bool result) mutable {
        if (!weakThis) {
            callback(StoreResult::InternalError);
            return;
        }
        if (!result) {
            callback(StoreResult::QuotaError);
            return;
        }
        weakThis->storeFetchAfterQuotaCheck(identifier, downloadTotal, uploadTotal, responseBodyIndexToClear, WTFMove(data), WTFMove(callback));
    });
}

void BackgroundFetchStoreManager::storeFetchAfterQuotaCheck(const String& identifier, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&& data, CompletionHandler<void(StoreResult)>&& callback)
{
    assertIsCurrent(m_taskQueue.get());

    if (m_path.isEmpty()) {
        callback(StoreResult::OK);
        return;
    }

    auto filePath = FileSystem::pathByAppendingComponents(m_path, { identifier });
    m_ioQueue->dispatch([queue = Ref { m_taskQueue }, filePath = WTFMove(filePath).isolatedCopy(), responseBodyIndexToClear, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        // FIXME: Cover the case of partial write.
        auto writtenSize = FileSystem::overwriteEntireFile(filePath, { data.data(), data.size() });
        auto result = static_cast<size_t>(writtenSize) == data.size() ? StoreResult::OK : StoreResult::InternalError;
        if (result == StoreResult::OK && responseBodyIndexToClear)
            FileSystem::deleteFile(makeString(filePath, '-', *responseBodyIndexToClear));
        RELEASE_LOG_ERROR_IF(result == StoreResult::InternalError, ServiceWorker, "BackgroundFetchStoreManager::storeFetch failed writing");
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    });
}

static String createFetchResponseBodyFile(const String& identifier, size_t index)
{
    return makeString(identifier, '-', index);
}

void BackgroundFetchStoreManager::storeFetchResponseBodyChunk(const String& identifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& callback)
{
    assertIsCurrent(m_taskQueue.get());

    if (m_path.isEmpty()) {
        auto& chunks = m_nonPersistentChunks.ensure(identifier, [] {
            return Vector<SharedBufferBuilder>();
        }).iterator->value;

        while (chunks.size() <= index)
            chunks.append({ });

        chunks[index].append(data);

        callback(StoreResult::OK);
        return;
    }

    auto filePath = FileSystem::pathByAppendingComponent(m_path, createFetchResponseBodyFile(identifier, index));
    m_ioQueue->dispatch([queue = Ref { m_taskQueue }, filePath = WTFMove(filePath).isolatedCopy(), data = Ref { data }, callback = WTFMove(callback)]() mutable {
        auto result = StoreResult::InternalError;
        FileSystem::PlatformFileHandle handle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::ReadWrite);
        if (FileSystem::isHandleValid(handle)) {
            // FIXME: Cover the case of partial write.
            auto writtenSize = FileSystem::writeToFile(handle, data->span());
            if (static_cast<size_t>(writtenSize) == data->size())
                result = StoreResult::OK;
            FileSystem::closeFile(handle);
        }

        RELEASE_LOG_ERROR_IF(result == StoreResult::InternalError, ServiceWorker, "BackgroundFetchStoreManager::storeFetchResponseBodyChunk failed writing");
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    });
}

void BackgroundFetchStoreManager::retrieveResponseBody(const String& identifier, size_t index, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& callback)
{
    assertIsCurrent(m_taskQueue.get());

    if (m_path.isEmpty()) {
        auto iterator = m_nonPersistentChunks.find(identifier);
        if (iterator == m_nonPersistentChunks.end() || iterator->value.size() <= index) {
            callback(RefPtr<SharedBuffer> { });
            return;
        }
        callback(iterator->value[index].copy()->makeContiguous());
        return;
    }

    auto filePath = FileSystem::pathByAppendingComponent(m_path, createFetchResponseBodyFile(identifier, index));
    m_ioQueue->dispatch([queue = Ref { m_taskQueue }, filePath = WTFMove(filePath).isolatedCopy(), callback = WTFMove(callback)]() mutable {
        RefPtr<SharedBuffer> buffer;
        auto fileSize = FileSystem::fileSize(filePath);
        if (fileSize)
            buffer = SharedBuffer::createWithContentsOfFile(filePath, FileSystem::MappedFileMode::Private, shouldUseFileMapping(*fileSize) ? SharedBuffer::MayUseFileMapping::Yes : SharedBuffer::MayUseFileMapping::No);

        queue->dispatch([buffer = WTFMove(buffer), callback = WTFMove(callback)]() mutable {
            callback(WTFMove(buffer));
        });
    });
}

} // namespace WebKit
